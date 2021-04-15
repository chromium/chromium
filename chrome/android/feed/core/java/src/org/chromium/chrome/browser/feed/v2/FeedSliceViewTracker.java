// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import android.graphics.Rect;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.feed.NtpListContentManager;

import java.util.HashSet;

/**
 * Tracks position of slice views. When a slice's view is first 2/3rds visible in the viewport,
 * the observer is notified.
 */
public class FeedSliceViewTracker implements ViewTreeObserver.OnPreDrawListener {
    private static final String TAG = "FeedSliceViewTracker";
    private static final double DEFAULT_VIEW_LOG_THRESHOLD = .66;

    @Nullable
    private RecyclerView mRootView;
    @Nullable
    private NtpListContentManager mContentManager;
    // The set of content keys already reported as visible.
    private HashSet<String> mContentKeysVisible = new HashSet<String>();
    private boolean mFeedContentVisible;
    @Nullable
    private Observer mObserver;

    /** Notified the first time slices are visible */
    public interface Observer {
        // Invoked the first time a slice is 66% visible.
        void sliceVisible(String sliceId);
        // Invoked when feed content is first visible. This can happens as soon as an xsurface view
        // is partially visible.
        void feedContentVisible();
    }

    FeedSliceViewTracker(@NonNull RecyclerView rootView,
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
    }

    /**
     * Clear tracking so that slices already seen can be reported as viewed again.
     */
    public void clear() {
        mContentKeysVisible.clear();
        mFeedContentVisible = false;
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
            if (mContentKeysVisible.contains(contentKey) || !isViewVisible(childView)) {
                continue;
            }

            mContentKeysVisible.add(contentKey);
            mObserver.sliceVisible(contentKey);
        }
        return true;
    }

    @VisibleForTesting
    boolean isViewVisible(View childView) {
        Rect rect = new Rect(0, 0, childView.getWidth(), childView.getHeight());
        int viewArea = rect.width() * rect.height();
        if (viewArea <= 0) return false;
        if (!mRootView.getChildVisibleRect(childView, rect, null)) return false;
        int visibleArea = rect.width() * rect.height();
        return (float) visibleArea / viewArea >= DEFAULT_VIEW_LOG_THRESHOLD;
    }
}
