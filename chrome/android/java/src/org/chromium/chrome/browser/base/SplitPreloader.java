// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.content.Context;
import android.content.res.Configuration;
import android.os.SystemClock;

import androidx.annotation.Nullable;
import androidx.collection.SimpleArrayMap;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.language.GlobalAppLocaleController;

/**
 * Handles preloading split Contexts on a background thread. Loading a new isolated split
 * Context can be expensive since the ClassLoader may need to be created. See crbug.com/1150600 for
 * more info.
 */
public class SplitPreloader {
    private final SimpleArrayMap<String, PreloadTask> mPreloadTasks = new SimpleArrayMap<>();
    private final Context mContext;

    /** Interface to run code after preload completion. */
    public interface PreloadHooks {
        /**
         * Runs immediately on the background thread as soon as the split context is available. Note
         * that normally runInUiThread() should be used instead because the context parameter here
         * may have an incorrect ClassLoader due to b/172602571. This method should only be used for
         * optimizations which need to run as soon as possible, and are safe throw away if a
         * different ClassLoader ends up being used.
         */
        default void runImmediatelyInBackgroundThread(Context unsafeClassLoaderContext) {}

        /**
         * Guaranteed to run in the UI thread before {@link SplitPreloader#wait(String)} returns.
         */
        default void runInUiThread(Context context) {}

        /**
         * Called when attempting to actually create the isolated split in the preload. Typical
         * calls to createIsolatedSplitContext end up waiting on the preload to finish, so we can
         * use this to provide a version which doesn't wait, and thus will not deadlock.
         */
        Context createIsolatedSplitContext(String name);
    }

    private class PreloadTask extends AsyncTask<Void> {
        private final String mName;
        private @Nullable PreloadHooks mPreloadHooks;

        public PreloadTask(String name, @Nullable PreloadHooks preloadHooks) {
            mName = name;
            mPreloadHooks = preloadHooks;
        }

        @Override
        protected Void doInBackground() {
            Context context = createSplitContext();
            if (mPreloadHooks != null) {
                mPreloadHooks.runImmediatelyInBackgroundThread(context);
            }
            return null;
        }

        @Override
        protected void onPostExecute(Void result) {
            finish();
        }

        /**
         * Waits for the preload to finish and calls the onComplete function if needed. onComplete
         * is expected to be called before {@link SplitPreloader#wait(String)} returns, so this
         * method is called there since onPostExecute does not run before get() returns.
         */
        public void finish() {
            try {
                get();
            } catch (Exception e) {
                // Ignore exception, not a problem if preload fails.
            }
            if (mPreloadHooks != null) {
                // Recreate the context here to make sure we have the latest version, in case there
                // was a race to update the class loader cache, see b/172602571.
                mPreloadHooks.runInUiThread(createSplitContext());
                mPreloadHooks = null;
            }
        }

        private Context createSplitContext() {
            if (BundleUtils.isIsolatedSplitInstalled(mName)) {
                Context context;
                if (mPreloadHooks != null) {
                    // We don't just use the basic BundleUtils.getIsolatedSplitContext as it waits
                    // for the preloader to finish, causing a deadlock.
                    context = mPreloadHooks.createIsolatedSplitContext(mName);
                } else {
                    context =
                            BundleUtils.createIsolatedSplitContext(
                                    ContextUtils.getApplicationContext(), mName);
                }
                if (GlobalAppLocaleController.getInstance().isOverridden()) {
                    Configuration config =
                            GlobalAppLocaleController.getInstance().getOverrideConfig(context);
                    context = context.createConfigurationContext(config);
                }
                return context;
            }
            return mContext;
        }
    }

    public SplitPreloader(Context context) {
        mContext = context;
    }

    /** Starts preloading a split context on a background thread. */
    public void preload(String name, PreloadHooks preloadHooks) {
        if (!BundleUtils.isIsolatedSplitInstalled(name) && preloadHooks == null) {
            return;
        }

        PreloadTask task = new PreloadTask(name, preloadHooks);
        task.executeWithTaskTraits(TaskTraits.USER_BLOCKING_MAY_BLOCK);
        synchronized (mPreloadTasks) {
            assert !mPreloadTasks.containsKey(name);
            mPreloadTasks.put(name, task);
        }
    }

    /** Waits for the specified split to be finished loading. */
    public void wait(String name) {
        try (TraceEvent te = TraceEvent.scoped("SplitPreloader.wait")) {
            PreloadTask task;
            synchronized (mPreloadTasks) {
                task = mPreloadTasks.remove(name);
            }
            if (task != null) {
                long startTime = SystemClock.uptimeMillis();
                // Make sure the task is finished and onComplete has run.
                task.finish();
                RecordHistogram.recordTimesHistogram(
                        "Android.IsolatedSplits.PreloadWaitTime." + name,
                        SystemClock.uptimeMillis() - startTime);
            }
        }
    }
}
