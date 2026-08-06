// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static java.util.Collections.emptySet;

import android.app.Activity;
import android.content.res.Configuration;
import android.transition.ChangeBounds;
import android.transition.Fade;
import android.transition.Transition;
import android.transition.TransitionSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.Px;

import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
import org.chromium.ui.base.ViewUtils;

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
    private final FrameLayout mRootView;
    private final @AnchorSide int mAnchorSide;
    private final VerticalTabListCoordinator mTabListCoordinator;
    private final VerticalTabRailCollapseController mCollapseController;
    private final @Px int mExpandedViewWidth;
    private final @Px int mCollapsedViewWidth;
    private final SettableNonNullObservableSupplier<Boolean> mIsVerticalTabsActiveSupplier;

    // Whether the vertical tab is set to visible via UI. Remains true even if it is temporarily
    // hidden by other conditions such as narrow window i.e. |mIsAutoHidden| is true.
    private boolean mManualVisible;

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
                        updateCollapseButtonAndRailState(
                                newConfig.screenWidthDp
                                        < VerticalTabUtils.MIN_EXPAND_WINDOW_WIDTH_DP);
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

    public void setVisible(boolean show, boolean suppressAnimations) {
        mManualVisible = show;
        mSideUiCoordinator.updateUi(new UiUpdateRequest(getSideUiId(), suppressAnimations));
        // Fallback: If hiding VT when spec diff is empty (no hide animation scheduled),
        // update active state immediately to avoid dropping the state update.
        SideUiSpecs currentSpecs = mSideUiCoordinator.getCurrentSideUiSpecs();
        if (!show && (currentSpecs == null || currentSpecs.getWidth(getAnchorSide()) == 0)) {
            mIsVerticalTabsActiveSupplier.set(false);
        }
    }

    public void destroy() {
        mSideUiCoordinator.removeObserver(this);
        mCollapseController.setRailCollapseListener(null);
        mTabListCoordinator.destroy();
        mIsVerticalTabsActiveSupplier.set(false);
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
        if (isFullscreen) {
            return new SideUiSize(0, HeightType.NOT_APPLICABLE);
        }
        int targetWidth =
                mCollapseController.getEffectiveRailCollapseState(isCurrentWindowNarrow())
                                == RailCollapseState.COLLAPSED
                        ? mCollapsedViewWidth
                        : mExpandedViewWidth;
        boolean shouldHide = availableWidth < targetWidth;
        return shouldHide
                ? new SideUiSize(0, HeightType.NOT_APPLICABLE)
                : new SideUiSize(targetWidth, HeightType.TOOLBAR);
    }

    @Override
    public @AnchorSide int getAnchorSide() {
        return mAnchorSide;
    }

    @Override
    public boolean hasContentToShow() {
        return mManualVisible;
    }

    /**
     * Returns whether or not Tab layout toggle menu can be activated. Used to grey out the menu
     * item if it cannot be activated due to conditions such as a narrow app Window width.
     */
    public boolean canActivateTabLayoutToggleMenu() {
        return mSideUiCoordinator.canShowSideUi(SideUiId.VERTICAL_TABS);
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
        updateCollapseButtonAndRailState(isCurrentWindowNarrow());
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
            updateCollapseButtonAndRailState(isCurrentWindowNarrow());
        } else {
            mSideUiCoordinator.updateUi(
                    new UiUpdateRequest(getSideUiId(), /* suppressAnimations= */ false));
        }
    }

    /**
     * Updates the collapse button enabled state and effective rail collapse state based on window
     * width.
     *
     * @param isNarrow True if the current window width is below the threshold for expanding.
     */
    private void updateCollapseButtonAndRailState(boolean isNarrow) {
        // Apply effective state (COLLAPSED if narrow, or mRailCollapseStateByUser if wide).
        mCollapseController.dispatchRailCollapseStateUpdate(
                mCollapseController.getEffectiveRailCollapseState(isNarrow));
        // Disable the collapse button in narrow windows so users cannot expand beyond bounds.
        mTabListCoordinator.setCollapseButtonEnabled(!isNarrow);
    }

    private boolean isCurrentWindowNarrow() {
        return mRootView.getContext().getResources().getConfiguration().screenWidthDp
                < VerticalTabUtils.MIN_EXPAND_WINDOW_WIDTH_DP;
    }

    @RailCollapseState
    int getRailCollapseStateForTesting() {
        return mCollapseController.getEffectiveRailCollapseState(isCurrentWindowNarrow());
    }
}
