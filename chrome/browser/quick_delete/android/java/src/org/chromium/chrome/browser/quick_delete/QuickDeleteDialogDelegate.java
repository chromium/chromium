// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import androidx.annotation.NonNull;

import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A delegate responsible for providing logic around the quick delete modal dialog.
 *
 * TODO(crbug.com/1412087): Add implementation logic for the dialog.
 */
class QuickDeleteDialogDelegate {
    private final @NonNull ModalDialogManager mModalDialogManager;
    private final @NonNull QuickDeleteSnackbarDelegate mQuickDeleteSnackbarDelegate;

    /**The {@link PropertyModel} of the underlying dialog where the quick dialog view would be
     * shown.*/
    private final PropertyModel mModalDialogPropertyModel;

    /**
     * The modal dialog controller to detect events on the dialog.
     */
    private final ModalDialogProperties.Controller mModalDialogController =
            new ModalDialogProperties.Controller() {
                @Override
                public void onClick(PropertyModel model, int buttonType) {}
                @Override
                public void onDismiss(PropertyModel model, int dismissalCause) {}
            };

    /**
     * @param modalDialogManager A {@link ModalDialogManager} responsible for showing the quick
     *         delete modal dialog.
     * @param quickDeleteSnackbarDelegate A {@link QuickDeleteSnackbarDelegate} delegate responsible
     *         for showing the quick delete snack-bar.
     */
    QuickDeleteDialogDelegate(@NonNull ModalDialogManager modalDialogManager,
            @NonNull QuickDeleteSnackbarDelegate quickDeleteSnackbarDelegate) {
        mModalDialogManager = modalDialogManager;
        mQuickDeleteSnackbarDelegate = quickDeleteSnackbarDelegate;
        mModalDialogPropertyModel = createQuickDeleteDialogProperty();
    }

    /**
     * A method to create the dialog attributes for the quick delete dialog.
     *
     * TODO(crbug.com/1412087): Add logic to setup the quick delete dialog attributes and show
     * the snack-bar after "Delete" confirmation in the dialog.
     */
    private PropertyModel createQuickDeleteDialogProperty() {
        return new PropertyModel();
    }

    void showDialog() {
        mModalDialogManager.showDialog(mModalDialogPropertyModel,
                ModalDialogManager.ModalDialogType.APP,
                ModalDialogManager.ModalDialogPriority.HIGH);
    }
}