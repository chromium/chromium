// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ViewFlipper;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsDetailScreenCoordinator;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsPromoScreenCoordinator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Coordinator to manage the Restore Tabs on FRE feature. */
public class RestoreTabsCoordinator {
    private RestoreTabsMediator mMediator;
    private PropertyModel mModel = RestoreTabsProperties.createDefaultModel();
    private RestoreTabsPromoSheetContent mContent;
    private ViewFlipper mViewFlipperView;
    private RestoreTabsDetailScreenCoordinator mRestoreTabsDetailScreenCoordinator;

    public RestoreTabsCoordinator(
            Context context,
            Profile profile,
            TabCreatorManager tabCreatorManager,
            BottomSheetController bottomSheetController) {
        this(context, profile, new RestoreTabsMediator(), tabCreatorManager, bottomSheetController);
    }

    protected RestoreTabsCoordinator(
            Context context,
            Profile profile,
            RestoreTabsMediator mediator,
            TabCreatorManager tabCreatorManager,
            BottomSheetController bottomSheetController) {
        mMediator = mediator;
        mMediator.initialize(mModel, profile, tabCreatorManager, bottomSheetController);

        View rootView =
                LayoutInflater.from(context)
                        .inflate(R.layout.restore_tabs_bottom_sheet, /* root= */ null);
        mContent = new RestoreTabsPromoSheetContent(rootView, mModel, bottomSheetController);

        View restoreTabsPromoScreenView =
                rootView.findViewById(R.id.restore_tabs_promo_screen_sheet);
        new RestoreTabsPromoScreenCoordinator(restoreTabsPromoScreenView, mModel);

        View detailScreenView = rootView.findViewById(R.id.restore_tabs_detail_screen_sheet);
        mRestoreTabsDetailScreenCoordinator =
                new RestoreTabsDetailScreenCoordinator(context, detailScreenView, mModel, profile);

        mViewFlipperView =
                (ViewFlipper) rootView.findViewById(R.id.restore_tabs_bottom_sheet_view_flipper);
        mModel.addObserver(
                (source, propertyKey) -> {
                    if (RestoreTabsProperties.CURRENT_SCREEN == propertyKey) {
                        mViewFlipperView.setDisplayedChild(
                                getScreenIndexForScreenType(
                                        mModel.get(RestoreTabsProperties.CURRENT_SCREEN)));
                    } else if (RestoreTabsProperties.VISIBLE == propertyKey) {
                        boolean visibilityChangeSuccessful =
                                mMediator.setVisible(
                                        mModel.get(RestoreTabsProperties.VISIBLE), mContent);
                        if (!visibilityChangeSuccessful
                                && mModel.get(RestoreTabsProperties.VISIBLE)) {
                            mMediator.dismiss();
                        }
                    }
                });
    }

    // Helper function to convert the screen type to an index for CURRENT_SCREEN.
    private static int getScreenIndexForScreenType(@ScreenType int screenType) {
        switch (screenType) {
            case ScreenType.HOME_SCREEN:
                return 0;
                // Both the device and review tabs selection screens are displayed on the detail
                // screen.
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
        mRestoreTabsDetailScreenCoordinator.destroy();
        mRestoreTabsDetailScreenCoordinator = null;
    }

    public void showHomeScreen(
            ForeignSessionHelper foreignSessionHelper,
            List<ForeignSession> sessions,
            RestoreTabsControllerDelegate delegate) {
        mMediator.showHomeScreen(foreignSessionHelper, sessions, delegate);
    }

    PropertyModel getPropertyModelForTesting() {
        return mModel;
    }

    ViewFlipper getViewFlipperForTesting() {
        return mViewFlipperView;
    }

    View getContentViewForTesting() {
        return mContent.getContentView();
    }
}
