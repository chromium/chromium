// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.LayoutInflater;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;

import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Coordinator for the PriceTrackingSettings dialog in grid tab switcher.
 */
class PriceTrackingDialogCoordinator implements OnCheckedChangeListener {
    private final PropertyModel mModel;
    private final ModalDialogManager mModalDialogManager;
    private final PriceTrackingDialogView mDialogView;

    PriceTrackingDialogCoordinator(Context context, ModalDialogManager modalDialogManager,
            TabSwitcherMediator.ResetHandler resetHandler, TabModelSelector tabModelSelector,
            PriceDropNotificationManager notificationManager) {
        mDialogView = (PriceTrackingDialogView) LayoutInflater.from(context).inflate(
                R.layout.price_tracking_dialog_layout, null, false);
        mDialogView.setupTrackPricesSwitchOnCheckedChangeListener(this);
        mDialogView.setupPriceAlertsArrowOnClickListener(
                v -> { notificationManager.launchNotificationSettings(); });
        mModalDialogManager = modalDialogManager;

        ModalDialogProperties.Controller dialogController = new ModalDialogProperties.Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {}

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {
                if (dismissalCause == DialogDismissalCause.ACTIVITY_DESTROYED) return;

                resetHandler.resetWithTabList(
                        tabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter(),
                        false, TabSwitcherMediator.isShowingTabsInMRUOrder());
            }
        };

        mModel = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                         .with(ModalDialogProperties.CONTROLLER, dialogController)
                         .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                         .with(ModalDialogProperties.CUSTOM_VIEW, mDialogView)
                         .build();
    }

    void show() {
        mDialogView.setupPriceAlertsRowMenuVisibility();
        mDialogView.updateSwitch();
        mModalDialogManager.showDialog(mModel, ModalDialogManager.ModalDialogType.APP);
    }

    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        assert buttonView.getId() == R.id.track_prices_switch;
        if (isChecked != PriceTrackingUtilities.isTrackPricesOnTabsEnabled()) {
            PriceTrackingUtilities.flipTrackPricesOnTabs();
        }
    }
}
