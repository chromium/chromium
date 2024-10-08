// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.content.Context;
import android.graphics.RectF;

import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.ui.resources.ResourceManager;

/**
 * The {@link LayoutManagerHost} usually is based on a {@link android.view.View}. This
 * implementation is stripped down with static sizes but still support 2 different orientations.
 */
class MockLayoutHost implements LayoutManagerHost, LayoutRenderHost {

    public static final int LAYOUT_HOST_PORTRAIT_WIDTH = 320; // dp
    public static final int LAYOUT_HOST_PORTRAIT_HEIGHT = 460; // dp

    private final Context mContext;
    private boolean mPortrait = true;
    private final BrowserControlsManager mBrowserControlsManager;

    MockLayoutHost(Context context) {
        mContext = context;
        mBrowserControlsManager =
                new BrowserControlsManager(null, BrowserControlsStateProvider.ControlsPosition.TOP);
    }

    public void setOrientation(boolean portrait) {
        mPortrait = portrait;
    }

    @Override
    public void requestRender() {}

    @Override
    public void onCompositorLayout() {}

    @Override
    public void didSwapFrame(int pendingFrameCount) {}

    @Override
    public void onSurfaceResized(int width, int height) {}

    @Override
    public Context getContext() {
        return mContext;
    }

    @Override
    public int getWidth() {
        final float density = mContext.getResources().getDisplayMetrics().density;
        final float size = mPortrait ? LAYOUT_HOST_PORTRAIT_WIDTH : LAYOUT_HOST_PORTRAIT_HEIGHT;
        return (int) (density * size);
    }

    @Override
    public int getHeight() {
        final float density = mContext.getResources().getDisplayMetrics().density;
        final float size = mPortrait ? LAYOUT_HOST_PORTRAIT_HEIGHT : LAYOUT_HOST_PORTRAIT_WIDTH;
        return (int) (density * size);
    }

    @Override
    public void getWindowViewport(RectF outRect) {
        outRect.set(0, 0, getWidth(), getHeight());
    }

    @Override
    public void getVisibleViewport(RectF outRect) {
        outRect.set(0, 0, getWidth(), getHeight());
    }

    @Override
    public void getViewportFullControls(RectF outRect) {
        outRect.set(0, 0, getWidth(), getHeight());
    }

    @Override
    public LayoutRenderHost getLayoutRenderHost() {
        return this;
    }

    @Override
    public void setContentOverlayVisibility(boolean visible, boolean canBeFocusable) {}

    @Override
    public BrowserControlsManager getBrowserControlsManager() {
        return mBrowserControlsManager;
    }

    @Override
    public FullscreenManager getFullscreenManager() {
        return null;
    }

    @Override
    public ResourceManager getResourceManager() {
        return null;
    }

    @Override
    public void invalidateAccessibilityProvider() {}

    @Override
    public void onContentChanged() {}

    @Override
    public void hideKeyboard(Runnable postHideTask) {}
}
