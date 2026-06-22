// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottombar;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.menu_button.MenuUiState;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.actions.appmenu.AppMenuActionProperties;
import org.chromium.chrome.browser.ui.actions.appmenu.MenuButtonState;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Controls the update badge for the app menu button in the bottom bar. */
// TODO(crbug.com/524729679): Share logic with toolbar.
@NullMarked
public class BottomBarAppMenuUpdateBadgeController implements Destroyable {
    private final NullableObservableSupplier<PropertyModel> mAppMenuActionSupplier;
    private final NullableObservableSupplier<Profile> mProfileSupplier;
    private final Runnable mMenuStateObserver = this::updateBadgeState;
    private final CallbackController mCallbackController = new CallbackController();
    private final Callback<@Nullable PropertyModel> mActionModelObserver =
            (actionModel) -> {
                if (actionModel != null) {
                    updateBadgeState();
                }
            };

    private final Callback<@Nullable Profile> mProfileObserver =
            new Callback<@Nullable Profile>() {
                @Override
                public void onResult(@Nullable Profile profile) {
                    if (mUpdateMenuItemHelper != null) {
                        mUpdateMenuItemHelper.unregisterObserver(mMenuStateObserver);
                        mUpdateMenuItemHelper = null;
                    }
                    if (profile == null) {
                        updateBadgeState();
                        return;
                    }
                    mUpdateMenuItemHelper =
                            UpdateMenuItemHelper.getInstance(profile.getOriginalProfile());
                    mUpdateMenuItemHelper.registerObserver(mMenuStateObserver);
                    updateBadgeState();
                }
            };

    private final AppMenuObserver mAppMenuObserver =
            new AppMenuObserver() {
                @Override
                public void onMenuVisibilityChanged(boolean isVisible) {
                    if (isVisible) {
                        dismissUpdateBadge();
                        if (mUpdateMenuItemHelper != null) {
                            mUpdateMenuItemHelper.onMenuButtonClicked();
                        }
                    }
                }

                @Override
                public void onMenuHighlightChanged(boolean highlighting) {}
            };

    private @Nullable UpdateMenuItemHelper mUpdateMenuItemHelper;
    private @Nullable AppMenuHandler mAppMenuHandler;

    /**
     * Creates a new {@link BottomBarAppMenuUpdateBadgeController}.
     *
     * @param actionRegistry The {@link ActionRegistry} used to retrieve the app menu action.
     * @param profileSupplier Supplier for the current {@link Profile}.
     * @param appMenuCoordinatorSupplier Supplier for the {@link AppMenuCoordinator}.
     */
    public BottomBarAppMenuUpdateBadgeController(
            ActionRegistry actionRegistry,
            NullableObservableSupplier<Profile> profileSupplier,
            OneshotSupplier<AppMenuCoordinator> appMenuCoordinatorSupplier) {
        mAppMenuActionSupplier = actionRegistry.get(ActionId.APP_MENU);
        mProfileSupplier = profileSupplier;

        mProfileSupplier.addSyncObserverAndPostIfNonNull(mProfileObserver);
        mAppMenuActionSupplier.addSyncObserver(mActionModelObserver);
        appMenuCoordinatorSupplier.onAvailable(
                mCallbackController.makeCancelable(
                        (appMenuCoordinator) -> {
                            mAppMenuHandler = appMenuCoordinator.getAppMenuHandler();
                            mAppMenuHandler.addObserver(mAppMenuObserver);
                        }));
    }

    private void updateBadgeState() {
        PropertyModel actionModel = mAppMenuActionSupplier.get();
        if (actionModel == null) return;

        boolean showBadge = false;
        @Nullable MenuButtonState buttonState = null;

        if (mUpdateMenuItemHelper != null) {
            MenuUiState uiState = mUpdateMenuItemHelper.getUiState();
            showBadge = uiState.buttonState != null;
            buttonState = uiState.buttonState;
        }

        if (mAppMenuHandler != null && mAppMenuHandler.isAppMenuShowing()) {
            showBadge = false;
        }

        boolean badgeChanged =
                actionModel.get(AppMenuActionProperties.SHOW_UPDATE_BADGE) != showBadge;
        boolean stateChanged =
                actionModel.get(AppMenuActionProperties.UPDATE_BADGE_BUTTON_STATE) != buttonState;

        if (!badgeChanged && !stateChanged) return;

        actionModel.set(AppMenuActionProperties.SHOW_UPDATE_BADGE, showBadge);
        actionModel.set(AppMenuActionProperties.UPDATE_BADGE_BUTTON_STATE, buttonState);

        int contentDescriptionId =
                showBadge && buttonState != null
                        ? buttonState.menuContentDescription
                        : R.string.accessibility_toolbar_btn_menu;
        actionModel.set(
                ActionProperties.CONTENT_DESCRIPTION_RESOLVER,
                (context) -> context.getString(contentDescriptionId));
    }

    private void dismissUpdateBadge() {
        PropertyModel actionModel = mAppMenuActionSupplier.get();
        if (actionModel == null || !actionModel.get(AppMenuActionProperties.SHOW_UPDATE_BADGE)) {
            return;
        }

        actionModel.set(AppMenuActionProperties.SHOW_UPDATE_BADGE, false);
        actionModel.set(
                ActionProperties.CONTENT_DESCRIPTION_RESOLVER,
                (context) -> context.getString(R.string.accessibility_toolbar_btn_menu));
    }

    @Override
    public void destroy() {
        mCallbackController.destroy();
        mProfileSupplier.removeObserver(mProfileObserver);
        if (mUpdateMenuItemHelper != null) {
            mUpdateMenuItemHelper.unregisterObserver(mMenuStateObserver);
            mUpdateMenuItemHelper = null;
        }
        mAppMenuActionSupplier.removeObserver(mActionModelObserver);
        if (mAppMenuHandler != null) {
            mAppMenuHandler.removeObserver(mAppMenuObserver);
            mAppMenuHandler = null;
        }
    }
}
