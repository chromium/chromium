// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.dragdrop.ChromeDragAndDropBrowserDelegate;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.base.ViewAndroidDelegate;
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
    private int mApplicationViewportInsetBottomPx;

    /** The inset supplier the observer is currently attached to. */
    private ObservableSupplier<Integer> mCurrentInsetSupplier;

    TabViewAndroidDelegate(Tab tab, ContentView containerView) {
        super(containerView);
        mTab = (TabImpl) tab;
        containerView.addOnDragListener(getDragStateTracker());

        if (ContentFeatureList.isEnabled(ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU)
                && DragAndDropDelegate.isDragAndDropSupportedForOs()) {
            mDragAndDropBrowserDelegate = new ChromeDragAndDropBrowserDelegate(tab.getContext());
            getDragAndDropDelegate().setDragAndDropBrowserDelegate(mDragAndDropBrowserDelegate);
        }

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

    @Override
    public @Nullable DragStateTracker getDragStateTracker() {
        return getDragStateTrackerInternal();
    }

    /** Sets the Visual Viewport bottom inset. */
    private void updateInsetViewportBottom() {
        int inset =
                mTab.isHidden() || mCurrentInsetSupplier == null ? 0 : mCurrentInsetSupplier.get();

        if (inset == mApplicationViewportInsetBottomPx) return;

        mApplicationViewportInsetBottomPx = inset;

        if (mTab.getWebContents() == null
                || mTab.getWebContents().getRenderWidgetHostView() == null) {
            return;
        }

        mTab.getWebContents().getRenderWidgetHostView().onViewportInsetBottomChanged();
    }

    @Override
    protected int getViewportInsetBottom() {
        return mApplicationViewportInsetBottomPx;
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

    @VisibleForTesting
    DragAndDropBrowserDelegate getDragAndDropBrowserDelegateForTesting() {
        return mDragAndDropBrowserDelegate;
    }
}
