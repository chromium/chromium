// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A delegate responsible for providing logic around the quick delete modal dialog.
 */
class QuickDeleteDialogDelegate {
    private final @NonNull ModalDialogManager mModalDialogManager;
    private final @NonNull Context mContext;
    private final @NonNull Callback<Integer> mOnDismissCallback;
    /**The {@link PropertyModel} of the underlying dialog where the quick dialog view would be
     * shown.*/
    private final PropertyModel mModalDialogPropertyModel;

    /**
     * The modal dialog controller to detect events on the dialog.
     */
    private final ModalDialogProperties.Controller mModalDialogController =
            new ModalDialogProperties.Controller() {
                @Override
                public void onClick(PropertyModel model, int buttonType) {
                    if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                        mModalDialogManager.dismissDialog(mModalDialogPropertyModel,
                                DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                    } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
                        mModalDialogManager.dismissDialog(mModalDialogPropertyModel,
                                DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                    }
                }

                @Override
                public void onDismiss(PropertyModel model, int dismissalCause) {
                    mOnDismissCallback.onResult(dismissalCause);
                }
            };

    /**
     * @param context The associated {@link Context}.
     * @param modalDialogManager A {@link ModalDialogManager} responsible for showing the quick
     *         delete modal dialog.
     * @param onDismissCallback A {@link Callback} that will be notified when the user confirms or
     *         cancels the deletion;
     */
    QuickDeleteDialogDelegate(@NonNull Context context,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull Callback<Integer> onDismissCallback) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mOnDismissCallback = onDismissCallback;
        mModalDialogPropertyModel = createQuickDeleteDialogProperty();
    }

    /**
     * A method to create the dialog attributes for the quick delete dialog.
     */
    private PropertyModel createQuickDeleteDialogProperty() {
        View quickDeleteDialogView =
                LayoutInflater.from(mContext).inflate(R.layout.quick_delete_dialog, /*root=*/null);

        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, mModalDialogController)
                .with(ModalDialogProperties.TITLE,
                        mContext.getString(R.string.quick_delete_dialog_title))
                .with(ModalDialogProperties.CUSTOM_VIEW, quickDeleteDialogView)
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                        mContext.getString(R.string.delete))
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                        mContext.getString(R.string.cancel))
                .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                .with(ModalDialogProperties.BUTTON_STYLES,
                        ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                .build();
    }

    /**
     * Shows the Quick delete dialog.
     */
    void showDialog() {
        mModalDialogManager.showDialog(
                mModalDialogPropertyModel, ModalDialogManager.ModalDialogType.APP);
    }
}
