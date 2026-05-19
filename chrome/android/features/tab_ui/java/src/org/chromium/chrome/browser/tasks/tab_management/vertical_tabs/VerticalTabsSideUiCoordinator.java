// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.side_ui.SideUiContainer;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;

/**
 * Coordinator that acts as a container for the Vertical Tab List within the Side UI framework. This
 * wraps {@link VerticalTabListCoordinator} to adapt it to the {@link SideUiContainer} interface,
 * separating container-level layout and sizing concerns from the tab list itself.
 */
@NullMarked
public class VerticalTabsSideUiCoordinator implements SideUiContainer {
    private final FrameLayout mRootView;
    private final @AnchorSide int mAnchorSide;
    private final VerticalTabListCoordinator mTabListCoordinator;

    public VerticalTabsSideUiCoordinator(
            Activity activity, VerticalTabListCoordinator tabListCoordinator) {
        // TODO(crbug.com/513622986): Resolve physical Left rail placement dynamically based on RTL.
        mAnchorSide = AnchorSide.START;

        mRootView = new FrameLayout(activity);
        mRootView.setLayoutParams(
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));

        mTabListCoordinator = tabListCoordinator;
        mRootView.addView(mTabListCoordinator.getView());
    }

    @Override
    public View getView() {
        return mRootView;
    }

    @Override
    public int getCurrentWidth() {
        return mRootView.getWidth();
    }

    @Override
    public int determineContainerWidth(int requestedWidth, int availableWidth, int windowWidth) {
        // TODO(crbug.com/513622986): Implement layout threshold negotiation to auto-hide rail.
        return requestedWidth;
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
    public void onWindowResized(boolean canShowSideUi) {
        // TODO(crbug.com/513622986): Handle auto-hide logic when screen size goes below threshold.
    }

    public void destroy() {
        mTabListCoordinator.destroy();
    }
}
