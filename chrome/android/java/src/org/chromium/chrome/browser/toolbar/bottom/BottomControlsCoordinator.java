// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.ToolbarSwipeLayout;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupUi;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.MenuButton;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsViewBinder.ViewHolder;
import org.chromium.chrome.browser.ui.ImmersiveModeManager;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.ResourceManager;

/**
 * The root coordinator for the bottom controls component. This component is intended for use with
 * bottom UI that re-sizes the web contents, scrolls off-screen, and hides when the keyboard is
 * shown. This class has two primary components, an Android view and a composited texture that draws
 * when the controls are being scrolled off-screen. The Android version does not draw unless the
 * controls offset is 0.
 */
public class BottomControlsCoordinator {
    /**
     * Interface for the BottomControls component to hide and show itself.
     */
    public interface BottomControlsVisibilityController {
        void setBottomControlsVisible(boolean isVisible);
    }

    /** The mediator that handles events from outside the bottom controls. */
    private final BottomControlsMediator mMediator;

    /** The coordinator for the split toolbar's bottom toolbar component. */
    private @Nullable BottomToolbarCoordinator mBottomToolbarCoordinator;
    private @Nullable TabGroupUi mTabGroupUi;

    /**
     * Build the coordinator that manages the bottom controls.
     * @param fullscreenManager A {@link ChromeFullscreenManager} to update the bottom controls
     *                          height for the renderer.
     * @param stub The bottom controls {@link ViewStub} to inflate.
     * @param tabProvider The {@link ActivityTabProvider} used in the bottom toolbar.
     * @param homeButtonListener The {@link OnClickListener} for the bottom toolbar's home button.
     * @param searchAcceleratorListener The {@link OnClickListener} for the bottom toolbar's
     *                                  search accelerator.
     * @param shareButtonListener The {@link OnClickListener} for the bottom toolbar's share button.
     * @param themeColorProvider The {@link ThemeColorProvider} for the bottom toolbar.
     */
    public BottomControlsCoordinator(ChromeFullscreenManager fullscreenManager, ViewStub stub,
            ActivityTabProvider tabProvider, OnClickListener homeButtonListener,
            OnClickListener searchAcceleratorListener, OnClickListener shareButtonListener,
            OnLongClickListener tabSwitcherLongclickListener,
            ThemeColorProvider themeColorProvider) {
        final ScrollingBottomViewResourceFrameLayout root =
                (ScrollingBottomViewResourceFrameLayout) stub.inflate();

        PropertyModel model = new PropertyModel(BottomControlsProperties.ALL_KEYS);

        PropertyModelChangeProcessor.create(
                model, new ViewHolder(root), BottomControlsViewBinder::bind);

        int bottomToolbarHeightId;
        int bottomToolbarHeightWithShadowId;

        if (FeatureUtilities.isLabeledBottomToolbarEnabled()) {
            bottomToolbarHeightId = R.dimen.labeled_bottom_toolbar_height;
            bottomToolbarHeightWithShadowId = R.dimen.labeled_bottom_toolbar_height_with_shadow;
        } else {
            bottomToolbarHeightId = R.dimen.bottom_toolbar_height;
            bottomToolbarHeightWithShadowId = R.dimen.bottom_toolbar_height_with_shadow;
        }

        View toolbar = root.findViewById(R.id.bottom_container_slot);
        ViewGroup.LayoutParams params = toolbar.getLayoutParams();
        params.height = root.getResources().getDimensionPixelOffset(bottomToolbarHeightId);
        mMediator = new BottomControlsMediator(model, fullscreenManager,
                root.getResources().getDimensionPixelOffset(bottomToolbarHeightId),
                root.getResources().getDimensionPixelOffset(bottomToolbarHeightWithShadowId));

        if (TabManagementModuleProvider.getDelegate() != null
                && FeatureUtilities.isTabGroupsAndroidEnabled()) {
            mTabGroupUi = TabManagementModuleProvider.getDelegate().createTabGroupUi(
                    root.findViewById(R.id.bottom_container_slot), themeColorProvider);
        } else {
            mBottomToolbarCoordinator =
                    new BottomToolbarCoordinator(root.findViewById(R.id.bottom_toolbar_stub),
                            tabProvider, homeButtonListener, searchAcceleratorListener,
                            shareButtonListener, tabSwitcherLongclickListener, themeColorProvider);
        }
    }

    /**
     * @param immersiveModeManager The {@link ImmersiveModeManager} for the containing activity.
     */
    public void setImmersiveModeManager(ImmersiveModeManager immersiveModeManager) {
        mMediator.setImmersiveModeManager(immersiveModeManager);
    }

