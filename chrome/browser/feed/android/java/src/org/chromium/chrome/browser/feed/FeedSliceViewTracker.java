// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.graphics.Rect;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;

/**
 * Tracks position of slice views. When a slice's view is first 2/3rds visible in the viewport,
 * the observer is notified.
 */
public class FeedSliceViewTracker implements ViewTreeObserver.OnPreDrawListener {
    private static final String TAG = "FeedSliceViewTracker";
    private static final float DEFAULT_VIEW_LOG_THRESHOLD = .66f;

    private class VisibilityObserver {
        final float mVisibilityThreshold;
        final Runnable mCallback;

        VisibilityObserver(float visibilityThreshold, Runnable callback) {
            mVisibilityThreshold = visibilityThreshold;
            mCallback = callback;
        }
    }

    @Nullable
    private RecyclerView mRootView;
    @Nullable
    private NtpListContentManager mContentManager;
    // The set of content keys already reported as visible.
    private HashSet<String> mContentKeysVisible = new HashSet<String>();
    private boolean mFeedContentVisible;
    @Nullable
    private Observer mObserver;
    // Map from content key to a list of watchers that will get notified for the first-time visible
    // changes. Each item in the waicther list consists of the view threshold percentage and the
    // callback.
    private HashMap<String, ArrayList<VisibilityObserver>> mWatchedSliceMap = new HashMap<>();

    /** Notified the first time slices are visible */
    public interface Observer {
        // Invoked the first time a slice is 66% visible.
        void sliceVisible(String sliceId);
        // Invoked when feed content is first visible. This can happens as soon as an xsurface view
        // is partially visible.
        void feedContentVisible();
    }

    public FeedSliceViewTracker(@NonNull RecyclerView rootView,
            @NonNull NtpListContentManager contentManager, @NonNull Observer observer) {
        mRootView = rootView;
        mContentManager = contentManager;
        mObserver = observer;
    }

    /** Attaches the tracker to the root view. */
    public void bind() {
        mRootView.getViewTreeObserver().addOnPreDrawListener(this);
    }

    /** Detaches the tracker from the view. */
    public void unbind() {
        if (mRootView != null && mRootView.getViewTreeObserver().isAlive()) {
            mRootView.getViewTreeObserver().removeOnPreDrawListener(this);
        }
    }

    /** Stop observing rootView. Prevents further calls to observer. */
    public void destroy() {
        unbind();
        mRootView = null;
        mObserver = null;
        mContentManager = null;
        mWatchedSliceMap = null;
    }

    /**
     * Clear tracking so that slices already seen can be reported as viewed again.
     */
    public void clear() {
        mContentKeysVisible.clear();
        mFeedContentVisible = false;
        mWatchedSliceMap.clear();
    }

    /**
     * Watches a slice view to get notified when the first time it has the visible area on screen
     * not less than the given threshold.
     * @param contentKey The content key of the view to watch for.
     * @param viewedThreshold The threshold of the percentage of the visible area on screen.
     * @param callback The callback to get notified.
     */
    public void watchForFirstVisible(String contentKey, float viewedThreshold, Runnable callback) {
        ArrayList<VisibilityObserver> watchers = mWatchedSliceMap.get(contentKey);
        if (watchers == null) {
            watchers = new ArrayList<>();
            mWatchedSliceMap.put(contentKey, watchers);
        }
        watchers.add(new VisibilityObserver(viewedThreshold, callback));
    }

    /**
     * Stops watching a slice view for first-time visible.
     * @param contentKey The content key of the view to stop watching for.
     * @param callback The callback to stop from getting the notification.
     */
    public void stopWatchingForFirstVisible(String contentKey, Runnable callback) {
        ArrayList<VisibilityObserver> watchers = mWatchedSliceMap.get(contentKey);
        if (watchers == null) {
            return;
        }
        for (int i = 0; i < watchers.size(); ++i) {
            if (watchers.get(i).mCallback == callback) {
                watchers.remove(i);
                break;
            }
        }
        if (watchers.isEmpty()) {
            mWatchedSliceMap.remove(contentKey);
        }
    }

    // ViewTreeObserver.OnPreDrawListener.
    @Override
    public boolean onPreDraw() {
        // Not sure why, but this method can be called just after destroy().
        if (mRootView == null) return true;
        if (!(mRootView.getLayoutManager() instanceof LinearLayoutManager)) return true;

        LinearLayoutManager layoutManager = (LinearLayoutManager) mRootView.getLayoutManager();
        int firstPosition = layoutManager.findFirstVisibleItemPosition();
        int lastPosition = layoutManager.findLastVisibleItemPosition();
        for (int i = firstPosition;
                i <= lastPosition && i < mContentManager.getItemCount() && i >= 0; ++i) {
            String contentKey = mContentManager.getContent(i).getKey();
            // Feed content slices come with a 'c/' prefix. Ignore everything else.
            if (!contentKey.startsWith("c/")) continue;
            View childView = layoutManager.findViewByPosition(i);
            if (childView == null) continue;

            if (!mFeedContentVisible) {
                mFeedContentVisible = true;
                mObserver.feedContentVisible();
            }

            ArrayList<VisibilityObserver> watchers = mWatchedSliceMap.get(contentKey);
            if (watchers != null) {
                ArrayList<Integer> indexesToRemove = new ArrayList<>();
                ArrayList<Runnable> callbacksToInvoke = new ArrayList<>();
                for (int j = 0; j < watchers.size(); ++j) {
                    VisibilityObserver observer = watchers.get(j);
                    if (isViewVisible(childView, observer.mVisibilityThreshold)) {
                        callbacksToInvoke.add(observer.mCallback);
                        indexesToRemove.add(j);
                    }
                }
                // Remove the indexes before invoking the callbacks in case that some callback may
                // call stopWatchingForFirstVisible.
                for (int j = indexesToRemove.size() - 1; j >= 0; --j) {
                    // Pass int, instead of Integer, to remove the specified index from the list.
                    watchers.remove(indexesToRemove.get(j).intValue());
                }
                if (watchers.isEmpty()) {
                    mWatchedSliceMap.remove(contentKey);
                }
                for (Runnable callback : callbacksToInvoke) {
                    callback.run();
                }
            }

            if (mContentKeysVisible.contains(contentKey)
                    || !isViewVisible(childView, DEFAULT_VIEW_LOG_THRESHOLD)) {
                continue;
            }

            mContentKeysVisible.add(contentKey);
            mObserver.sliceVisible(contentKey);
        }
        return true;
    }

    @VisibleForTesting
    boolean isViewVisible(View childView, float threshold) {
        Rect rect = new Rect(0, 0, childView.getWidth(), childView.getHeight());
        int viewArea = rect.width() * rect.height();
        if (viewArea <= 0) return false;
        if (!mRootView.getChildVisibleRect(childView, rect, null)) return false;
        int visibleArea = rect.width() * rect.height();
        return (float) visibleArea / viewArea >= threshold;
    }
}
