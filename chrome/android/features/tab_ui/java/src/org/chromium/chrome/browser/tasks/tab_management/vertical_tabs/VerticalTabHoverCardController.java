// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.Context;
import android.content.res.Resources;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.View;
import android.view.ViewStub;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupHoverCardPresenter;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupHoverCardView;
import org.chromium.chrome.browser.tasks.tab_management.TabHoverCardView;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;

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

        /**
         * @return Whether any context menu is currently showing.
         */
        default boolean isContextMenuShowing() {
            return false;
        }

        /**
         * @return Whether the vertical tab strip list is currently scrolling.
         */
        default boolean isScrolling() {
            return false;
        }
    }

    private final VerticalTabRailLayout mContainerView;
    private final TabModelSelector mTabModelSelector;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final TabGroupHoverCardPresenter mTabGroupHoverCardPresenter;
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
                    showOrHideTabGroupHoverCard(groupHeaderTabId, tabGroupId, view, isHovered);
                }

                @Override
                public boolean isContextMenuShowing() {
                    return VerticalTabHoverCardController.this.isContextMenuShowing();
                }

                @Override
                public boolean isScrolling() {
                    return VerticalTabHoverCardController.this.isScrolling();
                }
            };
    private final @Nullable ViewStub mTabHoverCardViewStub;
    private final @Nullable ViewStub mTabGroupHoverCardViewStub;
    private final @Nullable BooleanSupplier mIsContextMenuShowingSupplier;

    private long mLastHoverCardExitTime = INVALID_TIME;
    private int mCurrentHoveredTabId = Tab.INVALID_TAB_ID;
    private int mCurrentHoveredGroupHeaderTabId = Tab.INVALID_TAB_ID;
    private @Nullable View mCurrentHoveredAnchorView;
    private @Nullable Token mCurrentHoveredGroupId;
    private @Nullable TabHoverCardView mTabHoverCardView;
    private @Nullable TabGroupHoverCardView mTabGroupHoverCardView;
    private @Nullable Runnable mPendingHoverCardRunnable;

    /**
     * Constructs a {@link VerticalTabHoverCardController}.
     *
     * @param containerView The vertical tab rail container view.
     * @param tabHoverCardViewStub The view stub for inflating the tab hover card.
     * @param tabGroupHoverCardViewStub The view stub for inflating the tab group hover card.
     * @param tabModelSelector The {@link TabModelSelector} for accessing tabs and selection state.
     * @param tabContentManagerSupplier Supplier of the manager providing tab thumbnail snapshots.
     * @param isContextMenuShowingSupplier Supplier returning whether any context menu is open.
     */
    VerticalTabHoverCardController(
            VerticalTabRailLayout containerView,
            @Nullable ViewStub tabHoverCardViewStub,
            @Nullable ViewStub tabGroupHoverCardViewStub,
            TabModelSelector tabModelSelector,
            Supplier<@Nullable TabContentManager> tabContentManagerSupplier,
            @Nullable BooleanSupplier isContextMenuShowingSupplier) {
        mContainerView = containerView;
        mTabHoverCardViewStub = tabHoverCardViewStub;
        mTabGroupHoverCardViewStub = tabGroupHoverCardViewStub;
        mTabModelSelector = tabModelSelector;
        mIsContextMenuShowingSupplier = isContextMenuShowingSupplier;
        mTabGroupHoverCardPresenter = new TabGroupHoverCardPresenter(tabModelSelector);

        if (mTabHoverCardViewStub != null) {
            mTabHoverCardViewStub.setOnInflateListener(
                    (ViewStub _, View view) -> {
                        mTabHoverCardView = (TabHoverCardView) view;
                        mTabHoverCardView.initialize(mTabModelSelector, tabContentManagerSupplier);
                        mTabHoverCardView.setOnCardHeightChangedCallback(this::repositionHoverCard);
                        mTabHoverCardView.hide();
                    });
        }
        if (mTabGroupHoverCardViewStub != null) {
            mTabGroupHoverCardViewStub.setOnInflateListener(
                    (ViewStub _, View view) ->
                            mTabGroupHoverCardView = (TabGroupHoverCardView) view);
        }
    }

    /** Returns the {@link TabHoverCardListener} instance. */
    TabHoverCardListener getTabHoverCardListener() {
        return mTabHoverCardListener;
    }

    /** Immediately hides any active hover card and cancels any scheduled display. */
    void hideHoverCard() {
        cancelPendingHoverCard();
        if (isHoverCardShowing()) {
            mLastHoverCardExitTime = SystemClock.uptimeMillis();
        }
        if (mTabHoverCardView != null) {
            mTabHoverCardView.hide();
        }
        if (mTabGroupHoverCardView != null) {
            mTabGroupHoverCardView.hide();
        }
        View anchorView = mCurrentHoveredAnchorView;
        mCurrentHoveredAnchorView = null;
        mCurrentHoveredTabId = Tab.INVALID_TAB_ID;
        mCurrentHoveredGroupHeaderTabId = Tab.INVALID_TAB_ID;
        mCurrentHoveredGroupId = null;

        if (anchorView != null) {
            Runnable onHoverExit = (Runnable) anchorView.getTag(R.id.tab_hover_exit_listener);
            if (onHoverExit != null) {
                onHoverExit.run();
            }
        }
    }

    /** Destroys references and cancels pending handlers. */
    void destroy() {
        hideHoverCard();
        mHandler.removeCallbacksAndMessages(null);
        if (mTabHoverCardViewStub != null) {
            mTabHoverCardViewStub.setOnInflateListener(null);
        }
        if (mTabGroupHoverCardViewStub != null) {
            mTabGroupHoverCardViewStub.setOnInflateListener(null);
        }
        if (mTabHoverCardView != null) {
            mTabHoverCardView.setOnCardHeightChangedCallback(null);
            mTabHoverCardView.destroy();
            mTabHoverCardView = null;
        }
        if (mTabGroupHoverCardView != null) {
            mTabGroupHoverCardView.destroy();
            mTabGroupHoverCardView = null;
        }
        mCurrentHoveredTabId = Tab.INVALID_TAB_ID;
        mCurrentHoveredAnchorView = null;
        mCurrentHoveredGroupHeaderTabId = Tab.INVALID_TAB_ID;
        mCurrentHoveredGroupId = null;
        mLastHoverCardExitTime = INVALID_TIME;
    }

    /**
     * Recalculates and updates the hover card's position when its content or dimensions change
     * asynchronously while visible (e.g. when memory usage information is retrieved and displayed).
     */
    private void repositionHoverCard() {
        if (mTabHoverCardView == null || !mTabHoverCardView.isShown()) return;
        if (mCurrentHoveredAnchorView == null || mCurrentHoveredTabId == Tab.INVALID_TAB_ID) {
            return;
        }
        Tab tab = mTabModelSelector.getTabById(mCurrentHoveredTabId);
        if (tab == null) return;

        float[] position =
                getHoverCardPosition(
                        mCurrentHoveredAnchorView,
                        mContainerView,
                        mTabHoverCardView,
                        tab.getIsPinned(),
                        mContainerView.isCollapsed());
        mTabHoverCardView.setX(position[0]);
        mTabHoverCardView.setY(position[1]);
    }

    /** Handles hover and keyboard focus state changes on vertical tab item views. */
    private void showOrHideTabHoverCard(int tabId, View view, boolean isHovered) {
        if (isHovered) {
            if (isContextMenuShowing() || isScrolling()) {
                return;
            }

            mCurrentHoveredTabId = tabId;
            mCurrentHoveredAnchorView = view;
            mCurrentHoveredGroupHeaderTabId = Tab.INVALID_TAB_ID;
            mCurrentHoveredGroupId = null;

            // Skip showing for the currently selected tab.
            if (mTabModelSelector.getCurrentTabId() == tabId) {
                hideHoverCard();
                return;
            }

            scheduleOrShowHoverCard(view, () -> showHoverCard(tabId, view));
        } else if (mCurrentHoveredTabId == tabId) {
            // Only hide if the exit event belongs to the currently hovered tab. When scrubbing,
            // Android may dispatch HOVER_ENTER on the new tab before HOVER_EXIT on the previous
            // one.
            mCurrentHoveredTabId = Tab.INVALID_TAB_ID;
            mCurrentHoveredAnchorView = null;
            hideHoverCard();
        }
    }

    /** Handles hover state changes on vertical tab group header views. */
    private void showOrHideTabGroupHoverCard(
            int groupHeaderTabId, @Nullable Token tabGroupId, View view, boolean isHovered) {
        if (isHovered) {
            if (isContextMenuShowing() || isScrolling()) {
                return;
            }

            mCurrentHoveredTabId = Tab.INVALID_TAB_ID;
            mCurrentHoveredAnchorView = view;
            mCurrentHoveredGroupHeaderTabId = groupHeaderTabId;
            mCurrentHoveredGroupId = tabGroupId;

            scheduleOrShowHoverCard(
                    view, () -> showGroupHoverCard(groupHeaderTabId, tabGroupId, view));
        } else {
            boolean isMatchingGroup =
                    (tabGroupId != null && tabGroupId.equals(mCurrentHoveredGroupId))
                            || (groupHeaderTabId != Tab.INVALID_TAB_ID
                                    && groupHeaderTabId == mCurrentHoveredGroupHeaderTabId);
            if (isMatchingGroup) {
                mCurrentHoveredGroupHeaderTabId = Tab.INVALID_TAB_ID;
                mCurrentHoveredGroupId = null;
                mCurrentHoveredAnchorView = null;
                hideHoverCard();
            }
        }
    }

    boolean isScrolling() {
        VerticalTabListRecyclerView recyclerView = mContainerView.getRecyclerView();
        return recyclerView != null
                && recyclerView.getScrollState() != RecyclerView.SCROLL_STATE_IDLE;
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

    private void scheduleOrShowHoverCard(View view, Runnable showRunnable) {
        cancelPendingHoverCard();
        if (isContextMenuShowing() || isScrolling()) {
            return;
        }

        if (shouldShowHoverCardImmediately(view)) {
            showRunnable.run();
        } else {
            mPendingHoverCardRunnable = showRunnable;
            mHandler.postDelayed(mPendingHoverCardRunnable, getHoverCardDelay());
        }
    }

    private boolean shouldShowHoverCardImmediately(View view) {
        // Show immediately if the item view has keyboard focus.
        if (view.hasFocus()) return true;
        // Show immediately if a card is already visible while scrubbing across adjacent items.
        if (isHoverCardShowing()) return true;
        // Do not show immediately if no previous hover card has been shown/hidden yet.
        if (mLastHoverCardExitTime == INVALID_TIME) return false;
        // Show immediately if the cursor moved into this item within the 300ms grace window after
        // exiting a previous item.
        long elapsedTime = SystemClock.uptimeMillis() - mLastHoverCardExitTime;
        return elapsedTime <= SHOW_HOVER_CARD_WITHOUT_DELAY_TIME_BUFFER_MS;
    }

    private void showHoverCard(int tabId, View view) {
        if (isContextMenuShowing()) {
            return;
        }
        if (mTabGroupHoverCardView != null) {
            mTabGroupHoverCardView.hide();
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

        // Bind tab content first so child views (title, URL, thumbnail) are populated
        // with the target tab's data before measuring height.
        mTabHoverCardView.bindTab(tab);

        float[] position =
                getHoverCardPosition(
                        view,
                        mContainerView,
                        mTabHoverCardView,
                        tab.getIsPinned(),
                        mContainerView.isCollapsed());
        mTabHoverCardView.show(position[0], position[1]);
    }

    private void showGroupHoverCard(int groupHeaderTabId, @Nullable Token tabGroupId, View view) {
        if (isContextMenuShowing()) {
            return;
        }
        if (mTabHoverCardView != null) {
            mTabHoverCardView.hide();
        }
        if (mTabGroupHoverCardViewStub != null && mTabGroupHoverCardViewStub.getParent() != null) {
            mTabGroupHoverCardViewStub.inflate();
        }
        if (mTabGroupHoverCardView == null) return;

        if (mTabGroupHoverCardView.isShown()) {
            mTabGroupHoverCardView.hide();
        }

        // Bind group content first so child views are populated before measuring height.
        if (!mTabGroupHoverCardPresenter.bindData(
                mTabGroupHoverCardView, groupHeaderTabId, tabGroupId)) {
            return;
        }

        float[] position =
                getHoverCardPosition(
                        view,
                        mContainerView,
                        mTabGroupHoverCardView,
                        /* isPinnedTab= */ false,
                        mContainerView.isCollapsed());
        mTabGroupHoverCardView.show(position[0], position[1]);
    }

    private void cancelPendingHoverCard() {
        if (mPendingHoverCardRunnable != null) {
            mHandler.removeCallbacks(mPendingHoverCardRunnable);
            mPendingHoverCardRunnable = null;
        }
    }

    private boolean isContextMenuShowing() {
        return mIsContextMenuShowingSupplier != null
                && mIsContextMenuShowingSupplier.getAsBoolean();
    }

    boolean isHoverCardShowing() {
        return (mTabHoverCardView != null && mTabHoverCardView.isShown())
                || (mTabGroupHoverCardView != null && mTabGroupHoverCardView.isShown());
    }

    /**
     * Get the x and y coordinates of the position of the hover card, in px.
     *
     * @param anchorView The item view being hovered.
     * @param containerView The vertical tab container / parent view.
     * @param hoverCardView The hover card view instance.
     * @param isPinnedTab True if the hovered item is a pinned tab.
     * @param isRailCollapsed True if the vertical tab rail is currently collapsed.
     * @return A float array specifying the x (array[0]) and y (array[1]) coordinates.
     */
    @VisibleForTesting
    static float[] getHoverCardPosition(
            View anchorView,
            View containerView,
            View hoverCardView,
            boolean isPinnedTab,
            boolean isRailCollapsed) {
        // 1. Calculate relative coordinates of the anchor view and rail container relative to the
        // hover card's parent container.
        View parentView =
                hoverCardView.getParent() instanceof View parent
                        ? parent
                        : containerView.getRootView();
        int[] anchorViewLocation = new int[2];
        int[] containerLocation = new int[2];
        int[] parentLocation = new int[2];
        anchorView.getLocationOnScreen(anchorViewLocation);
        containerView.getLocationOnScreen(containerLocation);
        parentView.getLocationOnScreen(parentLocation);
        float relativeX = anchorViewLocation[0] - parentLocation[0];
        float relativeY = anchorViewLocation[1] - parentLocation[1];
        float containerRelativeX = containerLocation[0] - parentLocation[0];

        // 2. Measure the hover card with exact card width to obtain its height for dynamic content.
        Context context = containerView.getContext();
        Resources resources = context.getResources();
        int cardWidth = TabHoverCardView.getHoverCardWidthPx(context);
        hoverCardView.measure(
                View.MeasureSpec.makeMeasureSpec(cardWidth, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
        float hoverCardHeight = hoverCardView.getMeasuredHeight();

        // 3. Offset for shadow length, background inset, and card margin.
        float cardShadowOffset = resources.getDimension(R.dimen.popup_menu_shadow_length);
        float backgroundInset =
                (!isPinnedTab && !isRailCollapsed)
                        ? resources.getDimension(
                                VerticalTabUtils.isTablet(context)
                                        ? R.dimen.vertical_tab_item_touch_target_inset_tablet
                                        : R.dimen.vertical_tab_item_touch_target_inset)
                        : 0f;
        float hoverCardMarginToRail =
                resources.getDimension(R.dimen.vertical_tab_hover_card_margin_to_rail);
        float parentHeight = parentView.getHeight();
        float visibleCardHeight = hoverCardHeight - 2 * cardShadowOffset;

        // 4. Determine visible hover card position based on pinned and rail state.
        float visibleX;
        float visibleY;
        if (isPinnedTab
                && !isRailCollapsed
                && relativeY + anchorView.getHeight() + visibleCardHeight <= parentHeight) {
            // Show below the pinned tab.
            visibleX = relativeX;
            visibleY = relativeY + anchorView.getHeight();
        } else {
            // Show to the right of the rail container, top-aligned with the tab.
            visibleX = containerRelativeX + containerView.getWidth() + hoverCardMarginToRail;
            visibleY = relativeY + backgroundInset;
        }

        // When space below is limited, align the bottom of the card with the bottom of the window,
        // leaving a margin.
        float maxVisibleBottom = parentHeight - hoverCardMarginToRail;
        if (visibleY + visibleCardHeight > maxVisibleBottom) {
            visibleY = maxVisibleBottom - visibleCardHeight;
        }

        // Convert visible coordinates to View coordinates by subtracting shadow offset.
        visibleY = Math.max(0, visibleY);
        float hoverCardX = visibleX - cardShadowOffset;
        float hoverCardY = visibleY - cardShadowOffset;
        return new float[] {hoverCardX, hoverCardY};
    }
}
