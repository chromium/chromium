// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Simple ModalDialogProperties.Controller impl that returns which button was pressed or whether no
 * click occurred.
 */
public class StrictButtonPressController implements ModalDialogProperties.Controller {
    @IntDef({
        ButtonClickResult.NO_CLICK,
        ButtonClickResult.POSITIVE,
        ButtonClickResult.NEGATIVE,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ButtonClickResult {
        int NO_CLICK = 0;
        int POSITIVE = 1;
        int NEGATIVE = 2;
    }

    private final ModalDialogManager mModalDialogManager;
    private final Callback<Integer> mOnButtonClick;

    /**
     * @param modalDialogManager The global modal dialog manager.
     */
    public StrictButtonPressController(
            ModalDialogManager modalDialogManager, Callback<Integer> onButtonClick) {
        mModalDialogManager = modalDialogManager;
        mOnButtonClick = onButtonClick;
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
        mOnButtonClick.onResult(getButtonClickResult(dismissalCause));
    }

    private @ButtonClickResult int getButtonClickResult(@DialogDismissalCause int dismissalCause) {
        if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
            return ButtonClickResult.POSITIVE;
        } else if (dismissalCause == DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
            return ButtonClickResult.NEGATIVE;
        } else {
            return ButtonClickResult.NO_CLICK;
        }
    }
}
