// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser.tab;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;

import com.ark.browser.ui.widget.swiperefresh.SwipeRefreshLayout;
import com.ark.browser.utils.ArkLogger;
import com.zpj.skin.SkinEngine;

import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.OverscrollAction;
import org.chromium.ui.OverscrollRefreshHandler;

/**
 * An overscroll handler implemented in terms a modified version of the Android
 * compat library's SwipeRefreshLayout effect.
 */
public class ArkSwipeRefreshHandler implements OverscrollRefreshHandler {

    private static final String TAG = "SwipeRefreshHandler";

    // Synthetic delay between the {@link #didStopRefreshing()} signal and the
    // call to stop the refresh animation.
    private static final int STOP_REFRESH_ANIMATION_DELAY_MS = 500;

    // Max allowed duration of the refresh animation after a refresh signal,
    // guarding against cases where the page reload fails or takes too long.
    private static final int MAX_REFRESH_ANIMATION_DURATION_MS = 7500;

    private @OverscrollAction int mSwipeType;

    // The modified AppCompat version of the refresh effect, handling all core
    // logic, rendering and animation.
    @NonNull
    private final SwipeRefreshLayout mSwipeRefreshLayout;

    // The Tab where the swipe occurs.
    private ArkTabImpl mTab;

    private EmptyTabObserver mTabObserver;

    // Async runnable for ending the refresh animation after the page first
    // loads a frame. This is used to provide a reasonable minimum animation time.
    private Runnable mStopRefreshingRunnable;

    // Handles removing the layout from the view hierarchy.  This is posted to ensure it does not
    // conflict with pending Android draws.
    private Runnable mDetachRefreshLayoutRunnable;

    // Accessibility utterance used to indicate refresh activation.
    private String mAccessibilityRefreshString;

    private boolean mNavigateForward;

    private Boolean mIncognito;

    public ArkSwipeRefreshHandler(@NonNull SwipeRefreshLayout swipeRefreshLayout) {
        mSwipeRefreshLayout = swipeRefreshLayout;
        mSwipeRefreshLayout.setOnRefreshListener(() -> {
            if (mTab == null) {
                return;
            }
            cancelStopRefreshingRunnable();
            PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, getStopRefreshingRunnable(),
                    MAX_REFRESH_ANIMATION_DURATION_MS);
            if (mAccessibilityRefreshString == null) {
                int resId = R.string.accessibility_swipe_refresh;
                mAccessibilityRefreshString = swipeRefreshLayout.getResources().getString(resId);
            }
            mSwipeRefreshLayout.announceForAccessibility(mAccessibilityRefreshString);
            mTab.reload();
            RecordUserAction.record("MobilePullGestureReload");
        });
        mSwipeRefreshLayout.setOnResetListener(() -> {
            if (mDetachRefreshLayoutRunnable != null) return;
            mDetachRefreshLayoutRunnable = () -> {
                mDetachRefreshLayoutRunnable = null;
                cancelDetachLayoutRunnable();
            };
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, mDetachRefreshLayoutRunnable);
        });
    }

    public ArkTabImpl getTab() {
        return mTab;
    }

    public void setTab(ArkTabImpl tab) {
        if (mTab == tab) {
            return;
        }
        mTab = tab;
    }

    public void destroy() {
        if (mTab != null) {
            mTab = null;
        }
        cancelStopRefreshingRunnable();
        cancelDetachLayoutRunnable();
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
    public boolean start(
            @OverscrollAction int type, float startX, float startY, boolean navigateForward) {
        if (mTab == null) {
            return false;
        }
        mSwipeType = type;
        if (type == OverscrollAction.PULL_TO_REFRESH) {
            initRefreshLayoutStyle(mTab.isIncognito());
            cancelDetachLayoutRunnable();
            return mSwipeRefreshLayout.start();
        } else if (type == OverscrollAction.HISTORY_NAVIGATION) {
            initRefreshLayoutStyle(mTab.isIncognito());
            cancelDetachLayoutRunnable();
            if (!mSwipeRefreshLayout.start()) {
                return false;
            }

            ArkLogger.d(TAG, "HISTORY_NAVIGATION startX=" + startX + " startY=" + startY + " navigateForward=" + navigateForward);

            boolean handle;

            if (navigateForward) {
                handle = mTab.canGoForward2();
            } else {
                handle = mTab.canGoBack2();
            }
            if (handle) {
                mNavigateForward = navigateForward;
                mSwipeType = OverscrollAction.HISTORY_NAVIGATION;
                return true;
            }
        }
        mSwipeType = OverscrollAction.NONE;
        return false;
    }

    @Override
    public void pull(float xDelta, float yDelta) {
        TraceEvent.begin("SwipeRefreshHandler.pull");
        if (mSwipeType == OverscrollAction.PULL_TO_REFRESH) {
            mSwipeRefreshLayout.pull(yDelta);
        } else if (mSwipeType == OverscrollAction.HISTORY_NAVIGATION) {
            // TODO pull xDelta
            if (mNavigateForward) {
                mSwipeRefreshLayout.pullRight(xDelta);
            } else {
                mSwipeRefreshLayout.pullLeft(xDelta);
            }
        }
        TraceEvent.end("SwipeRefreshHandler.pull");
    }

    @Override
    public void release(boolean allowRefresh) {
        TraceEvent.begin("SwipeRefreshHandler.release");
        if (mSwipeType == OverscrollAction.PULL_TO_REFRESH) {
            mSwipeRefreshLayout.release(allowRefresh);
        } else if (mSwipeType == OverscrollAction.HISTORY_NAVIGATION) {
            // TODO release
            if (mSwipeRefreshLayout.canBackOrForward()) {
                if (mNavigateForward) {
                    mTab.goForward2();
                } else {
                    mTab.goBack2();
                }
            }
            mSwipeRefreshLayout.release(allowRefresh);
        }
        TraceEvent.end("SwipeRefreshHandler.release");
    }

    @Override
    public void reset() {
        cancelStopRefreshingRunnable();
        mSwipeRefreshLayout.reset();
        // TODO navigation reset
    }

    @Override
    public void setEnabled(boolean enabled) {
        if (!enabled) reset();
    }

    private void initRefreshLayoutStyle(boolean incognito) {
        if (mIncognito == null || mIncognito != incognito) {
            mIncognito = incognito;
            Context context = mSwipeRefreshLayout.getContext();
            final @ColorInt int backgroundColor = incognito
                    ? context.getResources().getColor(R.color.default_bg_color_dark_elev_2_baseline)
                    : ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_2);
            mSwipeRefreshLayout.setProgressBackgroundColorSchemeColor(backgroundColor);
            final @ColorInt int iconColor = incognito
                    ? context.getResources().getColor(R.color.default_icon_color_blue_light)
                    : SkinEngine.getColor(context, R.attr.colorAccent);
            mSwipeRefreshLayout.setColorSchemeColors(iconColor);
        }
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
                mSwipeRefreshLayout.setRefreshing(false);
            };
        }
        return mStopRefreshingRunnable;
    }

}
