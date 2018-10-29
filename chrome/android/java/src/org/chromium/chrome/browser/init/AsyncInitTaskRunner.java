// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.ChromeActivitySessionTracker;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher;
import org.chromium.content_public.browser.ChildProcessLauncherHelper;

import java.util.concurrent.Executor;

/**
 * Runs asynchronous startup task that need to be run before the native side is
 * started. Currently it runs two tasks:
 * - Native library loading
 * - Fetching the variations seed on first run
 */
public abstract class AsyncInitTaskRunner {
    private boolean mFetchingVariations;
    private boolean mLibraryLoaded;
    private boolean mAllocateChildConnection;

    private FetchSeedTask mFetchSeedTask;

    @VisibleForTesting
    boolean shouldFetchVariationsSeedDuringFirstRun() {
        return ChromeVersionInfo.isOfficialBuild();
    }

    private class FetchSeedTask extends AsyncTask<Void> {
        private final String mRestrictMode;
        private final String mMilestone;
        private final String mChannel;

        public FetchSeedTask(String restrictMode) {
            mRestrictMode = restrictMode;
            mMilestone = Integer.toString(ChromeVersionInfo.getProductMajorVersion());
            mChannel = getChannelString();
        }

        @Override
        protected Void doInBackground() {
            VariationsSeedFetcher.get().fetchSeed(mRestrictMode, mMilestone, mChannel);
            return null;
        }

        @Override
        protected void onPostExecute(Void result) {
            mFetchingVariations = false;
            tasksPossiblyComplete(true);
        }

        private String getChannelString() {
            if (ChromeVersionInfo.isCanaryBuild()) {
                return "canary";
            }
            if (ChromeVersionInfo.isDevBuild()) {
                return "dev";
            }
            if (ChromeVersionInfo.isBetaBuild()) {
                return "beta";
            }
            if (ChromeVersionInfo.isStableBuild()) {
                return "stable";
            }
            return "";
        }
    }

    /**
     * Starts the background tasks.
     * @param allocateChildConnection Whether a spare child connection should be allocated. Set to
     *                                false if you know that no new renderer is needed.
     * @param fetchVariationSeed Whether to initialize the variations seed, if it hasn't been
     *                          initialized in a previous run.
     */
    public void startBackgroundTasks(boolean allocateChildConnection, boolean fetchVariationSeed) {
        ThreadUtils.assertOnUiThread();
        if (fetchVariationSeed && shouldFetchVariationsSeedDuringFirstRun()) {
            mFetchingVariations = true;

            // Fetching variations restrict mode requires AccountManagerFacade to be initialized.
            ProcessInitializationHandler.getInstance().initializePreNative();

            ChromeActivitySessionTracker sessionTracker =
                    ChromeActivitySessionTracker.getInstance();
            sessionTracker.getVariationsRestrictModeValue(new Callback<String>() {
                @Override
                public void onResult(String restrictMode) {
                    mFetchSeedTask = new FetchSeedTask(restrictMode);
                    mFetchSeedTask.executeOnExecutor(getFetchSeedExecutor());
                }
            });
        }

        // Remember to allocate child connection once library loading completes. We do it after
        // the loading to reduce stress on the OS caused by running library loading in parallel
        // with UI inflation, see AsyncInitializationActivity.setContentViewAndLoadLibrary().
        mAllocateChildConnection = allocateChildConnection;

        // Load the library on a background thread. Using a plain Thread instead of AsyncTask
        // because the latter would be throttled, and this task is on the critical path of the
        // browser initialization.
        getTaskPerThreadExecutor().execute(() -> {
            final boolean libraryLoaded = loadNativeLibrary();
            ThreadUtils.postOnUiThread(() -> {
                mLibraryLoaded = libraryLoaded;
                tasksPossiblyComplete(mLibraryLoaded);
            });
        });
    }

    /**
     * Loads the native library. Can be run on any thread.
     *
     * @return true iff loading succeeded.
     */
    private static boolean loadNativeLibrary() {
        try {
            LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
            // The prefetch is done after the library load for two reasons:
            // - It is easier to know the library location after it has
            // been loaded.
            // - Testing has shown that this gives the best compromise,
            // by avoiding performance regression on any tested
            // device, and providing performance improvement on
            // some. Doing it earlier delays UI inflation and more
            // generally startup on some devices, most likely by
            // competing for IO.
            // For experimental results, see http://crbug.com/460438.
            LibraryLoader.getInstance().asyncPrefetchLibrariesToMemory();
        } catch (ProcessInitException e) {
            return false;
        }
        return true;
    }

    private void tasksPossiblyComplete(boolean result) {
        ThreadUtils.assertOnUiThread();

        if (!result) {
            if (mFetchSeedTask != null) mFetchSeedTask.cancel(true);
            onFailure();
        }

        if (mLibraryLoaded && !mFetchingVariations) {
            if (mAllocateChildConnection) {
                ChildProcessLauncherHelper.warmUp(ContextUtils.getApplicationContext());
            }
            onSuccess();
        }
    }

    @VisibleForTesting
    protected Executor getFetchSeedExecutor() {
        return AsyncTask.THREAD_POOL_EXECUTOR;
    }

    @VisibleForTesting
    protected Executor getTaskPerThreadExecutor() {
        return runnable -> new Thread(runnable).start();
    }

    /**
     * Handle successful completion of the Async initialization tasks.
     */
    protected abstract void onSuccess();

    /**
     * Handle failed completion of the Async initialization tasks.
     */
    protected abstract void onFailure();
}
