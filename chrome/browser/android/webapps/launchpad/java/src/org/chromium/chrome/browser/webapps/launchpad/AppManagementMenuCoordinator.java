// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.supplier.Supplier;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for displaying the app management menu.
 */
class AppManagementMenuCoordinator implements ModalDialogProperties.Controller {
    private final Context mContext;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private PropertyModel mDialogModel;

    AppManagementMenuCoordinator(
            Context context, Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        mContext = context;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
    }

    void destroy() {
        mModalDialogManagerSupplier.get().dismissDialog(
                mDialogModel, DialogDismissalCause.TAB_DESTROYED);
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {}

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        mDialogModel = null;
    }

    void show(LaunchpadItem item) {
        mDialogModel = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                               .with(ModalDialogProperties.CONTROLLER, this)
                               .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                               .with(ModalDialogProperties.CUSTOM_VIEW, createDialogView(item))
                               .build();

        mModalDialogManagerSupplier.get().showDialog(
                mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    private View createDialogView(LaunchpadItem item) {
        View dialogView = LayoutInflater.from(mContext).inflate(
                R.layout.launchpad_menu_dialog_layout, null, false);

        View headerView = dialogView.findViewById(R.id.dialog_header);
        PropertyModel headerModel = AppManagementMenuHeaderProperties.buildHeader(item);
        PropertyModelChangeProcessor.create(
                headerModel, headerView, new AppManagementMenuHeaderViewBinder());

        return dialogView;
    }
}
