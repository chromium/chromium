// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import android.content.Context;
import android.graphics.RectF;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.Layout.ViewportMode;
import org.chromium.chrome.browser.compositor.layouts.LayoutProvider;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.components.VirtualView;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilter;
import org.chromium.chrome.browser.compositor.overlays.SceneOverlay;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.toolbar.ToolbarColors;
import org.chromium.chrome.browser.ui.widget.ClipDrawableProgressBar.DrawingInfo;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.resources.ResourceManager;

import java.util.List;

/**
 * A SceneLayer to render layers for the toolbar.
 */
@JNINamespace("android")
public class ToolbarSceneLayer extends SceneOverlayLayer implements SceneOverlay {
    /** Pointer to native ToolbarSceneLayer. */
    private long mNativePtr;

    /** Information used to draw the progress bar. */
    private DrawingInfo mProgressBarDrawingInfo;

    /** An Android Context. */
    private Context mContext;

    /** A LayoutProvider for accessing the current layout. */
    private LayoutProvider mLayoutProvider;

    /** A LayoutRenderHost for accessing drawing information about the toolbar. */
    private LayoutRenderHost mRenderHost;

    /** The static Y offset for the cases where there is a another cc layer above the toolbar. */
    private int mStaticYOffset;

    /**
     * @param context An Android context to use.
     * @param provider A LayoutProvider for accessing the current layout.
     * @param renderHost A LayoutRenderHost for accessing drawing information about the toolbar.
     */
    public ToolbarSceneLayer(Context context, LayoutProvider provider,
            LayoutRenderHost renderHost) {
        mContext = context;
        mLayoutProvider = provider;
        mRenderHost = renderHost;
    }

    /**
     * Set a static Y offset for the toolbar.
     * @param staticYOffset The Y offset in pixels.
     */
    public void setStaticYOffset(int staticYOffset) {
        mStaticYOffset = staticYOffset;
    }

