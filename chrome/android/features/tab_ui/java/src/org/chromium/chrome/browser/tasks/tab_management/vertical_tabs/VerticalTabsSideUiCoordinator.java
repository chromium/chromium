// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.app.Activity;
import android.content.Context;
import android.transition.ChangeBounds;
import android.transition.Fade;
import android.transition.Transition;
import android.transition.TransitionSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.Px;

import org.chromium.base.MathUtils;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.side_ui.SideUiContainer;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.HeightType;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs.SideUiSize;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest;
import org.chromium.chrome.browser.ui.side_ui.SideUiObserver;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils.WindowWidthBoundary;
import org.chromium.ui.base.ViewUtils;

/**
 * Coordinator that acts as a container for the Vertical Tab List within the Side UI framework. This
 * wraps {@link VerticalTabListCoordinator} to adapt it to the {@link SideUiContainer} interface,
 * separating container-level layout and sizing concerns from the tab list itself.
 */
@NullMarked
public class VerticalTabsSideUiCoordinator implements SideUiContainer, SideUiObserver {
    static final int VIEW_WIDTH_DP = VerticalTabUtils.SIDE_UI_CONTAINER_WIDTH_DP;
    static final int COLLAPSED_WIDTH_DP = VerticalTabUtils.SIDE_UI_CONTAINER_COLLAPSED_WIDTH_DP;

    private final SideUiCoordinator mSideUiCoordinator;
    private final FrameLayout mRootView;
    private final @AnchorSide int mAnchorSide;
    private final VerticalTabListCoordinator mTabListCoordinator;
    private final VerticalTabRailCollapseController mCollapseController;
    private final @Px int mExpandedViewWidth;
    private final @Px int mCollapsedViewWidth;
    private final @Px int mMinManualWidth;
    private final @Px int mMaxManualWidth;
    private final SettableNonNullObservableSupplier<Boolean> mIsVerticalTabsActiveSupplier;
    private final SettableNonNullObservableSupplier<Boolean> mIsAutoHiddenSupplier =
            ObservableSuppliers.createNonNull(false);

    // Whether the vertical tab is set to visible via UI. Remains true even if it is temporarily
    // hidden by other conditions such as narrow window i.e. |mIsAutoHiddenSupplier.get()| is true.
    private boolean mManualVisible;
    // The rail width proposed by an in-progress manual resize drag, or 0 if no drag is in progress.
    private @Px int mLiveResizeWidth;

