// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout.LayoutParams;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ValueChangedCallback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController.MenuOrKeyboardActionHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.ui.util.TokenHolder;

/**
 * Implementation of {@link HubManager} and {@link HubController}.
 *
 * <p>This class holds all the dependencies of {@link HubCoordinator} so that the Hub UI can be
 * created and torn down as needed when {@link HubLayout} visibility changes.
 */
public class HubManagerImpl implements HubManager, HubController {
    private final ValueChangedCallback<Pane> mOnFocusedPaneChanged =
            new ValueChangedCallback<>(this::onFocusedPaneChanged);
    private final @NonNull ObservableSupplierImpl<Boolean> mHubVisibilitySupplier =
            new ObservableSupplierImpl<>();
    private final @NonNull Activity mActivity;
    private final @NonNull OneshotSupplier<ProfileProvider> mProfileProviderSupplier;
    private final @NonNull PaneManagerImpl mPaneManager;
    private final @NonNull HubContainerView mHubContainerView;
    private final @NonNull BackPressManager mBackPressManager;
    private final @NonNull MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    private final @NonNull SnackbarManager mSnackbarManager;
    private final @NonNull ObservableSupplier<Tab> mTabSupplier;
    private final @NonNull MenuButtonCoordinator mMenuButtonCoordinator;
    private final @NonNull HubShowPaneHelper mHubShowPaneHelper;
    private final @NonNull ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier;
    private final @NonNull SearchActivityClient mSearchActivityClient;

    // This is effectively NonNull and final once the HubLayout is initialized.
    private HubLayoutController mHubLayoutController;
    private HubCoordinator mHubCoordinator;
    private int mSnackbarOverrideToken;
    private int mStatusIndicatorHeight;
    private int mAppHeaderHeight;

    /** See {@link HubManagerFactory#createHubManager}. */
    public HubManagerImpl(
            @NonNull Activity activity,
            @NonNull OneshotSupplier<ProfileProvider> profileProviderSupplier,
            @NonNull PaneListBuilder paneListBuilder,
            @NonNull BackPressManager backPressManager,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull SnackbarManager snackbarManager,
            @NonNull ObservableSupplier<Tab> tabSupplier,
            @NonNull MenuButtonCoordinator menuButtonCoordinator,
            @NonNull HubShowPaneHelper hubShowPaneHelper,
            @NonNull ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            @NonNull SearchActivityClient searchActivityClient) {
        mActivity = activity;
        mProfileProviderSupplier = profileProviderSupplier;
        mPaneManager = new PaneManagerImpl(paneListBuilder, mHubVisibilitySupplier);
        mBackPressManager = backPressManager;
        mMenuOrKeyboardActionController = menuOrKeyboardActionController;
        mSnackbarManager = snackbarManager;
        mTabSupplier = tabSupplier;
        mMenuButtonCoordinator = menuButtonCoordinator;
        mHubShowPaneHelper = hubShowPaneHelper;
        mEdgeToEdgeSupplier = edgeToEdgeSupplier;
        mSearchActivityClient = searchActivityClient;

        // TODO(crbug.com/40283238): Consider making this a xml file so the entire core UI is
        // inflated.
        mHubContainerView = new HubContainerView(mActivity);
        LayoutParams params =
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        mHubContainerView.setLayoutParams(params);

        mPaneManager.getFocusedPaneSupplier().addObserver(mOnFocusedPaneChanged);
    }

    @Override
    public void destroy() {
        mHubVisibilitySupplier.set(false);
        mPaneManager.getFocusedPaneSupplier().removeObserver(mOnFocusedPaneChanged);
        mPaneManager.destroy();
        destroyHubCoordinator();
    }

    @Override
    public @NonNull PaneManager getPaneManager() {
        return mPaneManager;
    }

    @Override
    public @NonNull HubController getHubController() {
        return this;
    }

    @Override
    public @NonNull ObservableSupplier<Boolean> getHubVisibilitySupplier() {
        return mHubVisibilitySupplier;
    }

    @Override
    public @NonNull HubShowPaneHelper getHubShowPaneHelper() {
        return mHubShowPaneHelper;
    }

    @Override
    public void setStatusIndicatorHeight(int height) {
        LayoutParams params = (LayoutParams) mHubContainerView.getLayoutParams();
        assert params != null : "HubContainerView should always have layout params.";
        mStatusIndicatorHeight = height;
        params.topMargin = mStatusIndicatorHeight + mAppHeaderHeight;
        mHubContainerView.setLayoutParams(params);
    }