    /**
     * Initialize the bottom controls with the components that had native initialization
     * dependencies.
     * <p>
     * Calling this must occur after the native library have completely loaded.
     * @param chromeActivity ChromeActivity instance to use.
     * @param resourceManager A {@link ResourceManager} for loading textures into the compositor.
     * @param layoutManager A {@link LayoutManager} to attach overlays to.
     * @param tabSwitcherListener An {@link OnClickListener} that is triggered when the
     *                            bottom toolbar's tab switcher button is clicked.
     * @param newTabClickListener An {@link OnClickListener} that is triggered when the
     *                            bottom toolbar's new tab button is clicked.
     * @param menuButtonHelper An {@link AppMenuButtonHelper} that is triggered when the
     *                         bottom toolbar's menu button is clicked.
     * @param overviewModeBehavior The overview mode manager.
     * @param windowAndroid A {@link WindowAndroid} for watching keyboard visibility events.
     * @param tabCountProvider Updates the tab count number in the tab switcher button and in the
     *                         incognito toggle tab layout.
     * @param incognitoStateProvider Notifies components when incognito mode is entered or exited.
     * @param topToolbarRoot The root {@link ViewGroup} of the top toolbar.
     */
    public void initializeWithNative(ChromeActivity chromeActivity, ResourceManager resourceManager,
            LayoutManager layoutManager, OnClickListener tabSwitcherListener,
            OnClickListener newTabClickListener, OnClickListener closeTabsClickListener,
            AppMenuButtonHelper menuButtonHelper, OverviewModeBehavior overviewModeBehavior,
            WindowAndroid windowAndroid, TabCountProvider tabCountProvider,
            IncognitoStateProvider incognitoStateProvider, ViewGroup topToolbarRoot) {
        mMediator.setLayoutManager(layoutManager);
        mMediator.setResourceManager(resourceManager);
        mMediator.setWindowAndroid(windowAndroid);

        if (mBottomToolbarCoordinator != null) {
            mBottomToolbarCoordinator.initializeWithNative(tabSwitcherListener, newTabClickListener,
                    closeTabsClickListener, menuButtonHelper, overviewModeBehavior,
                    tabCountProvider, incognitoStateProvider, topToolbarRoot);
            mMediator.setToolbarSwipeHandler(
                    layoutManager.createToolbarSwipeHandler(/* supportSwipeDown = */ false));
        }

        if (mTabGroupUi != null) {
            mTabGroupUi.initializeWithNative(chromeActivity, mMediator::setBottomControlsVisible);
        }
    }

    /**
     * @param isVisible Whether the bottom control is visible.
     */
    public void setBottomControlsVisible(boolean isVisible) {
        mMediator.setBottomControlsVisible(isVisible);
        if (mBottomToolbarCoordinator != null) {
            mBottomToolbarCoordinator.setBottomToolbarVisible(isVisible);
        }
    }

    /**
     * Show the update badge over the bottom toolbar's app menu.
     */
    public void showAppMenuUpdateBadge() {
        if (mBottomToolbarCoordinator != null) {
            mBottomToolbarCoordinator.showAppMenuUpdateBadge();
        }
    }

    /**
     * Remove the update badge.
     */
    public void removeAppMenuUpdateBadge() {
        if (mBottomToolbarCoordinator != null) {
            mBottomToolbarCoordinator.removeAppMenuUpdateBadge();
        }
    }

    /**
     * @return Whether the update badge is showing.
     */
    public boolean isShowingAppMenuUpdateBadge() {
        if (mBottomToolbarCoordinator != null) {
            return mBottomToolbarCoordinator.isShowingAppMenuUpdateBadge();
        }
        return false;
    }

    /**
     * @return The wrapper for the browsing mode toolbar's app menu button.
     */
    public MenuButton getMenuButtonWrapper() {
        if (mBottomToolbarCoordinator != null) {
            return mBottomToolbarCoordinator.getMenuButtonWrapper();
        }
        return null;
    }

    /**
     * Handles system back press action if needed.
     * @return Whether or not the back press event is consumed here.
     */
    public boolean onBackPressed() {
        return mTabGroupUi != null && mTabGroupUi.onBackPressed();
    }

    /**
     * @param layout The {@link ToolbarSwipeLayout} that the bottom controls will hook into. This
     *               allows the bottom controls to provide the layout with scene layers with the
     *               bottom controls' texture.
     */
    public void setToolbarSwipeLayout(ToolbarSwipeLayout layout) {
        mMediator.setToolbarSwipeLayout(layout);
    }

    /**
     * Clean up any state when the bottom controls component is destroyed.
     */
    public void destroy() {
        if (mBottomToolbarCoordinator != null) mBottomToolbarCoordinator.destroy();
        if (mTabGroupUi != null) mTabGroupUi.destroy();
        mMediator.destroy();
    }
}
