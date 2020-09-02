// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.menu_button;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuObserver;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.ui.UiUtils;
import org.chromium.ui.util.TokenHolder;

/**
 * Root component for the app menu button on the toolbar. Owns the MenuButton view and handles
 * changes to its visual state, e.g. showing/hiding the app update badge.
 */
public class MenuButtonCoordinator implements AppMenuObserver {
    public interface SetFocusFunction {
        void setFocus(boolean focus, int reason);
    }

    private ObservableSupplier<AppMenuCoordinator> mAppMenuCoordinatorSupplier;
    private Callback<AppMenuCoordinator> mAppMenuCoordinatorSupplierObserver;
    private @Nullable AppMenuPropertiesDelegate mAppMenuPropertiesDelegate;
    private AppMenuButtonHelper mAppMenuButtonHelper;
    private ObservableSupplierImpl<AppMenuButtonHelper> mAppMenuButtonHelperSupplier;
    private AppMenuHandler mAppMenuHandler;
    private final BrowserStateBrowserControlsVisibilityDelegate mControlsVisibilityDelegate;
    private final Activity mActivity;
    private int mFullscreenMenuToken = TokenHolder.INVALID_TOKEN;
    private int mFullscreenHighlightToken = TokenHolder.INVALID_TOKEN;
    private final SetFocusFunction mSetUrlBarFocusFunction;
    private Runnable mRequestRenderRunnable;
    private Runnable mUpdateStateChangedListener;
    private final boolean mShouldShowAppUpdateBadge;
    private Supplier<Boolean> mIsInOverviewModeSupplier;
    private MenuButton mMenuButton;

    /**
     *
     * @param appMenuCoordinatorSupplier Supplier for the AppMenuCoordinator, which owns all other
     *         app menu MVC components.
     * @param controlsVisibilityDelegate Delegate for forcing persistent display of browser
     *         controls.
     * @param activity Activity in which this object lives.
     * @param setUrlBarFocusFunction Function that allows setting focus on the url bar.
     * @param requestRenderRunnable Runnable that requests a re-rendering of the compositor view
     *         containing the app menu button.
     * @param shouldShowAppUpdateBadge Whether the app menu update badge should be shown if there is
     *         a pending update.
     * @param isInOverviewModeSupplier Supplier of overview mode state.
     * @param menuButton View that presents the MenuButton.
     */
    public MenuButtonCoordinator(ObservableSupplier<AppMenuCoordinator> appMenuCoordinatorSupplier,
            BrowserStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate,
            Activity activity, SetFocusFunction setUrlBarFocusFunction,
            Runnable requestRenderRunnable, boolean shouldShowAppUpdateBadge,
            Supplier<Boolean> isInOverviewModeSupplier, MenuButton menuButton) {
        mControlsVisibilityDelegate = controlsVisibilityDelegate;
        mActivity = activity;
        mSetUrlBarFocusFunction = setUrlBarFocusFunction;
        mAppMenuCoordinatorSupplier = appMenuCoordinatorSupplier;
        mAppMenuCoordinatorSupplierObserver = this::onAppMenuInitialized;
        appMenuCoordinatorSupplier.addObserver(mAppMenuCoordinatorSupplierObserver);
        mRequestRenderRunnable = requestRenderRunnable;
        mShouldShowAppUpdateBadge = shouldShowAppUpdateBadge;
        mIsInOverviewModeSupplier = isInOverviewModeSupplier;
        mMenuButton = menuButton;
        mAppMenuButtonHelperSupplier = new ObservableSupplierImpl<>();
    }

    /**
     * Update the state of AppMenu components that need to know if the current page is loading, e.g.
     * the stop/reload button.
     * @param isLoading Whether the current page is loading.
     */
    public void updateReloadingState(boolean isLoading) {
        if (mMenuButton == null || mAppMenuPropertiesDelegate == null || mAppMenuHandler == null) {
            return;
        }
        mAppMenuPropertiesDelegate.loadingStateChanged(isLoading);
        mAppMenuHandler.menuItemContentChanged(R.id.icon_row_menu_id);
    }

    /**
     * Disables the menu button, removing it from the view hierarchy and destroying it.
     */
    public void disableMenuButton() {
        if (mMenuButton != null) {
            UiUtils.removeViewFromParent(mMenuButton);
            destroy();
        }
    }

    public void destroy() {
        if (mAppMenuButtonHelper != null) {
            mAppMenuHandler.removeObserver(this);
            mAppMenuButtonHelper = null;
        }

        if (mUpdateStateChangedListener != null) {
            UpdateMenuItemHelper.getInstance().unregisterObserver(mUpdateStateChangedListener);
            mUpdateStateChangedListener = null;
        }

        if (mMenuButton != null) {
            mMenuButton.destroy();
            mMenuButton = null;
        }
    }

