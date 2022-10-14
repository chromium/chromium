// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser.tab;

import android.annotation.SuppressLint;
import android.content.Context;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import com.ark.browser.ui.widget.swiperefresh.SwipeRefreshLayout;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabWebContentsUserData;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.OverscrollAction;
import org.chromium.ui.OverscrollRefreshHandler;
import org.chromium.ui.base.WindowAndroid;

/**
 * An overscroll handler implemented in terms a modified version of the Android
 * compat library's SwipeRefreshLayout effect.
 */
public class ArkSwipeRefreshHandler
        extends TabWebContentsUserData implements OverscrollRefreshHandler {

    private static final String TAG = "SwipeRefreshHandler";

    private static final Class<ArkSwipeRefreshHandler> USER_DATA_KEY = ArkSwipeRefreshHandler.class;

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
    private final ArkTabImpl mTab;

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

    private boolean mNavigateForward;

    public static ArkSwipeRefreshHandler from(ArkTabImpl tab) {
        ArkSwipeRefreshHandler handler = get(tab);
        if (handler == null) {
            handler =
                    tab.getUserDataHost().setUserData(USER_DATA_KEY, new ArkSwipeRefreshHandler(tab));
        }
        return handler;
    }

    @Nullable
    public static ArkSwipeRefreshHandler get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    /**
     * Simple constructor to use when creating an OverscrollRefresh instance from code.
     *
     * @param tab The Tab where the swipe occurs.
     */
    private ArkSwipeRefreshHandler(ArkTabImpl tab) {
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
        final @ColorInt int backgroundColor = incognito
                ? context.getResources().getColor(R.color.default_bg_color_dark_elev_2_baseline)
                : ChromeColors.getSurfaceColor(context, R.dimen.default_elevation_2);
        mSwipeRefreshLayout.setProgressBackgroundColorSchemeColor(backgroundColor);
        final @ColorInt int iconColor = incognito
                ? context.getResources().getColor(R.color.default_icon_color_blue_light)
                : SemanticColorUtils.getDefaultIconColorAccent1(context);
        mSwipeRefreshLayout.setColorSchemeColors(iconColor);
        if (mContainerView != null) mSwipeRefreshLayout.setEnabled(true);

        mSwipeRefreshLayout.setOnRefreshListener(() -> {
            cancelStopRefreshingRunnable();
            PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, getStopRefreshingRunnable(),
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
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, mDetachRefreshLayoutRunnable);
        });
    }

    private void initNavigation(Context context) {

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

            if (mSwipeRefreshLayout == null) initSwipeRefreshLayout(mTab.getContext());
            attachSwipeRefreshLayoutIfNecessary();
            if (!mSwipeRefreshLayout.start()) {
                return false;
            }

            Log.d(TAG, "HISTORY_NAVIGATION startX=" + startX + " startY=" + startY + " navigateForward=" + navigateForward);

            boolean handle;
            if (navigateForward) {
//                handle = TabListManager.getInstance().getCurrentTabList().canGoForward();
                handle = mTab.getWindowAndroid().getNavigationHandler().canGoForward();
            } else {
//                handle = TabListManager.getInstance().getCurrentTabList().canGoBack();
                handle = mTab.getWindowAndroid().getNavigationHandler().canGoBack();
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
                    mTab.getWindowAndroid().getNavigationHandler().goForward();
//                    TabListManager.getInstance().getCurrentTabList().goForward();
                } else {
                    mTab.getWindowAndroid().getNavigationHandler().goBack();
//                    TabListManager.getInstance().getCurrentTabList().goBack();
                }
            }
            mSwipeRefreshLayout.release(allowRefresh);
        }
        TraceEvent.end("SwipeRefreshHandler.release");
    }

    @Override
    public void reset() {
        cancelStopRefreshingRunnable();
        if (mSwipeRefreshLayout != null) mSwipeRefreshLayout.reset();
        // TODO navigation reset
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
