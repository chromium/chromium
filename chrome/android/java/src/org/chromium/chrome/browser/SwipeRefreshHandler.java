// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.SuppressLint;
import android.content.Context;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationCoordinator;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabWebContentsUserData;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.third_party.android.swiperefresh.SwipeRefreshLayout;
import org.chromium.ui.OverscrollAction;
import org.chromium.ui.OverscrollRefreshHandler;
import org.chromium.ui.base.WindowAndroid;

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

    private @OverscrollAction int mSwipeType;

    // The modified AppCompat version of the refresh effect, handling all core
    // logic, rendering and animation.
    private SwipeRefreshLayout mSwipeRefreshLayout;

    // The Tab where the swipe occurs.
    private Tab mTab;

    private EmptyTabObserver mTabObserver;

    // The container view the SwipeRefreshHandler instance is currently
    // associated with.
    private ViewGroup mContainerView;

    // Async runnable for ending the refresh animation after the page first
    // loads a frame. This is used to provide a reasonable minimum animation time.
    private Runnable mStopRefreshingRunnable;

    // Handles removing the layout from the view hierarchy.  This is posted to ensure it does not
    // conflict with pending Android draws.
    private Runnable mDetachRefreshLayoutRunnable;

    // Accessibility utterance used to indicate refresh activation.
    private String mAccessibilityRefreshString;

    // Handles overscroll history navigation. Gesture events from native layer are forwarded
    // to this object. Remains null while navigation feature is disabled due to feature flag,
    // system settings (Q and forward), etc.
    private HistoryNavigationCoordinator mNavigationCoordinator;

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
        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
                if (window == null && mSwipeRefreshLayout != null) {
                    cancelStopRefreshingRunnable();
                    detachSwipeRefreshLayoutIfNecessary();
                    mSwipeRefreshLayout.setOnRefreshListener(null);
                    mSwipeRefreshLayout.setOnResetListener(null);
                    mSwipeRefreshLayout = null;
                }
            }
        };
        mTab.addObserver(mTabObserver);
    }

    private void initSwipeRefreshLayout(final Context context) {
        mSwipeRefreshLayout = new SwipeRefreshLayout(context);
        mSwipeRefreshLayout.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        final boolean incognito = mTab.isIncognito();
        final @ColorInt int incognitoColor = ChromeFeatureList.sBaselineGm3SurfaceColors.isEnabled()
                ? context.getColor(R.color.default_bg_color_dark_elev_2_gm3_baseline)
                : context.getColor(R.color.default_bg_color_dark_elev_2_baseline);
        final @ColorInt int backgroundColor = incognito
                ? incognitoColor
                : ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_2);
        mSwipeRefreshLayout.setProgressBackgroundColorSchemeColor(backgroundColor);
        final @ColorInt int iconColor = incognito
                ? context.getColor(R.color.default_icon_color_blue_light)
                : SemanticColorUtils.getDefaultIconColorAccent1(context);
        mSwipeRefreshLayout.setColorSchemeColors(iconColor);
        if (mContainerView != null) mSwipeRefreshLayout.setEnabled(true);

        mSwipeRefreshLayout.setOnRefreshListener(() -> {
            cancelStopRefreshingRunnable();
            PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, getStopRefreshingRunnable(),
                    MAX_REFRESH_ANIMATION_DURATION_MS);
            if (mAccessibilityRefreshString == null) {
                int resId = R.string.accessibility_swipe_refresh;
                mAccessibilityRefreshString = context.getResources().getString(resId);
            }
            mSwipeRefreshLayout.announceForAccessibility(mAccessibilityRefreshString);
            mTab.reload();
            RecordUserAction.record("MobilePullGestureReload");
        });
        mSwipeRefreshLayout.setOnResetListener(() -> {
            if (mDetachRefreshLayoutRunnable != null) return;
            mDetachRefreshLayoutRunnable = () -> {
                mDetachRefreshLayoutRunnable = null;
                detachSwipeRefreshLayoutIfNecessary();
            };
            PostTask.postTask(TaskTraits.UI_DEFAULT, mDetachRefreshLayoutRunnable);
        });
    }

    @SuppressLint("NewApi")
    @Override
    public void initWebContents(WebContents webContents) {
        webContents.setOverscrollRefreshHandler(this);
        mContainerView = mTab.getContentView();
        setEnabled(true);
    }

    @SuppressLint("NewApi")
    @Override
    public void cleanupWebContents(WebContents webContents) {
        detachSwipeRefreshLayoutIfNecessary();
        mContainerView = null;
        mNavigationCoordinator = null;
        setEnabled(false);
    }

    @Override
    public void destroyInternal() {
        if (mSwipeRefreshLayout != null) {
            mSwipeRefreshLayout.setOnRefreshListener(null);
            mSwipeRefreshLayout.setOnResetListener(null);
        }
    }

    /**
     * Notify the SwipeRefreshLayout that a refresh action has completed.
     * Defer the notification by a reasonable minimum to ensure sufficient
     * visiblity of the animation.
     */
    public void didStopRefreshing() {
        if (mSwipeRefreshLayout == null || !mSwipeRefreshLayout.isRefreshing()) return;
        cancelStopRefreshingRunnable();
        mSwipeRefreshLayout.postDelayed(
                getStopRefreshingRunnable(), STOP_REFRESH_ANIMATION_DELAY_MS);
    }

    @Override
    public boolean start(
            @OverscrollAction int type, float startX, float startY, boolean navigateForward) {
        mSwipeType = type;
        if (type == OverscrollAction.PULL_TO_REFRESH) {
            if (mSwipeRefreshLayout == null) initSwipeRefreshLayout(mTab.getContext());
            attachSwipeRefreshLayoutIfNecessary();
            return mSwipeRefreshLayout.start();
        } else if (type == OverscrollAction.HISTORY_NAVIGATION) {
            if (mNavigationCoordinator != null) {
                mNavigationCoordinator.startGesture();
                boolean navigable =
                        mNavigationCoordinator.triggerUi(navigateForward, startX, startY);
                boolean showGlow = navigateForward && !mTab.canGoForward();
                return showGlow || navigable;
            }
        }
        mSwipeType = OverscrollAction.NONE;
        return false;
    }

    /**
     * Sets {@link HistoryNavigationCoordinator} object.
     * @param layout {@link HistoryNavigationCoordinator} object.
     */
    public void setNavigationCoordinator(HistoryNavigationCoordinator navigationHandler) {
        mNavigationCoordinator = navigationHandler;
    }

    @Override
    public void pull(float xDelta, float yDelta) {
        TraceEvent.begin("SwipeRefreshHandler.pull");
        if (mSwipeType == OverscrollAction.PULL_TO_REFRESH) {
            mSwipeRefreshLayout.pull(yDelta);
        } else if (mSwipeType == OverscrollAction.HISTORY_NAVIGATION) {
            if (mNavigationCoordinator != null) mNavigationCoordinator.pull(xDelta, yDelta);
        }
        TraceEvent.end("SwipeRefreshHandler.pull");
    }

    @Override
    public void release(boolean allowRefresh) {
        TraceEvent.begin("SwipeRefreshHandler.release");
        if (mSwipeType == OverscrollAction.PULL_TO_REFRESH) {
            mSwipeRefreshLayout.release(allowRefresh);
        } else if (mSwipeType == OverscrollAction.HISTORY_NAVIGATION) {
            if (mNavigationCoordinator != null) mNavigationCoordinator.release(allowRefresh);
        }
        TraceEvent.end("SwipeRefreshHandler.release");
    }

    @Override
    public void reset() {
        cancelStopRefreshingRunnable();
        if (mSwipeRefreshLayout != null) mSwipeRefreshLayout.reset();
        if (mNavigationCoordinator != null) mNavigationCoordinator.reset();
    }

    @Override
    public void setEnabled(boolean enabled) {
        if (!enabled) reset();
    }

    private void cancelStopRefreshingRunnable() {
        if (mStopRefreshingRunnable != null) {
            ThreadUtils.getUiThreadHandler().removeCallbacks(mStopRefreshingRunnable);
        }
    }

    private void cancelDetachLayoutRunnable() {
        if (mDetachRefreshLayoutRunnable != null) {
            ThreadUtils.getUiThreadHandler().removeCallbacks(mDetachRefreshLayoutRunnable);
            mDetachRefreshLayoutRunnable = null;
        }
    }

    private Runnable getStopRefreshingRunnable() {
        if (mStopRefreshingRunnable == null) {
            mStopRefreshingRunnable = () -> {
                if (mSwipeRefreshLayout != null) {
                    mSwipeRefreshLayout.setRefreshing(false);
                }
            };
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
        if (mSwipeRefreshLayout == null) return;
        cancelDetachLayoutRunnable();
        if (mSwipeRefreshLayout.getParent() != null) {
            mContainerView.removeView(mSwipeRefreshLayout);
        }
    }
}