    public VerticalTabsSideUiCoordinator(
            Activity activity,
            SideUiCoordinator sideUiCoordinator,
            VerticalTabListCoordinator tabListCoordinator,
            SettableNonNullObservableSupplier<Boolean> isVerticalTabsActiveSupplier) {
        mAnchorSide = AnchorSide.LEFT;

        mSideUiCoordinator = sideUiCoordinator;
        mTabListCoordinator = tabListCoordinator;
        mCollapseController = mTabListCoordinator.getCollapseController();
        mIsVerticalTabsActiveSupplier = isVerticalTabsActiveSupplier;
        mSideUiCoordinator.addObserver(this);
        mSideUiCoordinator.registerSideUiContainer(this);

        mRootView = new FrameLayout(activity);
        mRootView.setLayoutParams(
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
        mRootView.addView(mTabListCoordinator.getView());
        mExpandedViewWidth = ViewUtils.dpToPx(activity, VIEW_WIDTH_DP);
        mCollapsedViewWidth = ViewUtils.dpToPx(activity, COLLAPSED_WIDTH_DP);
        mMinManualWidth = ViewUtils.dpToPx(activity, VerticalTabUtils.MIN_EXPANDED_WIDTH_DP);
        mMaxManualWidth = ViewUtils.dpToPx(activity, VerticalTabUtils.MAX_EXPANDED_WIDTH_DP);
        mCollapseController.setRailStateChangeDelegate(
                () ->
                        mSideUiCoordinator.updateUi(
                                new UiUpdateRequest(
                                        getSideUiId(), /* suppressAnimations= */ false)));
    }

    public NonNullObservableSupplier<Boolean> getIsAutoHiddenSupplier() {
        return mIsAutoHiddenSupplier;
    }

    public void setVisible(boolean show, boolean suppressAnimations) {
        mManualVisible = show;
        if (!show) {
            updateAutoHiddenState(false);
        }
        mSideUiCoordinator.updateUi(new UiUpdateRequest(getSideUiId(), suppressAnimations));
        // Fallback: If hiding VT when spec diff is empty (no hide animation scheduled),
        // update active state immediately to avoid dropping the state update.
        SideUiSpecs currentSpecs = mSideUiCoordinator.getCurrentSideUiSpecs();
        if (!show
                && (currentSpecs == null || currentSpecs.getReservedWidth(getAnchorSide()) == 0)) {
            mIsVerticalTabsActiveSupplier.set(false);
        }
    }

    public void destroy() {
        updateAutoHiddenState(false);
        mSideUiCoordinator.removeObserver(this);
        mSideUiCoordinator.unregisterSideUiContainer(this);
        mCollapseController.setRailStateChangeDelegate(null);
        mTabListCoordinator.destroy();
        mIsVerticalTabsActiveSupplier.set(false);
    }

    /** Requests keyboard focus on the Vertical Tabs rail. */
    public void requestKeyboardFocus() {
        mTabListCoordinator.requestKeyboardFocus();
    }

    /** Returns whether the Vertical Tabs rail contains keyboard focus. */
    public boolean containsKeyboardFocus() {
        View view = mTabListCoordinator.getView();
        return view != null && view.hasFocus();
    }

    /**
     * Opens the context menu for the currently keyboard-focused tab or group header in Vertical
     * Tabs.
     *
     * @return Whether the context menu was successfully opened.
     */
    public boolean openKeyboardFocusedContextMenu() {
        if (!containsKeyboardFocus()) {
            return false;
        }
        return mTabListCoordinator.openKeyboardFocusedContextMenu();
    }

    /**
     * Returns whether or not Tab layout toggle menu can be activated. Used to grey out the menu
     * item if it cannot be activated due to conditions such as a narrow app Window width.
     */
    public boolean canActivateTabLayoutToggleMenu() {
        return mSideUiCoordinator.canShowSideUi(SideUiId.VERTICAL_TABS);
    }

    // SideUiContainer implementation:
    @Override
    public View getView() {
        return mRootView;
    }

    @Override
    public @SideUiId int getSideUiId() {
        return SideUiId.VERTICAL_TABS;
    }

    @Override
    public SideUiSize determineShowableSize(
            @Px int availableWidth, @Px int windowWidth, boolean isFullscreen) {
        Context context = mRootView.getContext();
        int availableWidthDp = ViewUtils.pxToDp(context, availableWidth);
        int windowWidthDp = ViewUtils.pxToDp(context, windowWidth);
        @WindowWidthBoundary
        int boundary = VerticalTabUtils.getWindowWidthBoundary(windowWidthDp, availableWidthDp);

        // SideUiCoordinator only dispatches onSideUiSpecsChanged() when there is a non-empty
        // SideUiSpecs diff. If the window is resized while rail width remains unchanged (e.g.
        // staying at collapsed 76dp across the narrow-window threshold), onSideUiSpecsChanged()
        // will be skipped. Feeding the input here makes the controller re-apply the effective
        // state on forced-collapsed transitions.
        // If any future internal state changes while rail width (specs) remains unchanged,
        // synchronize it here or handle the state transition explicitly.
        mCollapseController.setWindowWidthBoundary(boundary);

        @RailCollapseState int effectiveState = mCollapseController.getEffectiveRailCollapseState();
        @Px
        int renderedWidth =
                calculateRenderedWidthPx(boundary, effectiveState, windowWidth, availableWidth);
        // Expanding on hover is a transient preview, so it must not resize or reposition anything
        // outside the rail: the rail keeps reserving its collapsed width and renders the expanded
        // width over the web contents and the toolbar.
        @Px
        int reservedWidth =
                effectiveState == RailCollapseState.EXPANDED_FOR_HOVERING
                        ? mCollapsedViewWidth
                        : renderedWidth;
        boolean shouldHide = boundary == WindowWidthBoundary.NOT_SHOWABLE;

        updateAutoHiddenState(mManualVisible && shouldHide);
        if (isFullscreen || shouldHide) {
            return new SideUiSize(0, HeightType.NOT_APPLICABLE);
        }
        return new SideUiSize(reservedWidth, renderedWidth, HeightType.TOOLBAR);
    }

    private void updateAutoHiddenState(boolean isHiddenDueToNarrowWidth) {
        mIsAutoHiddenSupplier.set(isHiddenDueToNarrowWidth);
    }

    @Override
    public @AnchorSide int getAnchorSide() {
        return mAnchorSide;
    }

    @Override
    public boolean hasContentToShow(Tab tab) {
        return mManualVisible;
    }

    @Override
    public void setRenderedWidth(int renderedWidth) {
        ViewGroup.LayoutParams layoutParams = mRootView.getLayoutParams();
        if (layoutParams != null) {
            layoutParams.width = renderedWidth;
            mRootView.setLayoutParams(layoutParams);
        }
    }

    @Override
    public boolean shouldLockTopControls() {
        return true;
    }

    @Override
    public void onUiUpdateCompleted(
            @Px int oldReservedWidth,
            @Px int newReservedWidth,
            @HeightType int oldHeightType,
            @HeightType int newHeightType) {
        mIsVerticalTabsActiveSupplier.set(mManualVisible);
    }

    @Override
    public boolean supportsManualResize() {
        // The hover expansion is transient and only overlays the web contents, so there is no
        // stable edge to drag while it is showing.
        return VerticalTabUtils.isManualResizeEnabled()
                && !mCollapseController.isForcedCollapsed()
                && !mCollapseController.isHoverExpanded();
    }

    @Override
    public void onResizeLive(@Px int proposedWidthPx) {
        mLiveResizeWidth = proposedWidthPx;
        // Dragging the rail is an explicit request for an expanded rail. The width is clamped to
        // the rail's minimum until the drag is released.
        mCollapseController.setCollapsedByUserFromResize(/* isCollapsed= */ false);
    }

    @Override
    public void onResizeCommitted(@Px int finalWidthPx) {
        mLiveResizeWidth = 0;
        boolean isCollapsed = finalWidthPx < mMinManualWidth;
        mCollapseController.setCollapsedByUserFromResize(isCollapsed);

        // Releasing the drag below the rail's minimum usable width collapses it. When isCollapsed
        // is true, setCollapsedByUserFromResize() persists the collapsed state, so we only persist
        // the non-collapsed user-set width here.
        if (!isCollapsed) {
            int widthDp = ViewUtils.pxToDp(mRootView.getContext(), finalWidthPx);
            VerticalTabUtils.setUserResizedWidthDp(widthDp);
        }
    }

    // SideUiObserver implementation:
    @Override
    public @Nullable Transition onPreSideUiSpecsChange(
            SideUiSpecs sideUiSpecs, UiUpdateRequest request) {
        int side = getAnchorSide();
        // The rail's contents follow the width the rail is rendered at, which is wider than the
        // width it reserves while it is expanded on hover.
        int newRenderedWidth = sideUiSpecs.getRenderedWidth(side);
        int oldRenderedWidth = mSideUiCoordinator.getCurrentSideUiSpecs().getRenderedWidth(side);

        if (oldRenderedWidth > 0 && newRenderedWidth > 0 && oldRenderedWidth != newRenderedWidth) {
            mTabListCoordinator.setInTransition(true);
            TransitionSet transitionSet =
                    new TransitionSet()
                            .setOrdering(TransitionSet.ORDERING_TOGETHER)
                            .addTransition(new ChangeBounds())
                            .addTransition(new Fade());
            transitionSet.excludeTarget(R.id.compositor_view_holder, /* exclude= */ true);
            transitionSet.excludeChildren(R.id.compositor_view_holder, /* exclude= */ true);
            return transitionSet;
        }
        return null;
    }

    @Override
    public void onTransitionEnded(SideUiSpecs sideUiSpecs, UiUpdateRequest request) {
        mTabListCoordinator.setInTransition(false);
    }

    @Override
    public void onSideUiSpecsChanged(SideUiSpecs sideUiSpecs, UiUpdateRequest request) {
        mCollapseController.applyEffectiveState();
    }

    private @Px int calculateRenderedWidthPx(
            @WindowWidthBoundary int boundary,
            @RailCollapseState int effectiveState,
            @Px int windowWidthPx,
            @Px int availableWidthPx) {
        if (effectiveState == RailCollapseState.COLLAPSED) {
            return mCollapsedViewWidth;
        }
        if (VerticalTabUtils.isManualResizeEnabled()) {
            @Px
            int savedManualWidth =
                    ViewUtils.dpToPx(
                            mRootView.getContext(), VerticalTabUtils.getUserResizedWidthDp());
            @Px int manualWidth = mLiveResizeWidth != 0 ? mLiveResizeWidth : savedManualWidth;
            if (manualWidth != 0) {
                // User-resized width overrides auto-sizing and is clamped only by rail and
                // available width bounds.
                return MathUtils.clamp(
                        manualWidth, mMinManualWidth, Math.min(mMaxManualWidth, availableWidthPx));
            }
        }
        if (boundary == WindowWidthBoundary.DYNAMIC_EXPANDABLE) {
            int ratioWidthPx =
                    Math.round(windowWidthPx * VerticalTabUtils.EXPANDED_WINDOW_WIDTH_RATIO);
            return Math.min(mExpandedViewWidth, Math.min(ratioWidthPx, availableWidthPx));
        }
        return mExpandedViewWidth;
    }

    @RailCollapseState
    int getRailCollapseStateForTesting() {
        return mCollapseController.getEffectiveRailCollapseState();
    }
}
