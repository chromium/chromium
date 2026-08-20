// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.View;
import android.view.ViewStub;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabHoverCardView;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;

import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/** Controller for tab and tab group hover card operations in vertical tabs. */
@NullMarked
public class VerticalTabHoverCardController {
    private static final int SHOW_HOVER_CARD_WITHOUT_DELAY_TIME_BUFFER_MS = 300;
    private static final long INVALID_TIME = -1L;

    /** Interface to receive tab and tab group hover card events. */
    public interface TabHoverCardListener {
        /**
         * Called when a tab item view hover or keyboard focus state changes.
         *
         * @param tabId The ID of the hovered/focused tab.
         * @param view The tab item view being hovered or focused.
         * @param isHovered True if hover or keyboard focus became active, false if both exited.
         */
        void onTabHoverCardStateChanged(int tabId, View view, boolean isHovered);

        /**
         * Called when a tab group header view hover or keyboard focus state changes.
         *
         * @param groupHeaderTabId The tab ID of the group header (or {@link Tab#INVALID_TAB_ID}).
         * @param tabGroupId The stable ID (Token) of the group being hovered/focused.
         * @param view The tab group header view being hovered or focused.
         * @param isHovered True if hover or keyboard focus became active, false if both exited.
         */
        void onTabGroupHoverCardStateChanged(
                int groupHeaderTabId, @Nullable Token tabGroupId, View view, boolean isHovered);
    }

    private final VerticalTabRailLayout mContainerView;
    private final TabModelSelector mTabModelSelector;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final TabHoverCardListener mTabHoverCardListener =
            new TabHoverCardListener() {
                @Override
                public void onTabHoverCardStateChanged(int tabId, View view, boolean isHovered) {
                    showOrHideTabHoverCard(tabId, view, isHovered);
                }

                @Override
                public void onTabGroupHoverCardStateChanged(
                        int groupHeaderTabId,
                        @Nullable Token tabGroupId,
                        View view,
                        boolean isHovered) {
                    // TODO(crbug.com/509226293): Implement group hover card controller
                    // orchestration.
                }
            };
    private final @Nullable ViewStub mTabHoverCardViewStub;
    private final @Nullable BooleanSupplier mIsContextMenuShowingSupplier;

    private long mLastHoverCardExitTime = INVALID_TIME;
    private int mCurrentHoveredTabId = Tab.INVALID_TAB_ID;
    private @Nullable TabHoverCardView mTabHoverCardView;
    private @Nullable Runnable mPendingHoverCardRunnable;

