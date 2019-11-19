// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.view.ViewGroup;

import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.ui.base.ViewAndroidDelegate;

/**
 * Implementation of the abstract class {@link ViewAndroidDelegate} for Chrome.
 */
public class TabViewAndroidDelegate extends ViewAndroidDelegate {
    private final Tab mTab;

    /**
     * The inset for the bottom of the Visual Viewport in pixels, or 0 for no insetting.
     * This is the source of truth for the application viewport inset for this embedder.
     */
    private int mApplicationViewportInsetBottomPx;

    TabViewAndroidDelegate(Tab tab, ViewGroup containerView) {
        super(containerView);
        mTab = tab;
    }

    @Override
    public void onBackgroundColorChanged(int color) {
        mTab.onBackgroundColorChanged(color);
    }

    @Override
    public void onTopControlsChanged(int topControlsOffsetY, int contentOffsetY) {
        TabBrowserControlsState.get(mTab).setTopOffset(topControlsOffsetY, contentOffsetY);
    }

    @Override
    public void onBottomControlsChanged(int bottomControlsOffsetY, int bottomContentOffsetY) {
        TabBrowserControlsState.get(mTab).setBottomOffset(bottomControlsOffsetY);
    }

    /**
     * Sets the Visual Viewport bottom inset.
     * @param viewportInsetBottomPx The bottom inset in pixels.  Use {@code 0} for no inset.
     */
    public void insetViewportBottom(int viewportInsetBottomPx) {
        mApplicationViewportInsetBottomPx = viewportInsetBottomPx;

        RenderWidgetHostView renderWidgetHostView = mTab.getWebContents().getRenderWidgetHostView();
        if (renderWidgetHostView == null) return;

        renderWidgetHostView.onViewportInsetBottomChanged();
    }

    @Override
    protected int getViewportInsetBottom() {
        return mApplicationViewportInsetBottomPx;
    }
}