    @Override
    public void setAppHeaderHeight(int height) {
        if (mAppHeaderHeight == height) return;
        LayoutParams params = (LayoutParams) mHubContainerView.getLayoutParams();
        assert params != null : "HubContainerView should always have layout params.";
        mAppHeaderHeight = height;
        params.topMargin = mStatusIndicatorHeight + mAppHeaderHeight;
        mHubContainerView.setLayoutParams(params);
    }

    @Override
    public void setHubLayoutController(@NonNull HubLayoutController hubLayoutController) {
        assert mHubLayoutController == null : "setHubLayoutController should only be called once.";
        mHubLayoutController = hubLayoutController;
    }

    @Override
    public @NonNull HubContainerView getContainerView() {
        assert mHubCoordinator != null : "Access of a HubContainerView with no descendants.";
        return mHubContainerView;
    }

    @Override
    public @Nullable View getPaneHostView() {
        assert mHubCoordinator != null : "Access of a Hub pane host view that doesn't exist";
        return mHubContainerView.findViewById(R.id.hub_pane_host);
    }

    @Override
    public @ColorInt int getBackgroundColor(@Nullable Pane pane) {
        @HubColorScheme int colorScheme = HubColors.getColorSchemeSafe(pane);
        return HubColors.getBackgroundColor(mActivity, colorScheme);
    }

    @Override
    public void onHubLayoutShow() {
        mHubVisibilitySupplier.set(true);
        ensureHubCoordinatorIsInitialized();
    }

    @Override
    public void onHubLayoutDoneHiding() {
        // TODO(crbug.com/40283238): Consider deferring this destruction till after a timeout.
        mHubContainerView.removeAllViews();
        mHubVisibilitySupplier.set(false);
        destroyHubCoordinator();
    }

    @Override
    public boolean onHubLayoutBackPressed() {
        if (mHubCoordinator == null) return false;

        switch (mHubCoordinator.handleBackPress()) {
            case BackPressResult.SUCCESS:
                return true;
            case BackPressResult.FAILURE:
                return false;
            default:
                assert false : "Not reached.";
                return false;
        }
    }

    private void ensureHubCoordinatorIsInitialized() {
        if (mHubCoordinator != null) return;

        assert mHubLayoutController != null
                : "HubLayoutController should be set before creating HubCoordinator.";

        mHubCoordinator =
                new HubCoordinator(
                        mActivity,
                        mProfileProviderSupplier,
                        mHubContainerView,
                        mPaneManager,
                        mHubLayoutController,
                        mTabSupplier,
                        mMenuButtonCoordinator,
                        mEdgeToEdgeSupplier,
                        mSearchActivityClient);
        mBackPressManager.addHandler(mHubCoordinator, BackPressHandler.Type.HUB);
        Pane pane = mPaneManager.getFocusedPaneSupplier().get();
        attachPaneDependencies(pane);
    }

    private void destroyHubCoordinator() {
        if (mHubCoordinator != null) {
            Pane pane = mPaneManager.getFocusedPaneSupplier().get();
            detachPaneDependencies(pane);

            mBackPressManager.removeHandler(mHubCoordinator);
            mHubCoordinator.destroy();
            mHubCoordinator = null;
        }
    }

    HubCoordinator getHubCoordinatorForTesting() {
        return mHubCoordinator;
    }

    private void onFocusedPaneChanged(@Nullable Pane newPane, @Nullable Pane oldPane) {
        detachPaneDependencies(oldPane);
        if (mHubCoordinator != null) {
            attachPaneDependencies(newPane);
        }
    }

    private void detachPaneDependencies(@Nullable Pane pane) {
        if (pane == null) return;

        pane.setPaneHubController(null);
        MenuOrKeyboardActionHandler menuOrKeyboardActionHandler =
                pane.getMenuOrKeyboardActionHandler();
        if (menuOrKeyboardActionHandler != null) {
            mMenuOrKeyboardActionController.unregisterMenuOrKeyboardActionHandler(
                    menuOrKeyboardActionHandler);
        }
        if (mSnackbarOverrideToken != TokenHolder.INVALID_TOKEN) {
            mSnackbarManager.popParentViewFromOverrideStack(mSnackbarOverrideToken);
            mSnackbarOverrideToken = TokenHolder.INVALID_TOKEN;
        }
    }

    private void attachPaneDependencies(@Nullable Pane pane) {
        if (pane == null) return;

        pane.setPaneHubController(mHubCoordinator);
        MenuOrKeyboardActionHandler menuOrKeyboardActionHandler =
                pane.getMenuOrKeyboardActionHandler();
        if (menuOrKeyboardActionHandler != null) {
            mMenuOrKeyboardActionController.registerMenuOrKeyboardActionHandler(
                    menuOrKeyboardActionHandler);
        }
        mSnackbarOverrideToken =
                mSnackbarManager.pushParentViewToOverrideStack(
                        mHubCoordinator.getSnackbarContainer());
    }
}
