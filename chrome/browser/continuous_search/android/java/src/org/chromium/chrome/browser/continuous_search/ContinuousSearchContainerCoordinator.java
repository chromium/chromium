// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.content.res.Resources;
import android.view.View;
import android.view.ViewStub;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/**
 * Coordinator in charge of managing the container for Continuous Search Navigation. The container
 * takes care of positioning itself in the top controls area and hosts the CSN list.
 */
public class ContinuousSearchContainerCoordinator implements View.OnLayoutChangeListener {
    /**
     * An observer that is notified of the container height changes.
     */
    @FunctionalInterface
    public interface HeightObserver {
        /**
         * This function is called when the container height changes.
         * @param newHeight the new container height.
         * @param animate whether the height change should be animated.
         */
        void onHeightChange(int newHeight, boolean animate);
    }

    private final ContinuousSearchContainerMediator mContainerMediator;
    private final ContinuousSearchListCoordinator mListCoordinator;
    private final ContinuousSearchSceneLayer mSceneLayer;
    private int mResourceId;
    private final LayoutManager mLayoutManager;
    private ViewResourceAdapter mResourceAdapter;
    private final ResourceManager mResourceManager;
    private boolean mResourceRegistered;
    private boolean mLayoutInitialized;
    private final ViewStub mViewStub;
    private ContinuousSearchViewResourceFrameLayout mRootView;

    public ContinuousSearchContainerCoordinator(ViewStub containerViewStub,
            LayoutManager layoutManager, ResourceManager resourceManager,
            ObservableSupplier<Tab> tabSupplier,
            BrowserControlsStateProvider browserControlsStateProvider,
            Supplier<Boolean> canAnimateNativeBrowserControls,
            Supplier<Integer> defaultTopContainerHeightSupplier,
            ThemeColorProvider themeColorProvider, Resources resources,
            Callback<Boolean> hideToolbarShadow) {
        mViewStub = containerViewStub;
        mLayoutManager = layoutManager;
        mResourceManager = resourceManager;
        mSceneLayer = new ContinuousSearchSceneLayer(mResourceManager);
        mLayoutManager.addSceneOverlay(mSceneLayer);
        mContainerMediator = new ContinuousSearchContainerMediator(browserControlsStateProvider,
                layoutManager, canAnimateNativeBrowserControls, defaultTopContainerHeightSupplier,
                this::initializeLayout, hideToolbarShadow);
        mListCoordinator = new ContinuousSearchListCoordinator(tabSupplier, isVisible -> {
            if (isVisible) {
                mContainerMediator.show();
            } else {
                mContainerMediator.hide();
            }
        }, themeColorProvider, resources);
    }

    private void initializeLayout() {
        if (mLayoutInitialized) return;

        mRootView = (ContinuousSearchViewResourceFrameLayout) mViewStub.inflate();
        mResourceId = mRootView.getId();
        mSceneLayer.setResourceId(mResourceId);
        mListCoordinator.initializeLayout(mRootView.findViewById(R.id.container_view));
        mResourceAdapter = mRootView.getResourceAdapter();
        registerResource();
        PropertyModel model = new PropertyModel(ContinuousSearchContainerProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                model, mRootView, ContinuousSearchContainerViewBinder::bindJavaView);
        mLayoutManager.createCompositorMCP(
                model, mSceneLayer, ContinuousSearchContainerViewBinder::bindCompositedView);
        mContainerMediator.onLayoutInitialized(model, mRootView::requestLayout);
        mRootView.addOnLayoutChangeListener(this);
        mLayoutInitialized = true;
    }

    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        assert mRootView != null;
        mContainerMediator.setJavaHeight(v.getHeight() - mRootView.getShadowHeight());
    }

    public static Class getSceneOverlayClass() {
        return ContinuousSearchSceneLayer.class;
    }

    /**
     *  Registers an observer that gets notified when the height of the CSN container changes.
     */
    public void addHeightObserver(HeightObserver observer) {
        mContainerMediator.addHeightObserver(observer);
    }

    public void removeHeightObserver(HeightObserver observer) {
        mContainerMediator.removeHeightObserver(observer);
    }

    public void updateTabObscured(boolean isObscured) {
        mContainerMediator.updateTabObscured(isObscured);
    }

    private void registerResource() {
        if (mResourceRegistered) return;

        mResourceManager.getDynamicResourceLoader().registerResource(mResourceId, mResourceAdapter);
        mResourceRegistered = true;
    }

    private void unregisterResource() {
        if (!mResourceRegistered) return;

        mResourceAdapter.dropCachedBitmap();
        mResourceManager.getDynamicResourceLoader().unregisterResource(mResourceId);
        mResourceRegistered = false;
    }

    public void destroy() {
        if (mLayoutInitialized) mRootView.removeOnLayoutChangeListener(this);
        unregisterResource();
        mContainerMediator.destroy();
        mListCoordinator.destroy();
    }

    @VisibleForTesting
    ContinuousSearchViewResourceFrameLayout getRootViewForTesting() {
        return mRootView;
    }
}