    /**
     * Update the toolbar and progress bar layers.
     *
     * @param browserControlsBackgroundColor The background color of the browser controls.
     * @param browserControlsUrlBarAlpha The alpha of the URL bar.
     * @param fullscreenManager A ChromeFullscreenManager instance.
     * @param resourceManager A ResourceManager for loading static resources.
     * @param forceHideAndroidBrowserControls True if the Android browser controls are being hidden.
     * @param viewportMode The sizing mode of the viewport being drawn in.
     * @param isTablet If the device is a tablet.
     * @param windowHeight The height of the window.
     */
    private void update(int browserControlsBackgroundColor, float browserControlsUrlBarAlpha,
            ChromeFullscreenManager fullscreenManager, ResourceManager resourceManager,
            boolean forceHideAndroidBrowserControls, @ViewportMode int viewportMode,
            boolean isTablet, float windowHeight) {
        if (!DeviceClassManager.enableFullscreen()) return;

        if (fullscreenManager == null) return;
        ControlContainer toolbarContainer = fullscreenManager.getControlContainer();
        if (!isTablet && toolbarContainer != null) {
            if (mProgressBarDrawingInfo == null) mProgressBarDrawingInfo = new DrawingInfo();
            toolbarContainer.getProgressBarDrawingInfo(mProgressBarDrawingInfo);
        } else {
            assert mProgressBarDrawingInfo == null;
        }

        // Texture is always used unless it is completely off-screen.
        boolean useTexture = !fullscreenManager.areBrowserControlsOffScreen()
                && viewportMode != ViewportMode.ALWAYS_FULLSCREEN;
        boolean showShadow = fullscreenManager.drawControlsAsTexture()
                || forceHideAndroidBrowserControls;

        int textBoxColor =
                ToolbarColors.getTextBoxColorForToolbarBackground(mContext.getResources(),
                        fullscreenManager.getTab(), browserControlsBackgroundColor);

        int textBoxResourceId = R.drawable.modern_location_bar;
        ToolbarSceneLayerJni.get().updateToolbarLayer(mNativePtr, ToolbarSceneLayer.this,
                resourceManager, R.id.control_container, browserControlsBackgroundColor,
                textBoxResourceId, browserControlsUrlBarAlpha, textBoxColor,
                fullscreenManager.getTopControlOffset() + mStaticYOffset, windowHeight, useTexture,
                showShadow);

        if (mProgressBarDrawingInfo == null) return;
        ToolbarSceneLayerJni.get().updateProgressBar(mNativePtr, ToolbarSceneLayer.this,
                mProgressBarDrawingInfo.progressBarRect.left,
                mProgressBarDrawingInfo.progressBarRect.top,
                mProgressBarDrawingInfo.progressBarRect.width(),
                mProgressBarDrawingInfo.progressBarRect.height(),
                mProgressBarDrawingInfo.progressBarColor,
                mProgressBarDrawingInfo.progressBarBackgroundRect.left,
                mProgressBarDrawingInfo.progressBarBackgroundRect.top,
                mProgressBarDrawingInfo.progressBarBackgroundRect.width(),
                mProgressBarDrawingInfo.progressBarBackgroundRect.height(),
                mProgressBarDrawingInfo.progressBarBackgroundColor);
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        ToolbarSceneLayerJni.get().setContentTree(mNativePtr, ToolbarSceneLayer.this, contentTree);
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = ToolbarSceneLayerJni.get().init(ToolbarSceneLayer.this);
        }
        assert mNativePtr != 0;
    }

    /**
     * Destroys this object and the corresponding native component.
     */
    @Override
    public void destroy() {
        super.destroy();
        mNativePtr = 0;
    }

    // SceneOverlay implementation.

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(RectF viewport, RectF visibleViewport,
            LayerTitleCache layerTitleCache, ResourceManager resourceManager, float yOffset) {
        boolean forceHideBrowserControlsAndroidView =
                mLayoutProvider.getActiveLayout().forceHideBrowserControlsAndroidView();
        @ViewportMode
        int viewportMode = mLayoutProvider.getActiveLayout().getViewportMode();

        // In Chrome modern design, the url bar is always opaque since it is drawn in the
        // compositor.
        float alpha = 1;

        update(mRenderHost.getBrowserControlsBackgroundColor(), alpha,
                mLayoutProvider.getFullscreenManager(), resourceManager,
                forceHideBrowserControlsAndroidView, viewportMode,
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext), viewport.height());

        return this;
    }

    @Override
    public boolean isSceneOverlayTreeShowing() {
        return true;
    }

    @Override
    public EventFilter getEventFilter() {
        return null;
    }

    @Override
    public void onSizeChanged(
            float width, float height, float visibleViewportOffsetY, int orientation) {}

    @Override
    public void getVirtualViews(List<VirtualView> views) {}

    @Override
    public boolean shouldHideAndroidBrowserControls() {
        return false;
    }

    @Override
    public boolean updateOverlay(long time, long dt) {
        return false;
    }

    @Override
    public boolean onBackPressed() {
        return false;
    }

    @Override
    public void onHideLayout() {}

    @Override
    public boolean handlesTabCreating() {
        return false;
    }

    @Override
    public void tabTitleChanged(int tabId, String title) {}

    @Override
    public void tabStateInitialized() {}

    @Override
    public void tabModelSwitched(boolean incognito) {}

    @Override
    public void tabCreated(long time, boolean incognito, int id, int prevId, boolean selected) {}

    @NativeMethods
    interface Natives {
        long init(ToolbarSceneLayer caller);
        void setContentTree(
                long nativeToolbarSceneLayer, ToolbarSceneLayer caller, SceneLayer contentTree);
        void updateToolbarLayer(long nativeToolbarSceneLayer, ToolbarSceneLayer caller,
                ResourceManager resourceManager, int resourceId, int toolbarBackgroundColor,
                int urlBarResourceId, float urlBarAlpha, int urlBarColor, float topOffset,
                float viewHeight, boolean visible, boolean showShadow);
        void updateProgressBar(long nativeToolbarSceneLayer, ToolbarSceneLayer caller,
                int progressBarX, int progressBarY, int progressBarWidth, int progressBarHeight,
                int progressBarColor, int progressBarBackgroundX, int progressBarBackgroundY,
                int progressBarBackgroundWidth, int progressBarBackgroundHeight,
                int progressBarBackgroundColor);
    }
}
