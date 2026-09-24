// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.Context;
import android.content.res.Resources;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupHoverCardPresenter;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupHoverCardView;
import org.chromium.chrome.browser.tasks.tab_management.TabHoverCardView;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;

import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/** Controller for tab and tab group hover operations in vertical tabs. */
@NullMarked
public class VerticalTabHoverController {
    private static final int SHOW_HOVER_CARD_WITHOUT_DELAY_TIME_BUFFER_MS = 300;
    private static final long INVALID_TIME = -1L;

    /** Interface to receive tab and tab group hover events. */
    public interface TabHoverListener {
        /**
         * Called when a tab item view hover or keyboard focus state changes.
         *
         * @param tabId The ID of the hovered/focused tab.
         * @param view The tab item view being hovered or focused.
         * @param isHovered True if hover or keyboard focus became active, false if both exited.
         */
        void onTabHoverStateChanged(@TabId int tabId, View view, boolean isHovered);

        /**
         * Called when a tab group header view hover or keyboard focus state changes.
         *
         * @param groupHeaderTabId The tab ID of the group header (or {@link Tab#INVALID_TAB_ID}).
         * @param tabGroupId The stable ID (Token) of the group being hovered/focused.
         * @param view The tab group header view being hovered or focused.
         * @param isHovered True if hover or keyboard focus became active, false if both exited.
         */
        void onTabGroupHoverStateChanged(
                @TabId int groupHeaderTabId,
                @Nullable Token tabGroupId,
                View view,
                boolean isHovered);

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
    private final TabHoverListener mTabHoverListener =
            new TabHoverListener() {
                @Override
                public void onTabHoverStateChanged(@TabId int tabId, View view, boolean isHovered) {
                    handleHoverStateChanged(
                            tabId,
                            /* tabGroupId= */ null,
                            /* isGroupHeader= */ false,
                            view,
                            isHovered);
                }

                @Override
                public void onTabGroupHoverStateChanged(
                        @TabId int groupHeaderTabId,
                        @Nullable Token tabGroupId,
                        View view,
                        boolean isHovered) {
                    handleHoverStateChanged(
                            groupHeaderTabId,
                            tabGroupId,
                            /* isGroupHeader= */ true,
                            view,
                            isHovered);
                }

                @Override
                public boolean isContextMenuShowing() {
                    return VerticalTabHoverController.this.isContextMenuShowing();
                }

                @Override
                public boolean isScrolling() {
                    return VerticalTabHoverController.this.isScrolling();
                }
            };
    private final @Nullable ViewStub mTabHoverCardViewStub;
    private final @Nullable ViewStub mTabGroupHoverCardViewStub;
    private final @Nullable BooleanSupplier mIsContextMenuShowingSupplier;

    private long mLastHoverCardExitTime = INVALID_TIME;
    private @TabId int mCurrentHoveredTabId = Tab.INVALID_TAB_ID;
    private @Nullable View mCurrentHoveredView;
    private @Nullable TabHoverCardView mTabHoverCardView;
    private @Nullable TabGroupHoverCardView mTabGroupHoverCardView;
    private @Nullable Runnable mPendingHoverCardRunnable;

