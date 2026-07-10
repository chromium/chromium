// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.Px;

import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.side_ui.SideUiContainer;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.ui.base.ViewUtils;

/**
 * Coordinator that acts as a container for the Vertical Tab List within the Side UI framework. This
 * wraps {@link VerticalTabListCoordinator} to adapt it to the {@link SideUiContainer} interface,
 * separating container-level layout and sizing concerns from the tab list itself.
 */
@NullMarked
public class VerticalTabsSideUiCoordinator implements SideUiContainer {
    static final int VIEW_WIDTH_DP = VerticalTabUtils.SIDE_UI_CONTAINER_WIDTH_DP;
    static final int COLLAPSED_WIDTH_DP = VerticalTabUtils.SIDE_UI_CONTAINER_COLLAPSED_WIDTH_DP;

    private final SideUiCoordinator mSideUiCoordinator;
    private final FrameLayout mRootView;
    private final @AnchorSide int mAnchorSide;
    private final VerticalTabListCoordinator mTabListCoordinator;
    private final @Px int mExpandedViewWidth;
    private final @Px int mCollapsedViewWidth;
    private final SettableNonNullObservableSupplier<Boolean> mIsVerticalTabsActiveSupplier;
    private boolean mIsCollapsed;

    // Whether the vertical tab is automatically hidden due to run-time conditions.
    // TODO(crbug.com/513622986): Handle auto-hide logic when screen size goes below threshold.
    @SuppressWarnings("UnusedVariable")
    private boolean mIsAutoHidden;

    // Whether the vertical tab is set to visible via UI. Remains true even if it is temporarily
    // hidden by other conditions such as narrow window i.e. |mIsAutoHidden| is true.
    private boolean mManualVisible;

    private final VerticalTabListCoordinator.RailCollapseListener mCollapseListener =
            this::onCollapseChanged;

    public VerticalTabsSideUiCoordinator(
            Activity activity,
            SideUiCoordinator sideUiCoordinator,
            VerticalTabListCoordinator tabListCoordinator,
            SettableNonNullObservableSupplier<Boolean> isVerticalTabsActiveSupplier) {
        mAnchorSide = AnchorSide.LEFT;

        mSideUiCoordinator = sideUiCoordinator;
        mTabListCoordinator = tabListCoordinator;
        mIsVerticalTabsActiveSupplier = isVerticalTabsActiveSupplier;

        mRootView = new FrameLayout(activity);
        mRootView.setLayoutParams(
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
        mRootView.addView(mTabListCoordinator.getView());
        mExpandedViewWidth = ViewUtils.dpToPx(activity, VIEW_WIDTH_DP);
        mCollapsedViewWidth = ViewUtils.dpToPx(activity, COLLAPSED_WIDTH_DP);
        mTabListCoordinator.setCollapseListener(mCollapseListener);
    }

    public void setVisible(boolean show) {
        mManualVisible = show;
        mSideUiCoordinator.updateUi(
                new UiUpdateRequest(getSideUiId(), /* suppressAnimations= */ false));
    }

    @Override
    public View getView() {
        return mRootView;
    }

    @Override
    public @SideUiId int getSideUiId() {
        return SideUiId.VERTICAL_TABS;
    }

    @Override
    public int determineShowableWidth(int availableWidth, int windowWidth) {
        // TODO(crbug.com/509226293): Implement layout threshold negotiation to auto-hide rail.
        int targetWidth = mIsCollapsed ? mCollapsedViewWidth : mExpandedViewWidth;
        return availableWidth < targetWidth ? 0 : targetWidth;
    }

    @Override
    @AnchorSide
    public int getAnchorSide() {
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
    public void onUiUpdateCompleted(@Px int oldWidth, @Px int newWidth) {
        mIsVerticalTabsActiveSupplier.set(newWidth > 0);
    }

    private void onCollapseChanged(boolean isCollapsed) {
        mIsCollapsed = isCollapsed;
        mSideUiCoordinator.updateUi(
                new UiUpdateRequest(getSideUiId(), /* suppressAnimations= */ true));
    }

    public void destroy() {
        mTabListCoordinator.setCollapseListener(null);
        mTabListCoordinator.destroy();
    }
}
