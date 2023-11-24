// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.content.Context;
import android.content.res.Configuration;
import android.os.SystemClock;

import androidx.collection.SimpleArrayMap;

import org.chromium.base.BundleUtils;
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
    public interface OnComplete {
        /**
         * Runs immediately on the background thread as soon as the split context is available.
         * Note that normally runInUiThread() should be used instead because the context parameter
         * here may have an incorrect ClassLoader due to b/172602571. This method should only be
         * used for optimizations which need to run as soon as possible, and are safe throw away if
         * a different ClassLoader ends up being used.
         */
        default void runImmediatelyInBackgroundThread(Context unsafeClassLoaderContext) {}

        /**
         * Guaranteed to run in the UI thread before {@link SplitPreloader#wait(String)} returns.
         */
        default void runInUiThread(Context context) {}
    }

    private class PreloadTask extends AsyncTask<Void> {
        private final String mName;
        private OnComplete mOnComplete;

        public PreloadTask(String name, OnComplete onComplete) {
            mName = name;
            mOnComplete = onComplete;
        }

        @Override
        protected Void doInBackground() {
            Context context = createSplitContext();
            if (mOnComplete != null) {
                mOnComplete.runImmediatelyInBackgroundThread(context);
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
            if (mOnComplete != null) {
                // Recreate the context here to make sure we have the latest version, in case there
                // was a race to update the class loader cache, see b/172602571.
                mOnComplete.runInUiThread(createSplitContext());
                mOnComplete = null;
            }
        }

        private Context createSplitContext() {
            if (BundleUtils.isIsolatedSplitInstalled(mName)) {
                Context context = BundleUtils.createIsolatedSplitContext(mContext, mName);
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
    public void preload(String name, OnComplete onComplete) {
        if (!BundleUtils.isIsolatedSplitInstalled(name) && onComplete == null) {
            return;
        }

        PreloadTask task = new PreloadTask(name, onComplete);
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