    /**
     * Constructs a {@link VerticalTabHoverController}.
     *
     * @param containerView The vertical tab rail container view.
     * @param tabHoverCardViewStub The view stub for inflating the tab hover card.
     * @param tabGroupHoverCardViewStub The view stub for inflating the tab group hover card.
     * @param tabModelSelector The {@link TabModelSelector} for accessing tabs and selection state.
     * @param tabContentManagerSupplier Supplier of the manager providing tab thumbnail snapshots.
     * @param isContextMenuShowingSupplier Supplier returning whether any context menu is open.
     */
    VerticalTabHoverController(
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

    /** Returns the {@link TabHoverListener} instance. */
    TabHoverListener getTabHoverListener() {
        return mTabHoverListener;
    }

    /** Returns the currently hovered tab or group header view, or null if none. */
    @VisibleForTesting
    @Nullable View getCurrentHoveredView() {
        return mCurrentHoveredView;
    }

    /**
     * Configures mouse hover and keyboard focus listeners for a tab row view and its optional
     * action button, orchestrating hover state transitions and visual updates.
     *
     * @param listener The {@link TabHoverListener} to notify of state changes, or null.
     * @param tabId The ID of the tab item.
     * @param view The root ViewGroup representing the tab row item.
     * @param actionButton The optional action/close button within the row.
     * @param onHoverVisualStateChanged Callback invoked to apply or clear visual hover state.
     */
    public static void setupTabHover(
            @Nullable TabHoverListener listener,
            @TabId int tabId,
            ViewGroup view,
            @Nullable View actionButton,
            Callback<Boolean> onHoverVisualStateChanged) {
        setupItemHover(
                listener,
                tabId,
                /* tabGroupId= */ null,
                /* isGroupHeader= */ false,
                view,
                actionButton,
                onHoverVisualStateChanged);
    }

    /**
     * Configures mouse hover and keyboard focus listeners for a tab group header row view and its
     * optional menu button, orchestrating hover state transitions and visual updates.
     *
     * @param listener The {@link TabHoverListener} to notify of state changes, or null.
     * @param groupHeaderTabId The representative tab ID of the tab group header.
     * @param tabGroupId The {@link Token} ID of the tab group.
     * @param view The root ViewGroup representing the tab group header row item.
     * @param menuButton The optional menu button within the row.
     * @param onHoverVisualStateChanged Callback invoked to apply or clear visual hover state.
     */
    public static void setupTabGroupHeaderHover(
            @Nullable TabHoverListener listener,
            @TabId int groupHeaderTabId,
            @Nullable Token tabGroupId,
            ViewGroup view,
            @Nullable View menuButton,
            Callback<Boolean> onHoverVisualStateChanged) {
        setupItemHover(
                listener,
                groupHeaderTabId,
                tabGroupId,
                /* isGroupHeader= */ true,
                view,
                menuButton,
                onHoverVisualStateChanged);
    }

    private static void setupItemHover(
            @Nullable TabHoverListener listener,
            @TabId int tabId,
            @Nullable Token tabGroupId,
            boolean isGroupHeader,
            ViewGroup view,
            @Nullable View childButton,
            Callback<Boolean> onHoverVisualStateChanged) {
        view.setTag(R.id.tab_hover_state_listener, onHoverVisualStateChanged);
        if (view.isHovered()) {
            onHoverVisualStateChanged.onResult(true);
        }

        view.setOnHoverListener(
                (View _, MotionEvent motionEvent) -> {
                    switch (motionEvent.getAction()) {
                        case MotionEvent.ACTION_HOVER_ENTER:
                        case MotionEvent.ACTION_HOVER_MOVE:
                            onItemHover(
                                    listener,
                                    tabId,
                                    tabGroupId,
                                    isGroupHeader,
                                    view,
                                    /* isHovered= */ true);
                            return true;
                        case MotionEvent.ACTION_HOVER_EXIT:
                            if (isOutsideParentBounds(
                                    view, motionEvent.getX(), motionEvent.getY())) {
                                onItemHover(
                                        listener,
                                        tabId,
                                        tabGroupId,
                                        isGroupHeader,
                                        view,
                                        /* isHovered= */ false);
                            }
                            return true;
                    }
                    return false;
                });

        if (childButton != null) {
            childButton.setOnHoverListener(
                    (v, motionEvent) -> {
                        switch (motionEvent.getAction()) {
                            case MotionEvent.ACTION_HOVER_ENTER:
                            case MotionEvent.ACTION_HOVER_MOVE:
                                v.setHovered(true);
                                onItemHover(
                                        listener,
                                        tabId,
                                        tabGroupId,
                                        isGroupHeader,
                                        view,
                                        /* isHovered= */ true);
                                return true;
                            case MotionEvent.ACTION_HOVER_EXIT:
                                v.setHovered(false);
                                if (isOutsideParentBounds(
                                        view,
                                        v.getLeft() + motionEvent.getX(),
                                        v.getTop() + motionEvent.getY())) {
                                    onItemHover(
                                            listener,
                                            tabId,
                                            tabGroupId,
                                            isGroupHeader,
                                            view,
                                            /* isHovered= */ false);
                                }
                                return true;
                        }
                        return false;
                    });
        }

        view.setOnFocusChangeListener(
                (v, hasFocus) -> {
                    if (listener != null) {
                        notifyHoverListener(
                                listener, tabId, tabGroupId, isGroupHeader, v, hasFocus);
                    }
                });
    }

    private static void onItemHover(
            @Nullable TabHoverListener listener,
            @TabId int tabId,
            @Nullable Token tabGroupId,
            boolean isGroupHeader,
            View view,
            boolean isHovered) {
        if (listener == null) {
            setHoverVisualState(view, isHovered);
            return;
        }
        if (listener.isContextMenuShowing() || listener.isScrolling()) {
            return;
        }
        view.setHovered(isHovered);
        notifyHoverListener(listener, tabId, tabGroupId, isGroupHeader, view, isHovered);
    }

    private static boolean isOutsideParentBounds(View parentView, float x, float y) {
        return x < 0 || x >= parentView.getWidth() || y < 0 || y >= parentView.getHeight();
    }

    private static void notifyHoverListener(
            TabHoverListener listener,
            @TabId int tabId,
            @Nullable Token tabGroupId,
            boolean isGroupHeader,
            View view,
            boolean isHovered) {
        if (isGroupHeader) {
            listener.onTabGroupHoverStateChanged(tabId, tabGroupId, view, isHovered);
        } else {
            listener.onTabHoverStateChanged(tabId, view, isHovered);
        }
    }

    /**
     * Immediately hides any active hover card popups and clears the currently hovered item view and
     * its visual hover state.
     */
    void resetHoverState() {
        hideHoverCard();
        mCurrentHoveredTabId = Tab.INVALID_TAB_ID;
        if (mCurrentHoveredView != null) {
            View previousHoveredView = mCurrentHoveredView;
            mCurrentHoveredView = null;
            setHoverVisualState(previousHoveredView, false);
        }
    }

    /** Immediately hides any active hover card popups and cancels any scheduled display. */
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
    }

