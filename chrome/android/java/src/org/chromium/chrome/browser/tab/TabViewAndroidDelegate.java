// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
 * Implementation of the abstract class {@link ViewAndroidDelegate} for Chrome.
 */
public class TabViewAndroidDelegate extends ViewAndroidDelegate {
    private final TabImpl mTab;

    /**
     * The inset for the bottom of the Visual Viewport in pixels, or 0 for no insetting.
     * This is the source of truth for the application viewport inset for this embedder.
     */
    private int mApplicationViewportInsetBottomPx;

    /** The inset supplier the observer is currently attached to. */
    private ObservableSupplier<Integer> mCurrentInsetSupplier;

    TabViewAndroidDelegate(Tab tab, ViewGroup containerView) {
        super(containerView);
        mTab = (TabImpl) tab;

        Callback<Integer> insetObserver = (inset) -> updateInsetViewportBottom();
        mCurrentInsetSupplier = tab.getWindowAndroid().getApplicationBottomInsetProvider();
        mCurrentInsetSupplier.addObserver(insetObserver);

        mTab.addObserver(new EmptyTabObserver() {
            @Override
            public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
                if (window != null) {
                    mCurrentInsetSupplier =
                            tab.getWindowAndroid().getApplicationBottomInsetProvider();
                    mCurrentInsetSupplier.addObserver(insetObserver);
                } else {
                    mCurrentInsetSupplier.removeObserver(insetObserver);
                    mCurrentInsetSupplier = null;
                    updateInsetViewportBottom();
                }
            }

            @Override
            public void onShown(Tab tab, int type) {
                updateInsetViewportBottom();
            }

            @Override
            public void onHidden(Tab tab, int reason) {
                updateInsetViewportBottom();
            }
        });
    }

    @Override
    public void onBackgroundColorChanged(int color) {
        mTab.onBackgroundColorChanged(color);
    }

    @Override
    public void onTopControlsChanged(
            int topControlsOffsetY, int contentOffsetY, int topControlsMinHeightOffsetY) {
        TabBrowserControlsOffsetHelper.get(mTab).setTopOffset(
                topControlsOffsetY, contentOffsetY, topControlsMinHeightOffsetY);
    }

    @Override
    public void onBottomControlsChanged(
            int bottomControlsOffsetY, int bottomControlsMinHeightOffsetY) {
        TabBrowserControlsOffsetHelper.get(mTab).setBottomOffset(
                bottomControlsOffsetY, bottomControlsMinHeightOffsetY);
    }

    /** Sets the Visual Viewport bottom inset. */
    private void updateInsetViewportBottom() {
        int inset =
                mTab.isHidden() || mCurrentInsetSupplier == null ? 0 : mCurrentInsetSupplier.get();

        if (inset == mApplicationViewportInsetBottomPx) return;

        mApplicationViewportInsetBottomPx = inset;

        RenderWidgetHostView renderWidgetHostView = mTab.getWebContents().getRenderWidgetHostView();
        if (renderWidgetHostView == null) return;

        renderWidgetHostView.onViewportInsetBottomChanged();
    }

    @Override
    protected int getViewportInsetBottom() {
        return mApplicationViewportInsetBottomPx;
    }
}
