// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupUi;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsViewBinder.ViewHolder;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.widget.Toast;

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
    private @Nullable TabGroupUi mTabGroupUi;

    /**
     * Build the coordinator that manages the bottom controls.
     * @param activity Activity instance to use.
     * @param windowAndroid A {@link WindowAndroid} for watching keyboard visibility events.
     * @param controlsSizer A {@link BrowserControlsSizer} to update the bottom controls
     *                          height for the renderer.
     * @param fullscreenManager A {@link FullscreenManager} to listen for fullscreen changes.
     * @param stub The bottom controls {@link ViewStub} to inflate.
     * @param themeColorProvider The {@link ThemeColorProvider} for the bottom toolbar.
     * @param shareDelegateSupplier The supplier for the {@link ShareDelegate} the bottom controls
     *         should use to share content.
     * @param scrimCoordinator The {@link ScrimCoordinator} to control scrim view.
     * @param omniboxFocusStateSupplier Supplier to access the focus state of the omnibox.
     * @param overlayPanelVisibilitySupplier Notifies overlay panel visibility event.
     * @param resourceManager A {@link ResourceManager} for loading textures into the compositor.
     * @param layoutManager A {@link LayoutManagerImpl} to attach overlays to.
     */
    @SuppressLint("CutPasteId") // Not actually cut and paste since it's View vs ViewGroup.
    public BottomControlsCoordinator(Activity activity, WindowAndroid windowAndroid,
            LayoutManager layoutManager, ResourceManager resourceManager,
            BrowserControlsSizer controlsSizer, FullscreenManager fullscreenManager, ViewStub stub,
            ThemeColorProvider themeColorProvider,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            ScrimCoordinator scrimCoordinator,
            ObservableSupplier<Boolean> omniboxFocusStateSupplier,
            ObservableSupplier<Boolean> overlayPanelVisibilitySupplier) {
        final ScrollingBottomViewResourceFrameLayout root =
                (ScrollingBottomViewResourceFrameLayout) stub.inflate();

        PropertyModel model = new PropertyModel(BottomControlsProperties.ALL_KEYS);

        ScrollingBottomViewSceneLayer sceneLayer =
                new ScrollingBottomViewSceneLayer(root, root.getTopShadowHeight());
        PropertyModelChangeProcessor.create(
                model, new ViewHolder(root, sceneLayer), BottomControlsViewBinder::bind);
        layoutManager.createCompositorMCP(
                model, sceneLayer, BottomControlsViewBinder::bindCompositorMCP);
        int bottomControlsHeightId = R.dimen.bottom_controls_height;

        View container = root.findViewById(R.id.bottom_container_slot);
        ViewGroup.LayoutParams params = container.getLayoutParams();
        params.height = root.getResources().getDimensionPixelOffset(bottomControlsHeightId);
        mMediator =
                new BottomControlsMediator(windowAndroid, model, controlsSizer, fullscreenManager,
                        root.getResources().getDimensionPixelOffset(bottomControlsHeightId),
                        overlayPanelVisibilitySupplier);

        resourceManager.getDynamicResourceLoader().registerResource(
                root.getId(), root.getResourceAdapter());

        if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled()
                || TabUiFeatureUtilities.isConditionalTabStripEnabled()) {
            mTabGroupUi = TabManagementModuleProvider.getDelegate().createTabGroupUi(
                    root.findViewById(R.id.bottom_container_slot), themeColorProvider,
                    scrimCoordinator, omniboxFocusStateSupplier);
        }
        Toast.setGlobalExtraYOffset(
                root.getResources().getDimensionPixelSize(bottomControlsHeightId));

        // Set the visibility of BottomControls to false by default. Components within
        // BottomControls should update the visibility explicitly if needed.
        setBottomControlsVisible(false);

        sceneLayer.setIsVisible(mMediator.isCompositedViewVisible());
        layoutManager.addSceneOverlay(sceneLayer);

        if (mTabGroupUi != null) {
            mTabGroupUi.initializeWithNative(activity, mMediator::setBottomControlsVisible);
        }
    }

    /**
     * @param layoutStateProvider {@link LayoutStateProvider} object.
     */
    public void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        mMediator.setLayoutStateProvider(layoutStateProvider);
    }

    /**
     * @param isVisible Whether the bottom control is visible.
     */
    public void setBottomControlsVisible(boolean isVisible) {
        mMediator.setBottomControlsVisible(isVisible);
    }

    /**
     * Handles system back press action if needed.
     * @return Whether or not the back press event is consumed here.
     */
    public boolean onBackPressed() {
        return mTabGroupUi != null && mTabGroupUi.onBackPressed();
    }

    /**
     * Clean up any state when the bottom controls component is destroyed.
     */
    public void destroy() {
        if (mTabGroupUi != null) mTabGroupUi.destroy();
        mMediator.destroy();
    }

    /**
     * @return {@link Supplier} that provides dialog visibility.
     */
    public Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        if (mTabGroupUi == null) {
            return null;
        }
        return mTabGroupUi.getTabGridDialogVisibilitySupplier();
    }
}
