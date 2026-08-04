// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabHoverCardView;

import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/** Controller for tab hover card operations in vertical tabs. */
@NullMarked
public class VerticalTabHoverCardController {
    private static final int DEFAULT_HOVER_CARD_DELAY_MS = 300;
    private static final int SHOW_HOVER_CARD_WITHOUT_DELAY_TIME_BUFFER_MS = 300;
    private static final long INVALID_TIME = -1L;

    /** Interface to receive tab hover card events. */
    @FunctionalInterface
    public interface TabHoverCardListener {
        /**
         * Called when a tab item view hover state changes.
         *
         * @param tabId The ID of the hovered tab.
         * @param view The tab item view being hovered.
         * @param isHovered True if the cursor entered hover state, false if it exited.
         */
        void onTabHoverCardStateChanged(int tabId, View view, boolean isHovered);
    }

    private final View mContainerView;
    private final TabModelSelector mTabModelSelector;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final @Nullable ViewStub mTabHoverCardViewStub;
    private final @Nullable BooleanSupplier mIsContextMenuShowingSupplier;

    private long mLastHoverCardExitTime = INVALID_TIME;
    private @Nullable TabHoverCardView mTabHoverCardView;
    private @Nullable Runnable mPendingHoverCardRunnable;

    /**
     * @param containerView The vertical tab rail container view.
     * @param tabHoverCardViewStub The view stub for inflating the hover card.
     * @param tabModelSelector The {@link TabModelSelector} for accessing tabs and selection state.
     * @param tabContentManagerSupplier Supplier for the {@link TabContentManager}.
     * @param isContextMenuShowingSupplier Supplier returning whether any context menu is open.
     */
    VerticalTabHoverCardController(
            View containerView,
            @Nullable ViewStub tabHoverCardViewStub,
            TabModelSelector tabModelSelector,
            Supplier<@Nullable TabContentManager> tabContentManagerSupplier,
            @Nullable BooleanSupplier isContextMenuShowingSupplier) {
        mContainerView = containerView;
        mTabHoverCardViewStub = tabHoverCardViewStub;
        mTabModelSelector = tabModelSelector;
        mIsContextMenuShowingSupplier = isContextMenuShowingSupplier;

        if (mTabHoverCardViewStub != null) {
            mTabHoverCardViewStub.setOnInflateListener(
                    (viewStub, view) -> {
                        mTabHoverCardView = (TabHoverCardView) view;
                        mTabHoverCardView.initialize(mTabModelSelector, tabContentManagerSupplier);
                    });
        }
    }

    /** Returns the {@link TabHoverCardListener} instance. */
    TabHoverCardListener getTabHoverCardListener() {
        return this::showOrHideTabHoverCard;
    }

    /** Immediately hides the hover card and cancels any scheduled display. */
    void hideHoverCard() {
        cancelPendingHoverCard();
        if (mTabHoverCardView != null) {
            if (mTabHoverCardView.isShown()) {
                mLastHoverCardExitTime = SystemClock.uptimeMillis();
            }
            mTabHoverCardView.hide();
        }
    }

    /** Destroys references and cancels pending handlers. */
    void destroy() {
        hideHoverCard();
        mHandler.removeCallbacksAndMessages(null);
        if (mTabHoverCardView != null) {
            mTabHoverCardView.destroy();
            mTabHoverCardView = null;
        }
    }

    /** Handles hover state changes on vertical tab item views. */
    private void showOrHideTabHoverCard(int tabId, View view, boolean isHovered) {
        if (isHovered) {
            cancelPendingHoverCard();
            if (mIsContextMenuShowingSupplier != null
                    && mIsContextMenuShowingSupplier.getAsBoolean()) {
                return;
            }
            // Skip showing for the currently selected tab.
            if (mTabModelSelector.getCurrentTabId() == tabId) return;

            if (shouldShowHoverCardImmediately()) {
                showHoverCard(tabId, view);
            } else {
                mPendingHoverCardRunnable = () -> showHoverCard(tabId, view);
                mHandler.postDelayed(mPendingHoverCardRunnable, DEFAULT_HOVER_CARD_DELAY_MS);
            }
        } else {
            hideHoverCard();
        }
    }

    private boolean shouldShowHoverCardImmediately() {
        if (mLastHoverCardExitTime == INVALID_TIME) {
            return false;
        }
        long elapsedTime = SystemClock.uptimeMillis() - mLastHoverCardExitTime;
        return elapsedTime <= SHOW_HOVER_CARD_WITHOUT_DELAY_TIME_BUFFER_MS;
    }

    private void showHoverCard(int tabId, View view) {
        if (mIsContextMenuShowingSupplier != null && mIsContextMenuShowingSupplier.getAsBoolean()) {
            return;
        }
        if (mTabHoverCardViewStub != null && mTabHoverCardViewStub.getParent() != null) {
            mTabHoverCardViewStub.inflate();
        }
        if (mTabHoverCardView == null) return;

        Tab tab = mTabModelSelector.getTabById(tabId);
        if (tab == null) return;

        float[] position = getHoverCardPosition(view, mContainerView, mTabHoverCardView);
        mTabHoverCardView.show(tab, position[0], position[1]);
    }

    private void cancelPendingHoverCard() {
        if (mPendingHoverCardRunnable != null) {
            mHandler.removeCallbacks(mPendingHoverCardRunnable);
            mPendingHoverCardRunnable = null;
        }
    }

    /**
     * Get the x and y coordinates of the position of the hover card, in px.
     *
     * @param tabView The tab item view being hovered.
     * @param containerView The vertical tab container / parent view.
     * @param hoverCardView The hover card view instance.
     * @return A float array specifying the x (array[0]) and y (array[1]) coordinates.
     */
    @VisibleForTesting
    static float[] getHoverCardPosition(
            View tabView, View containerView, TabHoverCardView hoverCardView) {
        View root = containerView.getRootView();
        int[] tabViewLocation = new int[2];
        int[] rootLocation = new int[2];
        tabView.getLocationOnScreen(tabViewLocation);
        root.getLocationOnScreen(rootLocation);
        float relativeX = tabViewLocation[0] - rootLocation[0];
        float relativeY = tabViewLocation[1] - rootLocation[1];

        Context context = hoverCardView.getContext();
        float hoverCardWidth = context.getResources().getDimension(R.dimen.tab_hover_card_width);
        DisplayMetrics displayMetrics = context.getResources().getDisplayMetrics();
        float windowWidthPx = displayMetrics.widthPixels;
        hoverCardWidth =
                Math.min(
                        hoverCardWidth,
                        TabHoverCardView.HOVER_CARD_MAX_WIDTH_PERCENT * windowWidthPx);

        ViewGroup.LayoutParams layoutParams = hoverCardView.getLayoutParams();
        if (layoutParams != null && hoverCardWidth != layoutParams.width) {
            layoutParams.width = Math.round(hoverCardWidth);
            hoverCardView.setLayoutParams(layoutParams);
        }

        float hoverCardX = relativeX + tabView.getWidth();
        float hoverCardY = relativeY;

        hoverCardView.measure(
                View.MeasureSpec.makeMeasureSpec(
                        Math.round(hoverCardWidth), View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
        float hoverCardHeight = hoverCardView.getMeasuredHeight();

        float parentHeight = root.getHeight();
        if (hoverCardY + hoverCardHeight > parentHeight) {
            hoverCardY = parentHeight - hoverCardHeight;
        }
        if (hoverCardY < 0) {
            hoverCardY = 0;
        }
        return new float[] {hoverCardX, hoverCardY};
    }
}