    /**
     * Signal to MenuButtonCoordinator that native is initialized and it's safe to access
     * dependencies that require native, e.g. the UpdateMenuItemHelper.
     */
    public void onNativeInitialized() {
        if (mShouldShowAppUpdateBadge) {
            mUpdateStateChangedListener = this::updateStateChanged;
            UpdateMenuItemHelper.getInstance().registerObserver(mUpdateStateChangedListener);
        }
    }

    @Nullable
    public ObservableSupplier<AppMenuButtonHelper> getMenuButtonHelperSupplier() {
        return mAppMenuButtonHelperSupplier;
    }

    /**
     * Suppress or un-suppress display of the "update available" badge.
     * @param isSuppressed
     */
    public void setAppMenuUpdateBadgeSuppressed(boolean isSuppressed) {
        if (mMenuButton == null) return;
        mMenuButton.setAppMenuUpdateBadgeSuppressed(isSuppressed);
    }

    @Override
    public void onMenuVisibilityChanged(boolean isVisible) {
        if (isVisible) {
            // Defocus here to avoid handling focus in multiple places, e.g., when the
            // forward button is pressed. (see crbug.com/414219)
            mSetUrlBarFocusFunction.setFocus(false, LocationBar.OmniboxFocusReason.UNFOCUS);

            if (!mIsInOverviewModeSupplier.get() && isShowingAppMenuUpdateBadge()) {
                // The app menu badge should be removed the first time the menu is opened.
                mMenuButton.removeAppMenuUpdateBadge(true);
                mRequestRenderRunnable.run();
            }

            mFullscreenMenuToken =
                    mControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                            mFullscreenMenuToken);
        } else {
            mControlsVisibilityDelegate.releasePersistentShowingToken(mFullscreenMenuToken);
        }

        if (isVisible && mMenuButton != null && mMenuButton.isShowingAppMenuUpdateBadge()) {
            UpdateMenuItemHelper.getInstance().onMenuButtonClicked();
        }
    }

    @Override
    public void onMenuHighlightChanged(boolean isHighlighting) {
        if (mMenuButton != null) mMenuButton.setMenuButtonHighlight(isHighlighting);

        if (isHighlighting) {
            mFullscreenHighlightToken =
                    mControlsVisibilityDelegate.showControlsPersistentAndClearOldToken(
                            mFullscreenHighlightToken);
        } else {
            mControlsVisibilityDelegate.releasePersistentShowingToken(mFullscreenHighlightToken);
        }
    }

    /**
     * Called when the app menu and related properties delegate are available.
     *
     * @param appMenuCoordinator The coordinator for interacting with the menu.
     */
    private void onAppMenuInitialized(AppMenuCoordinator appMenuCoordinator) {
        assert mAppMenuHandler == null;
        AppMenuHandler appMenuHandler = appMenuCoordinator.getAppMenuHandler();

        mAppMenuHandler = appMenuHandler;
        mAppMenuHandler.addObserver(this);
        mAppMenuButtonHelper = mAppMenuHandler.createAppMenuButtonHelper();
        mAppMenuButtonHelper.setOnAppMenuShownListener(
                () -> { RecordUserAction.record("MobileToolbarShowMenu"); });
        if (mMenuButton != null) {
            mMenuButton.setAppMenuButtonHelper(mAppMenuButtonHelper);
        }

        mAppMenuButtonHelperSupplier.set(mAppMenuButtonHelper);
        mAppMenuPropertiesDelegate = appMenuCoordinator.getAppMenuPropertiesDelegate();

        // TODO(pnoland, https://crbug.com/1084528): replace this with a one shot supplier so we can
        // express that we don't handle the menu coordinator being set more than once.
        mAppMenuCoordinatorSupplier.removeObserver(mAppMenuCoordinatorSupplierObserver);
        mAppMenuCoordinatorSupplier = null;
        mAppMenuCoordinatorSupplierObserver = null;
    }

    /**
     * @return Whether the badge is showing (either in the toolbar).
     */
    private boolean isShowingAppMenuUpdateBadge() {
        return mMenuButton != null && mMenuButton.isShowingAppMenuUpdateBadge();
    }

    @VisibleForTesting
    void updateStateChanged() {
        if (mMenuButton == null || mActivity.isFinishing() || mActivity.isDestroyed()
                || !mShouldShowAppUpdateBadge) {
            return;
        }

        UpdateMenuItemHelper.MenuButtonState buttonState =
                UpdateMenuItemHelper.getInstance().getUiState().buttonState;
        if (buttonState != null) {
            mMenuButton.showAppMenuUpdateBadgeIfAvailable(true);
            mRequestRenderRunnable.run();
        } else {
            mMenuButton.removeAppMenuUpdateBadge(false);
        }
    }
}