    /**
     * Constructs a {@link VerticalTabHoverCardController}.
     *
     * @param containerView The vertical tab rail container view.
     * @param tabHoverCardViewStub The view stub for inflating the hover card.
     * @param tabModelSelector The {@link TabModelSelector} for accessing tabs and selection state.
     * @param tabContentManagerSupplier Supplier for the {@link TabContentManager}.
     * @param isContextMenuShowingSupplier Supplier returning whether any context menu is open.
     */
    VerticalTabHoverCardController(
            VerticalTabRailLayout containerView,
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
                    (ViewStub _, View view) -> {
                        mTabHoverCardView = (TabHoverCardView) view;
                        mTabHoverCardView.initialize(mTabModelSelector, tabContentManagerSupplier);
                        mTabHoverCardView.hide();
                    });
        }
    }

    /** Returns the {@link TabHoverCardListener} instance. */
    TabHoverCardListener getTabHoverCardListener() {
        return mTabHoverCardListener;
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
        if (mTabHoverCardViewStub != null) {
            mTabHoverCardViewStub.setOnInflateListener(null);
        }
        if (mTabHoverCardView != null) {
            mTabHoverCardView.destroy();
            mTabHoverCardView = null;
        }
        mCurrentHoveredTabId = Tab.INVALID_TAB_ID;
        mLastHoverCardExitTime = INVALID_TIME;
    }

    /** Handles hover and keyboard focus state changes on vertical tab item views. */
    private void showOrHideTabHoverCard(int tabId, View view, boolean isHovered) {
        if (isHovered) {
            mCurrentHoveredTabId = tabId;
            cancelPendingHoverCard();
            if (mIsContextMenuShowingSupplier != null
                    && mIsContextMenuShowingSupplier.getAsBoolean()) {
                return;
            }
            // Skip showing for the currently selected tab.
            if (mTabModelSelector.getCurrentTabId() == tabId) {
                hideHoverCard();
                return;
            }

            if (shouldShowHoverCardImmediately(view)) {
                showHoverCard(tabId, view);
            } else {
                mPendingHoverCardRunnable = () -> showHoverCard(tabId, view);
                mHandler.postDelayed(mPendingHoverCardRunnable, getHoverCardDelay());
            }
        } else if (mCurrentHoveredTabId == tabId) {
            // Only hide if the exit event belongs to the currently hovered tab. When scrubbing,
            // Android may dispatch HOVER_ENTER on the new tab before HOVER_EXIT on the previous
            // one.
            mCurrentHoveredTabId = Tab.INVALID_TAB_ID;
            hideHoverCard();
        }
    }

    @VisibleForTesting
    int getHoverCardDelay() {
        Context context = mContainerView.getContext();
        float density = context.getResources().getDisplayMetrics().density;
        float railWidthDp = mContainerView.getWidth() / density;
        float minWidthDp = VerticalTabUtils.SIDE_UI_CONTAINER_COLLAPSED_WIDTH_DP;
        float maxWidthDp = VerticalTabUtils.SIDE_UI_CONTAINER_WIDTH_DP;
        return TabHoverCardView.getHoverCardDelay(railWidthDp, minWidthDp, maxWidthDp);
    }

    private boolean shouldShowHoverCardImmediately(View view) {
        // Show immediately if the tab view has keyboard focus.
        if (view.hasFocus()) return true;
        // Show immediately if a card is already visible while scrubbing across adjacent tabs.
        if (mTabHoverCardView != null && mTabHoverCardView.isShown()) return true;
        // Do not show immediately if no previous hover card has been shown/hidden yet.
        if (mLastHoverCardExitTime == INVALID_TIME) return false;
        // Show immediately if the cursor moved into this tab within the 300ms grace window after
        // exiting a previous tab.
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

        if (mTabHoverCardView.isShown()) {
            mTabHoverCardView.hide();
        }

        float[] position =
                getHoverCardPosition(
                        view,
                        mContainerView,
                        mTabHoverCardView,
                        tab.getIsPinned(),
                        mContainerView.isCollapsed());
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
     * @param isPinnedTab True if the hovered tab is a pinned tab.
     * @param isRailCollapsed True if the vertical tab rail is currently collapsed.
     * @return A float array specifying the x (array[0]) and y (array[1]) coordinates.
     */
    @VisibleForTesting
    static float[] getHoverCardPosition(
            View tabView,
            View containerView,
            TabHoverCardView hoverCardView,
            boolean isPinnedTab,
            boolean isRailCollapsed) {
        // 1. Calculate relative coordinates of the tab view and rail container relative to the root
        // view.
        View root = containerView.getRootView();
        int[] tabViewLocation = new int[2];
        int[] containerLocation = new int[2];
        int[] rootLocation = new int[2];
        tabView.getLocationOnScreen(tabViewLocation);
        containerView.getLocationOnScreen(containerLocation);
        root.getLocationOnScreen(rootLocation);
        float relativeX = tabViewLocation[0] - rootLocation[0];
        float relativeY = tabViewLocation[1] - rootLocation[1];
        float containerRelativeX = containerLocation[0] - rootLocation[0];

        // 2. Determine initial hover card position based on pinned and rail state.
        float hoverCardX;
        float hoverCardY;
        if (isPinnedTab && !isRailCollapsed) {
            hoverCardX = relativeX;
            hoverCardY = relativeY + tabView.getHeight();
        } else {
            hoverCardX = containerRelativeX + containerView.getWidth();
            hoverCardY = relativeY;
        }

        // 3. Measure the hover card to obtain its height for dynamic content.
        hoverCardView.measure(
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
        float hoverCardHeight = hoverCardView.getMeasuredHeight();

        // 4. Adjust the vertical position if the hover card extends beyond root view bounds.
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
