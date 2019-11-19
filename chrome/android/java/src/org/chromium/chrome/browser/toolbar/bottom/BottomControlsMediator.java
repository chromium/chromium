// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.compositor.layouts.ToolbarSwipeLayout;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EdgeSwipeHandler;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.ui.ImmersiveModeManager;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.resources.ResourceManager;

/**
 * This class is responsible for reacting to events from the outside world, interacting with other
 * coordinators, running most of the business logic associated with the bottom controls component,
 * and updating the model accordingly.
 */
class BottomControlsMediator implements ChromeFullscreenManager.FullscreenListener,
                                        KeyboardVisibilityDelegate.KeyboardVisibilityListener,
                                        SceneChangeObserver,
                                        OverlayPanelManager.OverlayPanelManagerObserver,
                                        ImmersiveModeManager.ImmersiveModeObserver {
    /** The model for the bottom controls component that holds all of its view state. */
    private final PropertyModel mModel;

    /** The fullscreen manager to observe browser controls events. */
    private final ChromeFullscreenManager mFullscreenManager;

    /**
     * The height of the bottom bar in pixels including any adjustments for immersive mode, but not
     * including the top shadow.
     */
    private int mBottomControlsHeight;

    /**
     * The base height of the bottom bar in pixels not including adjustments for immersive mode or
     * the top shadow.
     */
    private final int mBottomControlsBaseHeight;

    /**
     * The height of the bottom bar container (which includes the top shadow) in pixels not
     * including any offset for immersive mode.
     */
    private final int mBottomControlsContainerBaseHeight;

    /** A {@link WindowAndroid} for watching keyboard visibility events. */
    private WindowAndroid mWindowAndroid;

    /** The bottom controls visibility. */
    private boolean mIsBottomControlsVisible;

    /** Whether any overlay panel is showing. */
    private boolean mIsOverlayPanelShowing;

    /** Whether the swipe layout is currently active. */
    private boolean mIsInSwipeLayout;

    /** Whether the soft keyboard is visible. */
    private boolean mIsKeyboardVisible;

    /** The {@link ImmersiveModeManager} for the containing activity.*/
    private @Nullable ImmersiveModeManager mImmersiveModeManager;

    /**
     * Build a new mediator that handles events from outside the bottom controls component.
     * @param model The {@link BottomControlsProperties} that holds all the view state for the
     *         bottom controls component.
     * @param fullscreenManager A {@link ChromeFullscreenManager} for events related to the browser
     *                          controls.
     * @param bottomControlsHeight The height of the bottom bar in pixels.
     * @param bottomControlsContainerHeight The height of the bottom bar container in px. This
     *                                      should be the height of {@code bottomControlsHeight}
     *                                      plus the height of the top shadow.
     */
    BottomControlsMediator(PropertyModel model, ChromeFullscreenManager fullscreenManager,
            int bottomControlsHeight, int bottomControlsContainerHeight) {
        mModel = model;

        mFullscreenManager = fullscreenManager;
        mFullscreenManager.addListener(this);

        mBottomControlsBaseHeight = bottomControlsHeight;
        mBottomControlsHeight = mBottomControlsBaseHeight;
        mBottomControlsContainerBaseHeight = bottomControlsContainerHeight;
    }

    /**
     * @param swipeHandler The handler that controls the bottom toolbar's swipe behavior.
     */
    void setToolbarSwipeHandler(EdgeSwipeHandler swipeHandler) {
        mModel.set(BottomControlsProperties.TOOLBAR_SWIPE_HANDLER, swipeHandler);
    }

    void setResourceManager(ResourceManager resourceManager) {
        mModel.set(BottomControlsProperties.RESOURCE_MANAGER, resourceManager);
    }

    void setToolbarSwipeLayout(ToolbarSwipeLayout layout) {
        mModel.set(BottomControlsProperties.TOOLBAR_SWIPE_LAYOUT, layout);
    }

    void setWindowAndroid(WindowAndroid windowAndroid) {
        assert mWindowAndroid == null : "#setWindowAndroid should only be called once per toolbar.";
        // Watch for keyboard events so we can hide the bottom toolbar when the keyboard is showing.
        mWindowAndroid = windowAndroid;
        mWindowAndroid.getKeyboardDelegate().addKeyboardVisibilityListener(this);
    }

    void setLayoutManager(LayoutManager layoutManager) {
        mModel.set(BottomControlsProperties.LAYOUT_MANAGER, layoutManager);
        layoutManager.addSceneChangeObserver(this);
        layoutManager.getOverlayPanelManager().addObserver(this);
    }

    void setBottomControlsVisible(boolean visible) {
        mIsBottomControlsVisible = visible;
        updateCompositedViewVisibility();
        updateAndroidViewVisibility();
    }

    /**
     * @param immersiveModeManager The {@link ImmersiveModeManager} for the containing activity.
     */
    void setImmersiveModeManager(ImmersiveModeManager immersiveModeManager) {
        if (!immersiveModeManager.isImmersiveModeSupported()) return;

        mImmersiveModeManager = immersiveModeManager;
        mImmersiveModeManager.addObserver(this);

        if (mImmersiveModeManager.getBottomUiInsetPx() != 0) {
            onBottomUiInsetChanged(mImmersiveModeManager.getBottomUiInsetPx());
        }
    }

    /**
     * Clean up anything that needs to be when the bottom controls component is destroyed.
     */
    void destroy() {
        mFullscreenManager.removeListener(this);
        if (mWindowAndroid != null) {
            mWindowAndroid.getKeyboardDelegate().removeKeyboardVisibilityListener(this);
            mWindowAndroid = null;
        }
        if (mModel.get(BottomControlsProperties.LAYOUT_MANAGER) != null) {
            LayoutManager manager = mModel.get(BottomControlsProperties.LAYOUT_MANAGER);
            manager.getOverlayPanelManager().removeObserver(this);
            manager.removeSceneChangeObserver(this);
        }

        if (mImmersiveModeManager != null) mImmersiveModeManager.removeObserver(this);
    }

    @Override
    public void onContentOffsetChanged(int offset) {}

    @Override
    public void onControlsOffsetChanged(int topOffset, int bottomOffset, boolean needsAnimate) {
        mModel.set(BottomControlsProperties.Y_OFFSET, bottomOffset);
        updateAndroidViewVisibility();
    }

    @Override
    public void onToggleOverlayVideoMode(boolean enabled) {}

    @Override
    public void onBottomControlsHeightChanged(int bottomControlsHeight) {}

    @Override
    public void onOverlayPanelShown() {
        mIsOverlayPanelShowing = true;
        updateAndroidViewVisibility();
    }

    @Override
    public void onOverlayPanelHidden() {
        mIsOverlayPanelShowing = false;
        updateAndroidViewVisibility();
    }

    @Override
    public void keyboardVisibilityChanged(boolean isShowing) {
        mIsKeyboardVisible = isShowing;
        updateCompositedViewVisibility();
        updateAndroidViewVisibility();
    }

    @Override
    public void onTabSelectionHinted(int tabId) {}

    @Override
    public void onSceneChange(Layout layout) {
        mIsInSwipeLayout = layout instanceof ToolbarSwipeLayout;
        updateAndroidViewVisibility();
    }

    /**
     * @return Whether the browser is currently in fullscreen mode.
     */
    private boolean isInFullscreenMode() {
        return mFullscreenManager != null && mFullscreenManager.getPersistentFullscreenMode();
    }

    /**
     * The composited view is the composited version of the Android View. It is used to be able to
     * scroll the bottom controls off-screen synchronously. Since the bottom controls live below
     * the webcontents we re-size the webcontents through
     * {@link ChromeFullscreenManager#setBottomControlsHeight(int)} whenever the composited view
     * visibility changes.
     */
    private void updateCompositedViewVisibility() {
        final boolean isCompositedViewVisible =
                mIsBottomControlsVisible && !mIsKeyboardVisible && !isInFullscreenMode();
        mModel.set(BottomControlsProperties.COMPOSITED_VIEW_VISIBLE, isCompositedViewVisible);
        mFullscreenManager.setBottomControlsHeight(
                isCompositedViewVisible ? mBottomControlsHeight : 0);
    }

    /**
     * The Android View is the interactive view. The composited view should always be behind the
     * Android view which means we hide the Android view whenever the composited view is hidden.
     * We also hide the Android view as we are scrolling the bottom controls off screen this is
     * done by checking if {@link ChromeFullscreenManager#getBottomControlOffset()} is
     * non-zero.
     */
    private void updateAndroidViewVisibility() {
        mModel.set(BottomControlsProperties.ANDROID_VIEW_VISIBLE,
                mIsBottomControlsVisible && !mIsKeyboardVisible && !mIsOverlayPanelShowing
                        && !mIsInSwipeLayout && mFullscreenManager.getBottomControlOffset() == 0
                        && !isInFullscreenMode());
    }

    @Override
    public void onImmersiveModeChanged(boolean inImmersiveMode) {}

    @Override
    public void onBottomUiInsetChanged(int bottomUiInsetPx) {
        mBottomControlsHeight = mBottomControlsBaseHeight + bottomUiInsetPx;
        mModel.set(BottomControlsProperties.BOTTOM_CONTROLS_HEIGHT_PX, mBottomControlsHeight);
        mModel.set(BottomControlsProperties.BOTTOM_CONTROLS_CONTAINER_HEIGHT_PX,
                mBottomControlsContainerBaseHeight + bottomUiInsetPx);

        updateCompositedViewVisibility();
    }
}