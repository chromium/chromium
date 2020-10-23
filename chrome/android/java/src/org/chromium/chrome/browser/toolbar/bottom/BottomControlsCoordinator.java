// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.annotation.SuppressLint;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupUi;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsViewBinder.ViewHolder;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
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
     * @param controlsSizer A {@link BrowserControlsSizer} to update the bottom controls
     *                          height for the renderer.
     * @param fullscreenManager A {@link FullscreenManager} to listen for fullscreen changes.
     * @param stub The bottom controls {@link ViewStub} to inflate.
     * @param tabProvider
     * @param themeColorProvider The {@link ThemeColorProvider} for the bottom toolbar.
     * @param shareDelegateSupplier The supplier for the {@link ShareDelegate} the bottom controls
     *         should use to share content.
     * @param showStartSurfaceCallable The action that opens the start surface, returning true if
     *         the start surface is shown.
     * @param openHomepageAction The action that opens the homepage.
     * @param setUrlBarFocusAction The function that sets Url bar focus. The first argument is
     *         whether the bar should be focused, and the second is the OmniboxFocusReason.
     * @param overviewModeBehaviorSupplier Supplier for the overview mode manager.
     * @param scrimCoordinator The {@link ScrimCoordinator} to control scrim view.
     * @param omniboxFocusStateSupplier Supplier to access the focus state of the omnibox.
     */
    @SuppressLint("CutPasteId") // Not actually cut and paste since it's View vs ViewGroup.
    public BottomControlsCoordinator(BrowserControlsSizer controlsSizer,
            FullscreenManager fullscreenManager, ViewStub stub, ActivityTabProvider tabProvider,
            ThemeColorProvider themeColorProvider,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            ObservableSupplier<AppMenuButtonHelper> menuButtonHelperSupplier,
            Supplier<Boolean> showStartSurfaceCallable, Runnable openHomepageAction,
            Callback<Integer> setUrlBarFocusAction,
            OneshotSupplier<OverviewModeBehavior> overviewModeBehaviorSupplier,
            ScrimCoordinator scrimCoordinator,
            ObservableSupplier<Boolean> omniboxFocusStateSupplier) {
        final ScrollingBottomViewResourceFrameLayout root =
                (ScrollingBottomViewResourceFrameLayout) stub.inflate();

        PropertyModel model = new PropertyModel(BottomControlsProperties.ALL_KEYS);

        PropertyModelChangeProcessor.create(
                model, new ViewHolder(root), BottomControlsViewBinder::bind);

        int bottomControlsHeightId = R.dimen.bottom_controls_height;

        View container = root.findViewById(R.id.bottom_container_slot);
        ViewGroup.LayoutParams params = container.getLayoutParams();
        params.height = root.getResources().getDimensionPixelOffset(bottomControlsHeightId);
        mMediator = new BottomControlsMediator(model, controlsSizer, fullscreenManager,
                root.getResources().getDimensionPixelOffset(bottomControlsHeightId));

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
        mMediator.setBottomControlsVisible(false);
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
     * @param windowAndroid A {@link WindowAndroid} for watching keyboard visibility events.
     * @param tabCountProvider Updates the tab count number in the tab switcher button and in the
     *                         incognito toggle tab layout.
     * @param incognitoStateProvider Notifies components when incognito mode is entered or exited.
     * @param topToolbarRoot The root {@link ViewGroup} of the top toolbar.
     * @param closeAllTabsAction The runnable that closes all tabs in the current tab model.
     */
    public void initializeWithNative(ChromeActivity chromeActivity, ResourceManager resourceManager,
            LayoutManager layoutManager, OnClickListener tabSwitcherListener,
            OnClickListener newTabClickListener, WindowAndroid windowAndroid,
            TabCountProvider tabCountProvider, IncognitoStateProvider incognitoStateProvider,
            ViewGroup topToolbarRoot, Runnable closeAllTabsAction) {
        mMediator.setLayoutManager(layoutManager);
        mMediator.setResourceManager(resourceManager);
        mMediator.setWindowAndroid(windowAndroid);

        if (mTabGroupUi != null) {
            mTabGroupUi.initializeWithNative(chromeActivity, mMediator::setBottomControlsVisible);
        }
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