    /** Destroys references and cancels pending handlers. */
    void destroy() {
        resetHoverState();
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
        mLastHoverCardExitTime = INVALID_TIME;
    }

    /**
     * Recalculates and updates the hover card's position when its content or dimensions change
     * asynchronously while visible (e.g. when memory usage information is retrieved and displayed).
     */
    private void repositionHoverCard() {
        if (mTabHoverCardView == null || !mTabHoverCardView.isShown()) return;
        if (mCurrentHoveredView == null || mCurrentHoveredTabId == Tab.INVALID_TAB_ID) {
            return;
        }
        Tab tab = mTabModelSelector.getTabById(mCurrentHoveredTabId);
        if (tab == null) return;

        float[] position =
                getHoverCardPosition(
                        mCurrentHoveredView,
                        mContainerView,
                        mTabHoverCardView,
                        tab.getIsPinned(),
                        mContainerView.isCollapsed());
        mTabHoverCardView.setX(position[0]);
        mTabHoverCardView.setY(position[1]);
    }

    /**
     * Handles hover and keyboard focus state changes on vertical tab items and tab group headers.
     */
    private void handleHoverStateChanged(
            @TabId int id,
            @Nullable Token tabGroupId,
            boolean isGroupHeader,
            View view,
            boolean isHovered) {
        if (isContextMenuShowing() || isScrolling()) {
            return;
        }

        if (!isHovered) {
            if (mCurrentHoveredView == view) {
                // Only reset if the exit event belongs to the currently hovered item. When
                // scrubbing, Android may dispatch HOVER_ENTER on the new view before HOVER_EXIT on
                // the previous one.
                resetHoverState();
            } else {
                setHoverVisualState(view, false);
            }
            return;
        }

        boolean isSameView = (mCurrentHoveredView == view);
        if (!isSameView) {
            resetHoverState();
            mCurrentHoveredView = view;
            if (view.isHovered()) {
                setHoverVisualState(view, true);
                if (isGroupHeader
                        && !mContainerView.isCollapsed()
                        && view.findViewById(R.id.menu_button) != null) {
                    RecordUserAction.record("Android.VerticalTabs.GroupHeaderHovered");
                }
            }
        }
        mCurrentHoveredTabId = isGroupHeader ? Tab.INVALID_TAB_ID : id;

        // Skip showing hover card for the currently selected tab.
        if (!isGroupHeader && mTabModelSelector.getCurrentTabId() == id) {
            hideHoverCard();
            return;
        }

        if (!isSameView || (!isHoverCardShowing() && mPendingHoverCardRunnable == null)) {
            scheduleOrShowHoverCard(
                    view,
                    isGroupHeader
                            ? () -> showGroupHoverCard(id, tabGroupId, view)
                            : () -> showHoverCard(id, view));
        }
    }

    private static void setHoverVisualState(View view, boolean isHovered) {
        view.setHovered(isHovered);
        @SuppressWarnings("unchecked")
        Callback<Boolean> callback = (Callback<Boolean>) view.getTag(R.id.tab_hover_state_listener);
        if (callback != null) {
            callback.onResult(isHovered);
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
            mPendingHoverCardRunnable =
                    () -> {
                        mPendingHoverCardRunnable = null;
                        showRunnable.run();
                    };
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

    private void showHoverCard(@TabId int tabId, View view) {
        if (isContextMenuShowing()) {
            return;
        }
        if (mTabHoverCardViewStub != null && mTabHoverCardViewStub.getParent() != null) {
            mTabHoverCardViewStub.inflate();
        }
        if (mTabHoverCardView == null) return;

        Tab tab = mTabModelSelector.getTabById(tabId);
        if (tab == null) return;

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

    private void showGroupHoverCard(
            @TabId int groupHeaderTabId, @Nullable Token tabGroupId, View view) {
        if (isContextMenuShowing()) {
            return;
        }
        if (mTabGroupHoverCardViewStub != null && mTabGroupHoverCardViewStub.getParent() != null) {
            mTabGroupHoverCardViewStub.inflate();
        }
        if (mTabGroupHoverCardView == null) return;

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
