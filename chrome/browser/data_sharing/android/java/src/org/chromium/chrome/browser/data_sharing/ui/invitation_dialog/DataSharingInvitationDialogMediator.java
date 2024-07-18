// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.invitation_dialog;

import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator class responsible for handling button clicks and controlling the dialog state. */
class DataSharingInvitationDialogMediator
        implements ModalDialogProperties.Controller, ModalDialogManagerObserver {

    private PropertyModel mModel;
    private final ModalDialogManager mDialogManager;

    /**
     * Constructor for {@link DataSharingInvitationDialogMediator}.
     *
     * @param modalDialogManager The ModalDialogManager which is going to display the dialog.
     */
    DataSharingInvitationDialogMediator(ModalDialogManager modalDialogManager) {
        mDialogManager = modalDialogManager;
    }

    /** ModalDialogProperties.Controller implementation */
    @Override
    public void onClick(PropertyModel model, @ButtonType int buttonType) {}

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        mDialogManager.removeObserver(this);
    }

    /**
     * Show the dialog.
     *
     * @param model The {@link PropertyModel} describing the dialog to be shown.
     */
    void show(PropertyModel model) {
        mModel = model;
        mDialogManager.addObserver(this);
        mDialogManager.showDialog(
                model, ModalDialogManager.ModalDialogType.APP, /* showAsNext= */ true);
    }

    /** Dismisses the currently visible dialog. */
    void dismiss() {
        mDialogManager.dismissDialog(mModel, DialogDismissalCause.ACTION_ON_CONTENT);
    }
}
