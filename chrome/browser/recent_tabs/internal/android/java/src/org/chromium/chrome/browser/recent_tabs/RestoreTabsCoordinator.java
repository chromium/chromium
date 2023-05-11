// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ViewFlipper;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsDetailScreenCoordinator;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsPromoScreenCoordinator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Coordinator to manage the Restore Tabs on FRE feature.
 */
public class RestoreTabsCoordinator {
    private RestoreTabsMediator mMediator;
    private PropertyModel mModel = RestoreTabsProperties.createDefaultModel();
    private RestoreTabsPromoSheetContent mContent;
    private BottomSheetController mBottomSheetController;
    private ViewFlipper mViewFlipperView;

    public RestoreTabsCoordinator(Context context, Profile profile,
            RestoreTabsControllerFactory.ControllerListener listener,
            TabCreatorManager tabCreatorManager, BottomSheetController bottomSheetController) {
        this(context, profile, new RestoreTabsMediator(), listener, tabCreatorManager,
                bottomSheetController);
    }

    protected RestoreTabsCoordinator(Context context, Profile profile, RestoreTabsMediator mediator,
            RestoreTabsControllerFactory.ControllerListener listener,
            TabCreatorManager tabCreatorManager, BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
        mMediator = mediator;
        mMediator.initialize(mModel, listener, profile, tabCreatorManager);

        View rootView = LayoutInflater.from(context).inflate(
                R.layout.restore_tabs_bottom_sheet, /*root=*/null);
        mContent = new RestoreTabsPromoSheetContent(rootView);

        View restoreTabsPromoScreenView =
                rootView.findViewById(R.id.restore_tabs_promo_screen_sheet);
        RestoreTabsPromoScreenCoordinator restoreTabsPromoScreenCoordinator =
                new RestoreTabsPromoScreenCoordinator(restoreTabsPromoScreenView, mModel);

        View detailScreenView = rootView.findViewById(R.id.restore_tabs_detail_screen_sheet);
        RestoreTabsDetailScreenCoordinator restoreTabsDetailScreenCoordinator =
                new RestoreTabsDetailScreenCoordinator(context, detailScreenView, mModel);

        mViewFlipperView =
                (ViewFlipper) rootView.findViewById(R.id.restore_tabs_bottom_sheet_view_flipper);
        mModel.addObserver((source, propertyKey) -> {
            if (RestoreTabsProperties.CURRENT_SCREEN == propertyKey) {
                mViewFlipperView.setDisplayedChild(getScreenIndexForScreenType(
                        mModel.get(RestoreTabsProperties.CURRENT_SCREEN)));
            }
        });
    }

    // Helper function to convert the screen type to an index for CURRENT_SCREEN.
    private static int getScreenIndexForScreenType(@ScreenType int screenType) {
        switch (screenType) {
            case ScreenType.HOME_SCREEN:
                return 0;
            // Both the device and review tabs selection screens are displayed on the detail screen.
            case ScreenType.DEVICE_SCREEN:
            case ScreenType.REVIEW_TABS_SCREEN:
                return 1;
        }
        assert false : "Undefined ScreenType: " + screenType;
        return 0;
    }

    public void destroy() {
        mMediator.destroy();
        mMediator = null;
    }

    public void showHomeScreen() {
        mMediator.showHomeScreen();
    }

    @VisibleForTesting
    PropertyModel getPropertyModelForTesting() {
        return mModel;
    }

    @VisibleForTesting
    ViewFlipper getViewFlipperForTesting() {
        return mViewFlipperView;
    }

    @VisibleForTesting
    View getContentViewForTesting() {
        return mContent.getContentView();
    }
}
