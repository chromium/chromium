// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static java.util.Collections.emptySet;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.transition.ChangeBounds;
import android.transition.Fade;
import android.transition.Transition;
import android.transition.TransitionSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.Px;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
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
import org.chromium.ui.util.TokenHolder;

import java.util.ArrayList;
import java.util.List;

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
    private final BrowserStateBrowserControlsVisibilityDelegate mBrowserControlsVisibilityDelegate;
    private final FrameLayout mRootView;
    private final @AnchorSide int mAnchorSide;
    private final VerticalTabListCoordinator mTabListCoordinator;
    private final VerticalTabRailCollapseController mCollapseController;
    private final @Px int mExpandedViewWidth;
    private final @Px int mCollapsedViewWidth;
    private final SettableNonNullObservableSupplier<Boolean> mIsVerticalTabsActiveSupplier;
    private final SettableNonNullObservableSupplier<Boolean> mIsAutoHiddenSupplier =
            ObservableSuppliers.createNonNull(false);

    // Whether the vertical tab is set to visible via UI. Remains true even if it is temporarily
    // hidden by other conditions such as narrow window i.e. |mIsAutoHiddenSupplier.get()| is true.
    private boolean mManualVisible;
    private int mBrowserControlsToken = TokenHolder.INVALID_TOKEN;

    public VerticalTabsSideUiCoordinator(
            Activity activity,
            SideUiCoordinator sideUiCoordinator,
            BrowserStateBrowserControlsVisibilityDelegate browserControlsVisibilityDelegate,
            VerticalTabListCoordinator tabListCoordinator,
            SettableNonNullObservableSupplier<Boolean> isVerticalTabsActiveSupplier) {
        mAnchorSide = AnchorSide.LEFT;

        mSideUiCoordinator = sideUiCoordinator;
        mBrowserControlsVisibilityDelegate = browserControlsVisibilityDelegate;
        mTabListCoordinator = tabListCoordinator;
        mCollapseController = mTabListCoordinator.getCollapseController();
        mIsVerticalTabsActiveSupplier = isVerticalTabsActiveSupplier;
        mSideUiCoordinator.addObserver(this);

        mRootView =
                new FrameLayout(activity) {
                    @Override
                    protected void onConfigurationChanged(Configuration newConfig) {
                        super.onConfigurationChanged(newConfig);
                        // Observe configuration changes on mRootView because onSideUiSpecsChanged()
                        // is only dispatched when there is a non-empty diff between the new and
                        // current SideUiSpecs. If the window is resized without altering
                        // SideUiSpecs, this callback ensures we still update rail collapse and
                        // button state when crossing the narrow-window threshold.
                        boolean isForcedCollapsed =
                                VerticalTabUtils.getWindowWidthBoundary(newConfig.screenWidthDp)
                                        <= WindowWidthBoundary.FORCED_COLLAPSED;
                        updateCollapseButtonAndRailState(isForcedCollapsed);
                    }
                };
        mRootView.setLayoutParams(
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
        mRootView.addView(mTabListCoordinator.getView());
        mExpandedViewWidth = ViewUtils.dpToPx(activity, VIEW_WIDTH_DP);
        mCollapsedViewWidth = ViewUtils.dpToPx(activity, COLLAPSED_WIDTH_DP);
        mCollapseController.setRailCollapseListener(this::onRailCollapseStateChangeRequestedByUser);
    }

    public NonNullObservableSupplier<Boolean> getIsAutoHiddenSupplier() {
        return mIsAutoHiddenSupplier;
    }

    public void setVisible(boolean show, boolean suppressAnimations) {
        mManualVisible = show;
        if (!show) {
            updateAutoHiddenState(false);
            releasePersistentShowingToken();
        } else {
            if (mBrowserControlsToken == TokenHolder.INVALID_TOKEN) {
                mBrowserControlsToken = mBrowserControlsVisibilityDelegate.showControlsPersistent();
            }
        }
        mSideUiCoordinator.updateUi(new UiUpdateRequest(getSideUiId(), suppressAnimations));
        // Fallback: If hiding VT when spec diff is empty (no hide animation scheduled),
        // update active state immediately to avoid dropping the state update.
        SideUiSpecs currentSpecs = mSideUiCoordinator.getCurrentSideUiSpecs();
        if (!show && (currentSpecs == null || currentSpecs.getWidth(getAnchorSide()) == 0)) {
            mIsVerticalTabsActiveSupplier.set(false);
        }
    }

    public void destroy() {
        updateAutoHiddenState(false);
        releasePersistentShowingToken();
        mSideUiCoordinator.removeObserver(this);
        mCollapseController.setRailCollapseListener(null);
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

    private void releasePersistentShowingToken() {
        if (mBrowserControlsToken != TokenHolder.INVALID_TOKEN) {
            mBrowserControlsVisibilityDelegate.releasePersistentShowingToken(mBrowserControlsToken);
            mBrowserControlsToken = TokenHolder.INVALID_TOKEN;
        }
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

        int targetWidth = calculateWidthPx(boundary, windowWidth, availableWidth);
        boolean shouldHide = boundary == WindowWidthBoundary.NOT_SHOWABLE;

        updateAutoHiddenState(mManualVisible && shouldHide);
        if (isFullscreen || shouldHide) {
            return new SideUiSize(0, HeightType.NOT_APPLICABLE);
        }
        return new SideUiSize(targetWidth, HeightType.TOOLBAR);
    }

    private void updateAutoHiddenState(boolean isHiddenDueToNarrowWidth) {
        mIsAutoHiddenSupplier.set(isHiddenDueToNarrowWidth);
    }

    @Override
    public @AnchorSide int getAnchorSide() {
        return mAnchorSide;
    }

    @Override
    public boolean hasContentToShow() {
        return mManualVisible;
    }

    @Override
    public void setWidth(int width) {
        ViewGroup.LayoutParams layoutParams = mRootView.getLayoutParams();
        if (layoutParams != null) {
            layoutParams.width = width;
            mRootView.setLayoutParams(layoutParams);
        }
    }

    @Override
    public void onUiUpdateCompleted(
            @Px int oldWidth,
            @Px int newWidth,
            @HeightType int oldHeightType,
            @HeightType int newHeightType) {
        mIsVerticalTabsActiveSupplier.set(mManualVisible);
    }

    // SideUiObserver implementation:
    @Override
    public @Nullable Transition onPreSideUiSpecsChange(SideUiSpecs sideUiSpecs) {
        int side = getAnchorSide();
        int newWidth = sideUiSpecs.getWidth(side);
        int oldWidth = mSideUiCoordinator.getCurrentSideUiSpecs().getWidth(side);

        if (oldWidth > 0 && newWidth > 0 && oldWidth != newWidth) {
            TransitionSet transitionSet =
                    new TransitionSet()
                            .setOrdering(TransitionSet.ORDERING_TOGETHER)
                            .addTransition(new ChangeBounds())
                            .addTransition(new Fade());
            List<View> views = new ArrayList<>();
            views.add(mRootView);
            ViewUtils.getAllDescendants(mRootView, views, emptySet());
            for (View view : views) {
                transitionSet.addTarget(view);
            }
            return transitionSet;
        }
        return null;
    }

    @Override
    public void onSideUiSpecsChanged(SideUiSpecs sideUiSpecs) {
        updateCollapseButtonAndRailState(isForcedCollapsed());
    }

    // Sequence when user requests state change:
    // 1. onRailCollapseStateChangeRequestedByUser: updates mRailCollapseStateByUser & triggers Side
    // UI update.
    // 2. determineShowableSize: SideUiCoordinator queries target width for transition bounds.
    // 3. onSideUiSpecsChanged: fired post-specs change (only if width/specs changed) to sync button
    // and rail model state.
    private void onRailCollapseStateChangeRequestedByUser(
            @RailCollapseState int currentState, @RailCollapseState int targetState) {
        // TODO(crbug.com/527641177): Remove this if check after expand on hovering UI is done.
        if (VerticalTabRailCollapseController.isExpanded(currentState)
                && VerticalTabRailCollapseController.isExpanded(targetState)) {
            updateCollapseButtonAndRailState(isForcedCollapsed());
        } else {
            mSideUiCoordinator.updateUi(
                    new UiUpdateRequest(getSideUiId(), /* suppressAnimations= */ false));
        }
    }

    /**
     * Updates the collapse button enabled state and effective rail collapse state based on whether
     * the window forces the rail to collapse.
     *
     * @param isForcedCollapsed True if the window width is too narrow to expand.
     */
    private void updateCollapseButtonAndRailState(boolean isForcedCollapsed) {
        // Apply effective state (COLLAPSED if forced collapsed, or mRailCollapseStateByUser if
        // expandable).
        mCollapseController.dispatchRailCollapseStateUpdate(
                mCollapseController.getEffectiveRailCollapseState(isForcedCollapsed));
        // Disable the collapse button in narrow windows so users cannot expand beyond bounds.
        mTabListCoordinator.setCollapseButtonEnabled(!isForcedCollapsed);
    }

    private @Px int calculateWidthPx(
            @WindowWidthBoundary int boundary, @Px int windowWidthPx, @Px int availableWidthPx) {
        boolean isForcedCollapsed = boundary <= WindowWidthBoundary.FORCED_COLLAPSED;
        if (mCollapseController.getEffectiveRailCollapseState(isForcedCollapsed)
                == RailCollapseState.COLLAPSED) {
            return mCollapsedViewWidth;
        }
        if (boundary == WindowWidthBoundary.DYNAMIC_EXPANDABLE) {
            int ratioWidthPx =
                    Math.round(windowWidthPx * VerticalTabUtils.EXPANDED_WINDOW_WIDTH_RATIO);
            return Math.min(mExpandedViewWidth, Math.min(ratioWidthPx, availableWidthPx));
        }
        return mExpandedViewWidth;
    }

    private boolean isForcedCollapsed() {
        int screenWidthDp = mRootView.getContext().getResources().getConfiguration().screenWidthDp;
        return VerticalTabUtils.getWindowWidthBoundary(screenWidthDp)
                <= WindowWidthBoundary.FORCED_COLLAPSED;
    }

    @RailCollapseState
    int getRailCollapseStateForTesting() {
        return mCollapseController.getEffectiveRailCollapseState(isForcedCollapsed());
    }
}
