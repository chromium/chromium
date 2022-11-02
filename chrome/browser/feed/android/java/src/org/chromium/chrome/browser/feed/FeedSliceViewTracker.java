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

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.xsurface.ListLayoutHelper;

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
    private static final float GOOD_VISITS_EXPOSURE_THRESHOLD = 0.5f;
    private static final float GOOD_VISITS_COVERAGE_THRESHOLD = 0.25f;

    private class VisibilityObserver {
        final float mVisibilityThreshold;
        final Runnable mCallback;

        VisibilityObserver(float visibilityThreshold, Runnable callback) {
            mVisibilityThreshold = visibilityThreshold;
            mCallback = callback;
        }
    }

    private final Activity mActivity;
    @Nullable
    private RecyclerView mRootView;
    @Nullable
    private NtpListContentManager mContentManager;
    private ListLayoutHelper mLayoutHelper;
    // The set of content keys already reported as visible.
    private HashSet<String> mContentKeysVisible = new HashSet<String>();
    private boolean mFeedContentVisible;
    @Nullable
    private Observer mObserver;
    // Map from content key to a list of watchers that will get notified for the first-time visible
    // changes. Each item in the waicther list consists of the view threshold percentage and the
    // callback.
    private HashMap<String, ArrayList<VisibilityObserver>> mWatchedSliceMap = new HashMap<>();
    private boolean mTrackTimeForGoodVisits;
    // Thresholds for counting a view as visible for calculating time spent in feed for good visits.
    private float mGoodVisitExposureThreshold;
    private float mGoodVisitCoverageThreshold;
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
    }

    public FeedSliceViewTracker(@NonNull RecyclerView rootView, @NonNull Activity activity,
            @NonNull NtpListContentManager contentManager, @Nullable ListLayoutHelper layoutHelper,
            @NonNull Observer observer) {
        mActivity = activity;
        mRootView = rootView;
        mContentManager = contentManager;
        mLayoutHelper = layoutHelper;
        mObserver = observer;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_CLIENT_GOOD_VISITS)) {
            mTrackTimeForGoodVisits = true;
            mGoodVisitExposureThreshold =
                    (float) ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                            ChromeFeatureList.FEED_CLIENT_GOOD_VISITS, "slice_exposure_threshold",
                            GOOD_VISITS_EXPOSURE_THRESHOLD);
            mGoodVisitCoverageThreshold =
                    (float) ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                            ChromeFeatureList.FEED_CLIENT_GOOD_VISITS, "slice_coverage_threshold",
                            GOOD_VISITS_COVERAGE_THRESHOLD);
        }
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
        if (mRootView == null || mLayoutHelper == null) return true;

        int firstPosition = mLayoutHelper.findFirstVisibleItemPosition();
        int lastPosition = mLayoutHelper.findLastVisibleItemPosition();
        boolean countTimeForGoodVisits = false;
        for (int i = firstPosition;
                i <= lastPosition && i < mContentManager.getItemCount() && i >= 0; ++i) {
            String contentKey = mContentManager.getContent(i).getKey();
            // Feed content slices come with a 'c/' prefix. Ignore everything else.
            if (!contentKey.startsWith("c/")) continue;
            View childView = mRootView.getLayoutManager().findViewByPosition(i);
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

            if (mTrackTimeForGoodVisits) {
                countTimeForGoodVisits = countTimeForGoodVisits
                        || isViewVisible(childView, mGoodVisitExposureThreshold)
                        || isViewCoveringViewport(childView, mGoodVisitCoverageThreshold);
            }

            if (mContentKeysVisible.contains(contentKey)
                    || !isViewVisible(childView, DEFAULT_VIEW_LOG_THRESHOLD)) {
                continue;
            }

            mContentKeysVisible.add(contentKey);
            mObserver.sliceVisible(contentKey);
        }

        if (mTrackTimeForGoodVisits) {
            reportTimeForGoodVisitsIfNeeded();
            if (countTimeForGoodVisits) {
                mLastGoodVisibleTime = SystemClock.elapsedRealtime();
            }
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
