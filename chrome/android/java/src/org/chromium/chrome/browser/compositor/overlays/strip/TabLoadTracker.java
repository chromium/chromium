// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.os.Handler;

/**
 * {@code TabLoadTracker} is used to handle tracking whether or not to visually show if a tab is
 * loading or not.
 */
public class TabLoadTracker {
    private static final long LOAD_FINISHED_VISUAL_DELAY_MS = 100;

    private final Handler mHandler = new Handler();

    // Callback Tracking State
    private final int mId;
    private final TabLoadTrackerCallback mCallback;

    // Internal Loading State
    private boolean mLoading;

    /** The callback object to be notified of when the loading state changes. */
    public interface TabLoadTrackerCallback {
        /**
         * Called when the loading state tracked by this tab should visually change.
         *
         * @param id The id of the Tab.
         */
        void loadStateChanged(int id);
    }

    /**
     * Creates an instance of the {@link TabLoadTracker} class.
     *
     * @param id The id of the tab to track for callback purposes.
     * @param callback The {@link TabLoadTrackerCallback} to notify on loading state changes.
     */
    public TabLoadTracker(int id, TabLoadTrackerCallback callback) {
        mId = id;
        mCallback = callback;
    }

    /** Called when this tab has started loading resources. */
    public void loadingStarted() {
        if (!mLoading) {
            mLoading = true;
            mCallback.loadStateChanged(mId);
        }
        mHandler.removeCallbacks(mLoadFinishedRunnable);
    }

    /** Called when this tab has finished loading resources. */
    public void loadingFinished() {
        if (!mLoading) return;
        mHandler.removeCallbacks(mLoadFinishedRunnable);
        mHandler.postDelayed(mLoadFinishedRunnable, LOAD_FINISHED_VISUAL_DELAY_MS);
    }

    /**
     * @return Whether or not this tab should be visually represented as loading.
     */
    public boolean isLoading() {
        return mLoading;
    }

    private Runnable mLoadFinishedRunnable =
            new Runnable() {
                @Override
                public void run() {
                    mLoading = false;
                    mCallback.loadStateChanged(mId);
                }
            };
}
