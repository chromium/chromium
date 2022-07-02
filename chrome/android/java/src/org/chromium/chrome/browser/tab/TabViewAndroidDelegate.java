// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Build.VERSION;
import android.view.DragAndDropPermissions;
import android.view.DragEvent;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragAndDropBrowserDelegate;
import org.chromium.ui.dragdrop.DragStateTracker;
import org.chromium.ui.dragdrop.DropDataContentProvider;

/**
 * Implementation of the abstract class {@link ViewAndroidDelegate} for Chrome.
 */
public class TabViewAndroidDelegate extends ViewAndroidDelegate {
    private static final String PARAM_CLEAR_CACHE_DELAYED_MS = "ClearCacheDelayedMs";
    private static final String PARAM_DROP_IN_CHROME = "DropInChrome";
    private final TabImpl mTab;

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

        if (ContentFeatureList.isEnabled(ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU)) {
            int delay = ContentFeatureList.getFieldTrialParamByFeatureAsInt(
                    ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU, PARAM_CLEAR_CACHE_DELAYED_MS,
                    DropDataContentProvider.DEFAULT_CLEAR_CACHED_DATA_INTERVAL_MS);
            DropDataContentProvider.setClearCachedDataIntervalMs(delay);

            boolean supportDropInChrome = ContentFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU, PARAM_DROP_IN_CHROME, false);
            if (VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                DragAndDropBrowserDelegate browserDelegate =
                        new DragAndDropBrowserDelegateImpl(mTab, supportDropInChrome);
                getDragAndDropDelegate().setDragAndDropBrowserDelegate(browserDelegate);
            }
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

        RenderWidgetHostView renderWidgetHostView = mTab.getWebContents().getRenderWidgetHostView();
        if (renderWidgetHostView == null) return;

        renderWidgetHostView.onViewportInsetBottomChanged();
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
    }

    /**
     * Delegate for browser related functions used by Drag and Drop.
     */
    static class DragAndDropBrowserDelegateImpl implements DragAndDropBrowserDelegate {
        private final TabImpl mTab;
        private final boolean mSupportDropInChrome;

        public DragAndDropBrowserDelegateImpl(TabImpl tab, boolean supportDropInChrome) {
            mTab = tab;
            mSupportDropInChrome = supportDropInChrome;
        }

        @Override
        public boolean getSupportDropInChrome() {
            return mSupportDropInChrome;
        }

        @Override
        @RequiresApi(Build.VERSION_CODES.N)
        public DragAndDropPermissions getDragAndDropPermissions(DragEvent dropEvent) {
            Activity activity = ContextUtils.activityFromContext(mTab.getContext());
            if (activity == null) {
                return null;
            }
            return activity.requestDragAndDropPermissions(dropEvent);
        }

        @Override
        public Intent createLinkIntent(String urlString) {
            Intent intent = null;
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.NEW_INSTANCE_FROM_DRAGGED_LINK)
                    && MultiWindowUtils.isMultiInstanceApi31Enabled()) {
                intent = MultiWindowUtils.createNewWindowIntent(
                        mTab.getContext().getApplicationContext(),
                        MultiWindowUtils.getInstanceIdForViewIntent(), true, false);
                intent.setData(Uri.parse(urlString));
                intent.putExtra(IntentHandler.EXTRA_SOURCE_DRAG_DROP, true);
            }
            return intent;
        }
    }
}
