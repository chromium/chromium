// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import android.content.Context;
import android.content.res.Resources;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;

/** Dialog to confirm if the user is sure to disable Safe Browsing. */
public class NoProtectionConfirmationDialog {
    private Context mContext;
    private ModalDialogManager mManager;
    private PropertyModel mModel;
    private Callback<Boolean> mDidConfirmCallback;

    public static NoProtectionConfirmationDialog create(
            Context context, Callback<Boolean> didConfirmCallback) {
        return new NoProtectionConfirmationDialog(context, didConfirmCallback);
    }

    private NoProtectionConfirmationDialog(Context context, Callback<Boolean> didConfirmCallback) {
        mContext = context;
        mDidConfirmCallback = didConfirmCallback;
    }

    /** Show this dialog in the context of its enclosing activity. */
    public void show() {
        Resources resources = mContext.getResources();
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, makeController())
                        .with(
                                ModalDialogProperties.TITLE,
                                resources,
                                R.string.safe_browsing_no_protection_confirmation_dialog_title)
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                resources.getString(
                                        R.string
                                                .safe_browsing_no_protection_confirmation_dialog_message))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources,
                                R.string.safe_browsing_no_protection_confirmation_dialog_confirm)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                R.string.cancel);
        mModel = builder.build();
        mManager = new ModalDialogManager(new AppModalPresenter(mContext), ModalDialogType.APP);
        mManager.showDialog(mModel, ModalDialogType.APP);
    }

    private Controller makeController() {
        return new ModalDialogProperties.Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {
                switch (buttonType) {
                    case ModalDialogProperties.ButtonType.POSITIVE:
                        mManager.dismissDialog(
                                mModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                        break;
                    case ModalDialogProperties.ButtonType.NEGATIVE:
                        mManager.dismissDialog(
                                mModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                        break;
                    default:
                        assert false : "Should not be reached.";
                }
            }

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {
                if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                    mDidConfirmCallback.onResult(true);
                } else {
                    mDidConfirmCallback.onResult(false);
                }
                mManager.destroy();
            }
        };
    }
}
