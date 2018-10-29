// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabWebContentsUserData;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.WebContents;
import org.chromium.third_party.android.swiperefresh.SwipeRefreshLayout;
import org.chromium.ui.OverscrollRefreshHandler;

/**
 * An overscroll handler implemented in terms a modified version of the Android
 * compat library's SwipeRefreshLayout effect.
 */
public class SwipeRefreshHandler
        extends TabWebContentsUserData implements OverscrollRefreshHandler {
    private static final Class<SwipeRefreshHandler> USER_DATA_KEY = SwipeRefreshHandler.class;

    // Synthetic delay between the {@link #didStopRefreshing()} signal and the
    // call to stop the refresh animation.
    private static final int STOP_REFRESH_ANIMATION_DELAY_MS = 500;

    // Max allowed duration of the refresh animation after a refresh signal,
    // guarding against cases where the page reload fails or takes too long.
    private static final int MAX_REFRESH_ANIMATION_DURATION_MS = 7500;

    // The modified AppCompat version of the refresh effect, handling all core
    // logic, rendering and animation.
    private final SwipeRefreshLayout mSwipeRefreshLayout;

    // The Tab where the swipe occurs.
    private Tab mTab;

    // The container view the SwipeRefreshHandler instance is currently
    // associated with.
    private ViewGroup mContainerView;

    // Async runnable for ending the refresh animation after the page first
    // loads a frame. This is used to provide a reasonable minimum animation time.
    private Runnable mStopRefreshingRunnable;

    // Handles removing the layout from the view hierarchy.  This is posted to ensure it does not
    // conflict with pending Android draws.
    private Runnable mDetachLayoutRunnable;

    // Accessibility utterance used to indicate refresh activation.
    private String mAccessibilityRefreshString;

    public static SwipeRefreshHandler from(Tab tab) {
        SwipeRefreshHandler handler = get(tab);
        if (handler == null) {
            handler =
                    tab.getUserDataHost().setUserData(USER_DATA_KEY, new SwipeRefreshHandler(tab));
        }
        return handler;
    }

    @Nullable
    public static SwipeRefreshHandler get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    /**
     * Simple constructor to use when creating an OverscrollRefresh instance from code.
     *
     * @param tab The Tab where the swipe occurs.
     */
    private SwipeRefreshHandler(Tab tab) {
        super(tab);
        mTab = tab;

        final Context context = tab.getThemedApplicationContext();
        mSwipeRefreshLayout = new SwipeRefreshLayout(context);
        mSwipeRefreshLayout.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mSwipeRefreshLayout.setColorSchemeResources(R.color.light_active_color);
        // SwipeRefreshLayout.LARGE layouts appear broken on JellyBean.
        mSwipeRefreshLayout.setSize(SwipeRefreshLayout.DEFAULT);
        mSwipeRefreshLayout.setEnabled(false);

        mSwipeRefreshLayout.setOnRefreshListener(() -> {
            cancelStopRefreshingRunnable();
            mSwipeRefreshLayout.postDelayed(
                    getStopRefreshingRunnable(), MAX_REFRESH_ANIMATION_DURATION_MS);
            if (mAccessibilityRefreshString == null) {
                int resId = R.string.accessibility_swipe_refresh;
                mAccessibilityRefreshString = context.getResources().getString(resId);
            }
            mSwipeRefreshLayout.announceForAccessibility(mAccessibilityRefreshString);
            mTab.reload();
            RecordUserAction.record("MobilePullGestureReload");
        });
        mSwipeRefreshLayout.setOnResetListener(() -> {
            if (mDetachLayoutRunnable != null) return;
            mDetachLayoutRunnable = () -> {
                mDetachLayoutRunnable = null;
                detachSwipeRefreshLayoutIfNecessary();
            };
            mSwipeRefreshLayout.post(mDetachLayoutRunnable);
        });
    }

    @Override
    public void initWebContents(WebContents webContents) {
        webContents.setOverscrollRefreshHandler(this);
        mContainerView = mTab.getContentView();
        setEnabled(true);
    }

    @Override
    public void cleanupWebContents(WebContents webContents) {
        detachSwipeRefreshLayoutIfNecessary();
        cancelStopRefreshingRunnable();
        mContainerView = null;
        setEnabled(false);
    }

    @Override
    public void destroyInternal() {
        mSwipeRefreshLayout.setOnRefreshListener(null);
        mSwipeRefreshLayout.setOnResetListener(null);
    }

    /**
     * Notify the SwipeRefreshLayout that a refresh action has completed.
     * Defer the notification by a reasonable minimum to ensure sufficient
     * visiblity of the animation.
     */
    public void didStopRefreshing() {
        if (!mSwipeRefreshLayout.isRefreshing()) return;
        cancelStopRefreshingRunnable();
        mSwipeRefreshLayout.postDelayed(
                getStopRefreshingRunnable(), STOP_REFRESH_ANIMATION_DELAY_MS);
    }

    @Override
    public boolean start() {
        if (mTab.getActivity() != null && mTab.getActivity().getBottomSheet() != null) {
            Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedProfile());
            tracker.notifyEvent(EventConstants.PULL_TO_REFRESH);
        }

        attachSwipeRefreshLayoutIfNecessary();
        return mSwipeRefreshLayout.start();
    }

    @Override
    public void pull(float delta) {
        TraceEvent.begin("SwipeRefreshHandler.pull");
        mSwipeRefreshLayout.pull(delta);
        TraceEvent.end("SwipeRefreshHandler.pull");
    }

    @Override
    public void release(boolean allowRefresh) {
        TraceEvent.begin("SwipeRefreshHandler.release");
        mSwipeRefreshLayout.release(allowRefresh);
        TraceEvent.end("SwipeRefreshHandler.release");
    }

    @Override
    public void reset() {
        cancelStopRefreshingRunnable();
        mSwipeRefreshLayout.reset();
    }

    @Override
    public void setEnabled(boolean enabled) {
        mSwipeRefreshLayout.setEnabled(enabled);
        if (!enabled) reset();
    }

    private void cancelStopRefreshingRunnable() {
        if (mStopRefreshingRunnable != null) {
            mSwipeRefreshLayout.removeCallbacks(mStopRefreshingRunnable);
        }
    }

    private void cancelDetachLayoutRunnable() {
        if (mDetachLayoutRunnable != null) {
            mSwipeRefreshLayout.removeCallbacks(mDetachLayoutRunnable);
            mDetachLayoutRunnable = null;
        }
    }

    private Runnable getStopRefreshingRunnable() {
        if (mStopRefreshingRunnable == null) {
            mStopRefreshingRunnable = () -> mSwipeRefreshLayout.setRefreshing(false);
        }
        return mStopRefreshingRunnable;
    }

    // The animation view is attached/detached on-demand to minimize overlap
    // with composited SurfaceView content.
    private void attachSwipeRefreshLayoutIfNecessary() {
        cancelDetachLayoutRunnable();
        if (mSwipeRefreshLayout.getParent() == null) {
            mContainerView.addView(mSwipeRefreshLayout);
        }
    }

    private void detachSwipeRefreshLayoutIfNecessary() {
        cancelDetachLayoutRunnable();
        if (mSwipeRefreshLayout.getParent() != null) {
            mContainerView.removeView(mSwipeRefreshLayout);
        }
    }
}
