// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;
import android.graphics.Rect;
import android.os.SystemClock;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.xsurface.ListLayoutHelper;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;

/**
 * Tracks position of slice views. When a slice's view is first 2/3rds visible in the viewport,
 * the observer is notified.
 */
public class FeedSliceViewTracker implements ViewTreeObserver.OnPreDrawListener {
    private static final float DEFAULT_VIEW_LOG_THRESHOLD = .66f;
    private static final float GOOD_VISITS_EXPOSURE_THRESHOLD = 0.5f;
    private static final float GOOD_VISITS_COVERAGE_THRESHOLD = 0.25f;
    private static final float VISIBLE_CHANGE_LOG_THRESHOLD = 0.05f;

    private static class VisibilityObserver {
        final float mVisibilityThreshold;
        final Runnable mCallback;

        VisibilityObserver(float visibilityThreshold, Runnable callback) {
            mVisibilityThreshold = visibilityThreshold;
            mCallback = callback;
        }
    }

    private final Activity mActivity;
    // Whether to watch a slice view to get notified for user-interaction reliability related
    // UI changes.
    private final boolean mWatchForUserInteractionReliabilityReport;
    @Nullable private RecyclerView mRootView;
    @Nullable private FeedListContentManager mContentManager;
    private ListLayoutHelper mLayoutHelper;
    // The set of content keys already reported as mostly visible (66% threshold), which is used to
    // determine if a slice has been viewed by the user.
    private HashSet<String> mContentKeysMostlyVisible = new HashSet<String>();
    // The set of content keys already reported as barely visible (5% threshold), which is used to
    // determine if a slice has entered the view port.
    private HashSet<String> mContentKeysBarelyVisible = new HashSet<>();
    // The set of content keys for load-more indicators already reported as visible (5% threshold).
    private HashSet<String> mLoadMoreIndicatorContentKeys = new HashSet<>();
    // The set of content keys for load-more indicators already reported as that the user scrolled
    // away from the indicator.
    private HashSet<String> mLoadMoreAwayFromIndicatorContentKeys = new HashSet<>();
    private boolean mFeedContentVisible;
    @Nullable private Observer mObserver;
    // Map from content key to a list of watchers that will get notified for the first-time visible
    // changes. Each item in the waicther list consists of the view threshold percentage and the
    // callback.
    private HashMap<String, ArrayList<VisibilityObserver>> mWatchedSliceMap = new HashMap<>();
    // Timestamp for keeping track of time spent in feed for good visits.
    private long mLastGoodVisibleTime;

    /** Notified the first time slices are visible */
    public interface Observer {
        // Invoked the first time a slice is 66% visible.
        void sliceVisible(String sliceId);

        // Invoked any time at least one slice is X% exposed and all visible content slices cover Y%
        // of the viewport (see Good Visits threshold params).
        void reportContentSliceVisibleTime(long elapsedMs);

        // Invoked when feed content is first visible. This can happens as soon as an xsurface view
        // is partially visible.
        void feedContentVisible();

        // For reporting to feed user interaction reliability log.
        //
        // Called the first time a slice view is 5% visible.
        void reportViewFirstBarelyVisible(View view);

        // Called the first time a slice view is rendered.
        void reportViewFirstRendered(View view);

        // Called the first time a loading indicator for load-more is 5% visible.
        void reportLoadMoreIndicatorVisible();

        // Called the first time the user scrolled away from the loading indicator for load-more.
        void reportLoadMoreUserScrolledAwayFromIndicator();
    }

    public FeedSliceViewTracker(
            @NonNull RecyclerView rootView,
            @NonNull Activity activity,
            @NonNull FeedListContentManager contentManager,
            @Nullable ListLayoutHelper layoutHelper,
            boolean watchForUserInteractionReliabilityReport,
            @NonNull Observer observer) {
        mActivity = activity;
        mRootView = rootView;
        mContentManager = contentManager;
        mLayoutHelper = layoutHelper;
        mWatchForUserInteractionReliabilityReport = watchForUserInteractionReliabilityReport;
        mObserver = observer;
    }

    /** Attaches the tracker to the root view. */
    public void bind() {
        mRootView.getViewTreeObserver().addOnPreDrawListener(this);
        mLastGoodVisibleTime = 0L;
    }

    /** Detaches the tracker from the view. */
    public void unbind() {
        if (mRootView != null && mRootView.getViewTreeObserver().isAlive()) {
            mRootView.getViewTreeObserver().removeOnPreDrawListener(this);
        }
        reportTimeForGoodVisitsIfNeeded();
    }

    /** Stop observing rootView. Prevents further calls to observer. */
    public void destroy() {
        unbind();
        mRootView = null;
        mObserver = null;
        mContentManager = null;
        mWatchedSliceMap = null;
        mLayoutHelper = null;
    }

    /** Clear tracking so that slices already seen can be reported as viewed again. */
    public void clear() {
        mContentKeysMostlyVisible.clear();
        mFeedContentVisible = false;
        if (mWatchedSliceMap != null) {
            mWatchedSliceMap.clear();
        }
        mContentKeysBarelyVisible.clear();
    }

