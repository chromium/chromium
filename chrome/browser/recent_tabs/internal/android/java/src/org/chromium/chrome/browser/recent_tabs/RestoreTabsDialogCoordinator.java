// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ViewFlipper;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsDetailScreenCoordinator;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsPromoScreenCoordinator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.function.Supplier;

/** Coordinator to manage the Restore Tabs on FRE feature. */
@NullMarked
public class RestoreTabsDialogCoordinator {
    private RestoreTabsDialogMediator mMediator;
    private final PropertyModel mModel = RestoreTabsProperties.createDefaultModel();
    private final View mContent;
    private final ViewFlipper mViewFlipperView;
    private RestoreTabsDetailScreenCoordinator mRestoreTabsDetailScreenCoordinator;

    public RestoreTabsDialogCoordinator(
            Context context,
            Profile profile,
            TabCreatorManager tabCreatorManager,
            Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        this(
                context,
                profile,
                new RestoreTabsDialogMediator(),
                tabCreatorManager,
                modalDialogManagerSupplier);
    }

    protected RestoreTabsDialogCoordinator(
            Context context,
            Profile profile,
            RestoreTabsDialogMediator mediator,
            TabCreatorManager tabCreatorManager,
            Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        mMediator = mediator;
        mMediator.initialize(
                mModel, profile, tabCreatorManager, context, modalDialogManagerSupplier);

        View rootView =
                LayoutInflater.from(context)
                        .inflate(R.layout.restore_tabs_bottom_sheet, /* root= */ null);
        mContent = rootView;

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
                                RestoreTabsCoordinator.getScreenIndexForScreenType(
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

    @SuppressWarnings("NullAway")
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
        return mContent;
    }
}
