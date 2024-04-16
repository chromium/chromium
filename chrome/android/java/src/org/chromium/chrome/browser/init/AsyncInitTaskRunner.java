// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryPrefetcher;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.chrome.browser.ChromeActivitySessionTracker;
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
    private boolean mAllocateChildConnection;

    private FetchSeedTask mFetchSeedTask;

    // Barrier counter to determine when all tasks have completed and are
    // successful. -1 indicates "terminal state".
    private int mNumPendingSuccesses;

    @VisibleForTesting
    boolean shouldFetchVariationsSeedDuringFirstRun() {
        return VersionInfo.isOfficialBuild();
    }

    @VisibleForTesting
    void prefetchLibrary() {
        LibraryPrefetcher.asyncPrefetchLibrariesToMemory();
    }

    private class FetchSeedTask implements Runnable {
        private final String mRestrictMode;
        private final String mMilestone;
        private final String mChannel;

        public FetchSeedTask(String restrictMode) {
            mRestrictMode = restrictMode;
            mMilestone = Integer.toString(VersionInfo.getProductMajorVersion());
            mChannel = getChannelString();
        }

        @Override
        public void run() {
            VariationsSeedFetcher.get().fetchSeed(mRestrictMode, mMilestone, mChannel);
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    new Runnable() {
                        @Override
                        public void run() {
                            tasksPossiblyComplete(null);
                        }
                    });
        }

        private String getChannelString() {
            if (VersionInfo.isCanaryBuild()) {
                return "canary";
            }
            if (VersionInfo.isDevBuild()) {
                return "dev";
            }
            // TODO(crbug.com/40936710): Remove this if block after automotive beta ends.
            if (VersionInfo.isBetaBuild() && BuildInfo.getInstance().isAutomotive) {
                return "stable";
            }
            if (VersionInfo.isBetaBuild()) {
                return "beta";
            }
            if (VersionInfo.isStableBuild()) {
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
            ++mNumPendingSuccesses;

            // Fetching variations restrict mode requires AccountManagerFacade to be initialized.
            ProcessInitializationHandler.getInstance().initializePreNative();

            ChromeActivitySessionTracker sessionTracker =
                    ChromeActivitySessionTracker.getInstance();
            sessionTracker.getVariationsRestrictModeValue(
                    new Callback<String>() {
                        @Override
                        public void onResult(String restrictMode) {
                            mFetchSeedTask = new FetchSeedTask(restrictMode);
                            PostTask.postTask(TaskTraits.USER_BLOCKING, mFetchSeedTask);
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
        ++mNumPendingSuccesses;
        getTaskPerThreadExecutor()
                .execute(
                        () -> {
                            final ProcessInitException libraryLoadException = loadNativeLibrary();
                            ThreadUtils.postOnUiThread(
                                    () -> {
                                        tasksPossiblyComplete(libraryLoadException);
                                    });
                        });
    }

    /**
     * Loads the native library. Can be run on any thread.
     *
     * @return null if loading succeeds, or ProcessInitException if loading fails.
     */
    private ProcessInitException loadNativeLibrary() {
        try {
            LibraryLoader.getInstance().getMediator().ensureInitializedInMainProcess();
            LibraryLoader.getInstance().ensureInitialized();
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
            prefetchLibrary();
        } catch (ProcessInitException e) {
            return e;
        }
        return null;
    }

    private void tasksPossiblyComplete(Exception failureCause) {
        ThreadUtils.assertOnUiThread();

        if (mNumPendingSuccesses < 0) {
            // Terminal state: Have called onSuccess() or onFailure(). Tasks that complete after
            // onFailure() was triggered by a previous failure would arrive here for no-op.
            return;
        }

        if (failureCause == null) {
            // Task succeeded.
            --mNumPendingSuccesses;
            if (mNumPendingSuccesses == 0) {
                // All tasks succeeded: Finish tasks, call onSuccess(), and reach terminal state.
                if (mAllocateChildConnection) {
                    ChildProcessLauncherHelper.warmUpOnAnyThread(
                            ContextUtils.getApplicationContext(), true);
                }
                onSuccess();
                mNumPendingSuccesses = -1;
            }

        } else {
            // Task failed: Call onFailure(), and reach terminal state.
            onFailure(failureCause);
            mNumPendingSuccesses = -1;
        }
    }

    @VisibleForTesting
    protected Executor getTaskPerThreadExecutor() {
        return runnable -> new Thread(runnable).start();
    }

    /** Handle successful completion of the Async initialization tasks. */
    protected abstract void onSuccess();

    /**
     * Handle failed completion of the Async initialization tasks.
     * @param failureCause The Exception from the original failure.
     */
    protected abstract void onFailure(Exception failureCause);
}
