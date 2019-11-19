// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.SuppressLint;
import android.content.Context;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.annotation.Nullable;

import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationDelegate;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationDelegateFactory;
import org.chromium.chrome.browser.gesturenav.NavigationGlowFactory;
import org.chromium.chrome.browser.gesturenav.NavigationHandler;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabWebContentsUserData;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.third_party.android.swiperefresh.SwipeRefreshLayout;
import org.chromium.ui.OverscrollAction;
import org.chromium.ui.OverscrollRefreshHandler;

/**
 * An overscroll handler implemented in terms a modified version of the Android
 * compat library's SwipeRefreshLayout effect.
 */
public class SwipeRefreshHandler extends TabWebContentsUserData
        implements OverscrollRefreshHandler, OnAttachStateChangeListener {
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

    // Overscroll Navigation delegate providing info/object constructor.
    private HistoryNavigationDelegate mNavigationDelegate =
            HistoryNavigationDelegateFactory.DEFAULT;

    // Handles overscroll history navigation.
    private NavigationHandler mNavigationHandler;

    private NavigationHandler.ActionDelegate mActionDelegate;

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
            public void onActivityAttachmentChanged(Tab tab, boolean isAttached) {
                if (!isAttached && mSwipeRefreshLayout != null) {
                    cancelStopRefreshingRunnable();
                    detachSwipeRefreshLayoutIfNecessary();
                    mSwipeRefreshLayout.setOnRefreshListener(null);
                    mSwipeRefreshLayout.setOnResetListener(null);
                    mSwipeRefreshLayout = null;
                }
            }
        };
        mTab.addObserver(mTabObserver);
        mNavigationDelegate = HistoryNavigationDelegateFactory.create(tab);
    }

    private void initSwipeRefreshLayout(final Context context) {
        mSwipeRefreshLayout = new SwipeRefreshLayout(context);
        mSwipeRefreshLayout.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mSwipeRefreshLayout.setProgressBackgroundColorSchemeResource(
                R.color.default_bg_color_elev_2);
        mSwipeRefreshLayout.setColorSchemeResources(R.color.light_active_color);
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

    @SuppressLint("NewApi")
    @Override
    public void initWebContents(WebContents webContents) {
        webContents.setOverscrollRefreshHandler(this);
        mContainerView = mTab.getContentView();
        mContainerView.addOnAttachStateChangeListener(this);
        mNavigationDelegate.setWindowInsetsChangeObserver(
                mContainerView, () -> updateNavigationHandler());
        setEnabled(true);
    }

    @SuppressLint("NewApi")
    @Override
    public void cleanupWebContents(WebContents webContents) {
        if (mSwipeRefreshLayout != null) detachSwipeRefreshLayoutIfNecessary();
        mContainerView.removeOnAttachStateChangeListener(this);
        mNavigationDelegate.setWindowInsetsChangeObserver(mContainerView, null);
        mContainerView = null;
        if (mNavigationHandler != null) {
            mNavigationHandler.destroy();
            mNavigationHandler = null;
            mActionDelegate = null;
        }
        setEnabled(false);
    }

    @Override
    public void onViewAttachedToWindow(View v) {
        updateNavigationHandler();
    }

    @Override
    public void onViewDetachedFromWindow(View v) {}

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
            if (mNavigationHandler != null) {
                boolean navigable = mActionDelegate.canNavigate(navigateForward);
                boolean showGlow = navigateForward && !mTab.canGoForward();
                mNavigationHandler.onDown(); // Simulates the initial onDown event.
                if (navigable) {
                    mNavigationHandler.showArrowWidget(navigateForward);
                } else if (showGlow) {
                    mNavigationHandler.showGlow(startX, startY);
                }
                return navigable || showGlow;
            }
        }
        mSwipeType = OverscrollAction.NONE;
        return false;
    }

    private void updateNavigationHandler() {
        if (mNavigationDelegate.isNavigationEnabled(mContainerView)) {
            if (mNavigationHandler == null) {
                mActionDelegate = mNavigationDelegate.createActionDelegate();
                mNavigationHandler = new NavigationHandler(mContainerView, mTab.getContext(),
                        mNavigationDelegate,
                        NavigationGlowFactory.forRenderedPage(
                                mContainerView, mTab.getWebContents()));
            }
        } else {
            if (mNavigationHandler != null) {
                mNavigationHandler.destroy();
                mNavigationHandler = null;
            }
        }
    }

    @Override
    public void pull(float xDelta, float yDelta) {
        TraceEvent.begin("SwipeRefreshHandler.pull");
        if (mSwipeType == OverscrollAction.PULL_TO_REFRESH) {
            mSwipeRefreshLayout.pull(yDelta);
        } else if (mSwipeType == OverscrollAction.HISTORY_NAVIGATION) {
            if (mNavigationHandler != null) mNavigationHandler.pull(xDelta);
        }
        TraceEvent.end("SwipeRefreshHandler.pull");
    }

    @Override
    public void release(boolean allowRefresh) {
        TraceEvent.begin("SwipeRefreshHandler.release");
        if (mSwipeType == OverscrollAction.PULL_TO_REFRESH) {
            mSwipeRefreshLayout.release(allowRefresh);
        } else if (mSwipeType == OverscrollAction.HISTORY_NAVIGATION) {
            if (mNavigationHandler != null) mNavigationHandler.release(allowRefresh);
        }
        TraceEvent.end("SwipeRefreshHandler.release");
    }

    @Override
    public void reset() {
        cancelStopRefreshingRunnable();
        if (mSwipeRefreshLayout != null) mSwipeRefreshLayout.reset();
        if (mNavigationHandler != null) mNavigationHandler.reset();
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
        cancelDetachLayoutRunnable();
        if (mSwipeRefreshLayout.getParent() != null) {
            mContainerView.removeView(mSwipeRefreshLayout);
        }
    }
}
