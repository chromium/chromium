// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.side_ui.SideUiContainer;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiContainerProperties;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;
import org.chromium.ui.base.ViewUtils;

/**
 * Coordinator that acts as a container for the Vertical Tab List within the Side UI framework. This
 * wraps {@link VerticalTabListCoordinator} to adapt it to the {@link SideUiContainer} interface,
 * separating container-level layout and sizing concerns from the tab list itself.
 */
@NullMarked
public class VerticalTabsSideUiCoordinator implements SideUiContainer {
    private static final int VIEW_WIDTH_DP = 206;

    private final Activity mActivity;
    private final SideUiCoordinator mSideUiCoordinator;
    private final FrameLayout mRootView;
    private final @AnchorSide int mAnchorSide;
    private final VerticalTabListCoordinator mTabListCoordinator;

    // Whether the vertical tab is automatically hidden due to run-time conditions.
    private boolean mIsAutoHidden;

    // Whether the vertical tab is set to visible via UI. Remains true even if it is temporarily
    // hidden by other conditions such as narrow window i.e. |mIsAutoHidden| is true.
    private boolean mManualVisible;

    public VerticalTabsSideUiCoordinator(
            Activity activity,
            SideUiCoordinator sideUiCoordinator,
            VerticalTabListCoordinator tabListCoordinator) {
        mAnchorSide = AnchorSide.LEFT;

        mActivity = activity;
        mSideUiCoordinator = sideUiCoordinator;
        mTabListCoordinator = tabListCoordinator;

        mRootView = new FrameLayout(activity);
        mRootView.setLayoutParams(
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
        mRootView.addView(mTabListCoordinator.getView());
    }

    public void setVisible(boolean show) {
        mManualVisible = show;
        requestShow(show);
    }

    private void requestShow(boolean show) {
        @Px int viewWidth = show ? ViewUtils.dpToPx(mActivity, VIEW_WIDTH_DP) : 0;
        mSideUiCoordinator.requestUpdateContainer(
                new SideUiContainerProperties(getSideUiId(), mAnchorSide, viewWidth),
                /* suppressAnimations */ true);
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
    public int determineContainerWidth(int requestedWidth, int availableWidth, int windowWidth) {
        // TODO(crbug.com/509226293): Implement layout threshold negotiation to auto-hide rail.
        // Respond with the requested width only if currently on.
        return mManualVisible ? requestedWidth : 0;
    }

    @Override
    @AnchorSide
    public int getAnchorSide() {
        return mAnchorSide;
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
    public void onContainerResized(@Px int containerWidth) {}

    @Override
    public void onWindowResized(boolean canShowSideUi) {
        // TODO(crbug.com/513622986): Handle auto-hide logic when screen size goes below threshold.
        // No-op if currently off or visibility hasn't changed.
        if (!mManualVisible || (canShowSideUi != mIsAutoHidden)) return;

        mIsAutoHidden = !canShowSideUi;
        requestShow(canShowSideUi);
    }

    public void destroy() {
        mTabListCoordinator.destroy();
    }
}
