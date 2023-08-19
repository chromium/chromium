// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.dragdrop.ChromeDragAndDropBrowserDelegate;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.ViewportInsets;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragAndDropBrowserDelegate;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DragStateTracker;

/**
 * Implementation of the abstract class {@link ViewAndroidDelegate} for Chrome.
 */
public class TabViewAndroidDelegate extends ViewAndroidDelegate {
    private final TabImpl mTab;

    @Nullable
    private DragAndDropBrowserDelegate mDragAndDropBrowserDelegate;

    /**
     * The inset for the bottom of the Visual Viewport in pixels, or 0 for no insetting.
     * This is the source of truth for the application viewport inset for this embedder.
     */
    private int mVisualViewportInsetBottomPx;

    /** The inset supplier the observer is currently attached to. */
    private ApplicationViewportInsetSupplier mCurrentInsetSupplier;

    TabViewAndroidDelegate(Tab tab, ContentView containerView) {
        super(containerView);
        mTab = (TabImpl) tab;
        containerView.addOnDragListener(getDragStateTracker());

        if (ContentFeatureMap.isEnabled(ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU)
                && DragAndDropDelegate.isDragAndDropSupportedForOs()) {
            mDragAndDropBrowserDelegate = new ChromeDragAndDropBrowserDelegate(tab.getContext());
            getDragAndDropDelegate().setDragAndDropBrowserDelegate(mDragAndDropBrowserDelegate);
        }

        Callback<ViewportInsets> insetObserver = (unused) -> updateVisualViewportBottomInset();
        mCurrentInsetSupplier = tab.getWindowAndroid().getApplicationBottomInsetSupplier();
        mCurrentInsetSupplier.addObserver(insetObserver);

        mTab.addObserver(new EmptyTabObserver() {
            @Override
            public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
                if (window != null) {
                    mCurrentInsetSupplier =
                            tab.getWindowAndroid().getApplicationBottomInsetSupplier();
                    mCurrentInsetSupplier.addObserver(insetObserver);
                    updateVisualViewportBottomInset();
                } else {
                    mCurrentInsetSupplier.removeObserver(insetObserver);
                    mCurrentInsetSupplier = null;
                    updateVisualViewportBottomInset();
                }
            }

            @Override
            public void onShown(Tab tab, int type) {
                updateVisualViewportBottomInset();
            }

            @Override
            public void onHidden(Tab tab, int reason) {
                updateVisualViewportBottomInset();
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

    @Override
    public @Nullable DragStateTracker getDragStateTracker() {
        return getDragStateTrackerInternal();
    }

    /** Sets the Visual Viewport bottom inset. */
    private void updateVisualViewportBottomInset() {
        int inset = mTab.isHidden() || mCurrentInsetSupplier == null
                ? 0
                : mCurrentInsetSupplier.get().visualViewportBottomInset;

        if (inset == mVisualViewportInsetBottomPx) return;

        mVisualViewportInsetBottomPx = inset;

        if (mTab.getWebContents() == null
                || mTab.getWebContents().getRenderWidgetHostView() == null) {
            return;
        }

        mTab.getWebContents().getRenderWidgetHostView().onViewportInsetBottomChanged();
    }

    @Override
    // TODO(bokan): "Viewport Inset" is overloaded. Rename to make it clearer this is a "visual
    // viewport" inset. Also the RenderWidgetHostView call above.
    protected int getViewportInsetBottom() {
        return mVisualViewportInsetBottomPx;
    }

    @Override
    public void updateAnchorViews(ViewGroup oldContainerView) {
        super.updateAnchorViews(oldContainerView);

        assert oldContainerView
                instanceof ContentView
            : "TabViewAndroidDelegate does not host container views other than ContentView.";

        // Transfer the drag state tracker to the new container view.
        ((ContentView) oldContainerView).removeOnDragListener(getDragStateTracker());
        getContentView().addOnDragListener(getDragStateTracker());
    }

    private ContentView getContentView() {
        assert getContainerView()
                        instanceof ContentView
            : "TabViewAndroidDelegate does not host container views other than ContentView.";

        return (ContentView) getContainerView();
    }

    /* Destroy and clean up {@link DragStateTracker} to the content view. */
    @Override
    public void destroy() {
        super.destroy();
        if (getContentView() != null) {
            getContentView().removeOnDragListener(getDragStateTracker());
        }
        if (mDragAndDropBrowserDelegate != null) {
            getDragAndDropDelegate().setDragAndDropBrowserDelegate(null);
            mDragAndDropBrowserDelegate = null;
        }
    }

    DragAndDropBrowserDelegate getDragAndDropBrowserDelegateForTesting() {
        return mDragAndDropBrowserDelegate;
    }
}
