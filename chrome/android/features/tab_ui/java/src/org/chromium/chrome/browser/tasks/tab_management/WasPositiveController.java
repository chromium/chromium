// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.base.Callback;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Simple ModalDialogProperties.Controller impl that returns whether the positive button was pressed
 * or not on dismiss.
 */
public class WasPositiveController implements ModalDialogProperties.Controller {
    private final ModalDialogManager mModalDialogManager;
    private final Callback<Boolean> mOnDismissWhetherPositive;

    public WasPositiveController(
            ModalDialogManager modalDialogManager, Callback<Boolean> onDismissWhetherPositive) {
        mModalDialogManager = modalDialogManager;
        mOnDismissWhetherPositive = onDismissWhetherPositive;
    }

    @Override
    public void onClick(PropertyModel model, @ButtonType int buttonType) {
        @DialogDismissalCause
        int cause =
                buttonType == ModalDialogProperties.ButtonType.POSITIVE
                        ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                        : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED;
        mModalDialogManager.dismissDialog(model, cause);
    }

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        mOnDismissWhetherPositive.onResult(
                dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }
}