    /**
     * Watches a slice view to get notified when the first time it has the visible area on screen
     * not less than the given threshold.
     * @param contentKey The content key of the view to watch for.
     * @param viewedThreshold The threshold of the percentage of the visible area on screen.
     * @param callback The callback to get notified.
     */
    public void watchForFirstVisible(String contentKey, float viewedThreshold, Runnable callback) {
        if (mWatchedSliceMap == null) { // avoid crbug.com/1416344
            return;
        }
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
        if (mWatchedSliceMap == null) { // avoid crbug.com/1416344
            return;
        }
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
        if (mRootView == null || mLayoutHelper == null) return true;

        int firstPosition = mLayoutHelper.findFirstVisibleItemPosition();
        int lastPosition = mLayoutHelper.findLastVisibleItemPosition();
        boolean countTimeForGoodVisits = false;
        for (int i = firstPosition;
                i <= lastPosition && i < mContentManager.getItemCount() && i >= 0;
                ++i) {
            String contentKey = mContentManager.getContent(i).getKey();
            View childView = mRootView.getLayoutManager().findViewByPosition(i);
            if (childView == null) continue;

            // Loading spinner slices come with a fixed prefix and a different ID after it.
            if (mWatchForUserInteractionReliabilityReport
                    && contentKey.startsWith("load-more-spinner")) {
                if (!mLoadMoreIndicatorContentKeys.contains(contentKey)
                        && isViewVisible(childView, VISIBLE_CHANGE_LOG_THRESHOLD)) {
                    mLoadMoreIndicatorContentKeys.add(contentKey);
                    mObserver.reportLoadMoreIndicatorVisible();
                }
                if (!mLoadMoreAwayFromIndicatorContentKeys.contains(contentKey)
                        && mLoadMoreIndicatorContentKeys.contains(contentKey)
                        && !isViewVisible(childView, VISIBLE_CHANGE_LOG_THRESHOLD)) {
                    mLoadMoreAwayFromIndicatorContentKeys.add(contentKey);
                    mObserver.reportLoadMoreUserScrolledAwayFromIndicator();
                }
            }

            // Feed content slices come with a 'c/' prefix. Ignore everything else.
            if (!contentKey.startsWith("c/")) continue;

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

            countTimeForGoodVisits =
                    countTimeForGoodVisits
                            || isViewVisible(childView, GOOD_VISITS_EXPOSURE_THRESHOLD)
                            || isViewCoveringViewport(childView, GOOD_VISITS_COVERAGE_THRESHOLD);

            if (!mContentKeysMostlyVisible.contains(contentKey)
                    && isViewVisible(childView, DEFAULT_VIEW_LOG_THRESHOLD)) {
                mContentKeysMostlyVisible.add(contentKey);
                mObserver.sliceVisible(contentKey);
            }

            if (mWatchForUserInteractionReliabilityReport
                    && !mContentKeysBarelyVisible.contains(contentKey)
                    && isViewVisible(childView, VISIBLE_CHANGE_LOG_THRESHOLD)) {
                mObserver.reportViewFirstBarelyVisible(childView);
                // There is not a system way to measure the render latency. Here we mimic how
                // Time To First Draw Done is measured, which is done by posting a runnable after
                // onPreDraw.
                Runnable renderedRunnable =
                        () -> {
                            if (mObserver != null) {
                                mObserver.reportViewFirstRendered(childView);
                            }
                        };
                PostTask.postTask(TaskTraits.UI_DEFAULT, renderedRunnable);
                mContentKeysBarelyVisible.add(contentKey);
            }
        }

        reportTimeForGoodVisitsIfNeeded();
        if (countTimeForGoodVisits) {
            mLastGoodVisibleTime = SystemClock.elapsedRealtime();
        }

        return true;
    }

    private void reportTimeForGoodVisitsIfNeeded() {
        // Report elapsed time since we last saw that content was visible enough.
        if (mLastGoodVisibleTime != 0L) {
            mObserver.reportContentSliceVisibleTime(
                    SystemClock.elapsedRealtime() - mLastGoodVisibleTime);
            mLastGoodVisibleTime = 0L;
        }
    }

    @VisibleForTesting
    boolean isViewVisible(View childView, float threshold) {
        int viewArea = getViewArea(childView);
        if (viewArea <= 0) return false;
        return (float) getVisibleArea(childView) / viewArea >= threshold;
    }

    @VisibleForTesting
    boolean isViewCoveringViewport(View childView, float threshold) {
        int viewportArea = getViewportArea();
        if (viewportArea <= 0) return false;
        return (float) getVisibleArea(childView) / viewportArea >= threshold;
    }

    private int getViewArea(View childView) {
        return childView.getWidth() * childView.getHeight();
    }

    private int getViewportArea() {
        Rect viewport = new Rect();
        mActivity.getWindow().getDecorView().getWindowVisibleDisplayFrame(viewport);
        return viewport.width() * viewport.height();
    }

    private int getVisibleArea(View childView) {
        Rect rect = new Rect(0, 0, childView.getWidth(), childView.getHeight());
        if (!mRootView.getChildVisibleRect(childView, rect, null)) return 0;
        return rect.width() * rect.height();
    }
}
