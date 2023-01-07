// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.view.View;

import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class implements a VrAlertDialog which is similar to Android AlertDialog in VR.
 */
public class VrAlertDialog extends AlertDialog {
    private ModalDialogManager mModalDialogManager;
    private PropertyModel mModalDialogModel;
    private CharSequence mMessage;
    private DialogButton mButtonPositive;
    private DialogButton mButtonNegative;
    protected View mView;

    public VrAlertDialog(Context context, ModalDialogManager modalDialogManager) {
        super(context);
        mModalDialogManager = modalDialogManager;
    }

    private class DialogButton {
        private int mId;
        private String mText;
        private DialogInterface.OnClickListener mListener;
        DialogButton(int id, String text, DialogInterface.OnClickListener listener) {
            mId = id;
            mText = text;
            mListener = listener;
        }

        public int getId() {
            return mId;
        }
        public String getText() {
            return mText;
        }
        public DialogInterface.OnClickListener getListener() {
            return mListener;
        }
    }

    /**
     * Builds a ModalDialogView and ask ModalDialogManager to show it.
     */
    @Override
    public void show() {
        mModalDialogModel = createDialogModel();
        mModalDialogManager.showDialog(mModalDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    /**
     * Set the main view
     */
    @Override
    public void setView(View view) {
        mView = view;
    }

    /**
     * Set the message of the AlertDialog.
     * Note that message and view cannot be used at the same time.
     * If a view is set, then the message will be ignored.
     */
    @Override
    public void setMessage(CharSequence message) {
        mMessage = message;
    }

    /**
     * Add button to the list of buttons of this Dialog.
     */
    @Override
    public void setButton(
            int whichButton, CharSequence text, DialogInterface.OnClickListener listener) {
        assert (whichButton == DialogInterface.BUTTON_POSITIVE
                || whichButton == DialogInterface.BUTTON_NEGATIVE);
        if (whichButton == DialogInterface.BUTTON_POSITIVE) {
            mButtonPositive = new DialogButton(
                    ModalDialogProperties.ButtonType.POSITIVE, text.toString(), listener);
        } else if (whichButton == DialogInterface.BUTTON_NEGATIVE) {
            mButtonNegative = new DialogButton(
                    ModalDialogProperties.ButtonType.NEGATIVE, text.toString(), listener);
        }
    }

    /**
     * Dismiss the dialog.
     */
    @Override
    public void dismiss() {
        mModalDialogManager.dismissDialog(mModalDialogModel, DialogDismissalCause.UNKNOWN);
    }

    private PropertyModel createDialogModel() {
        ModalDialogProperties.Controller controller = new ModalDialogProperties.Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {
                if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                    mButtonPositive.getListener().onClick(null, mButtonPositive.getId());
                } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
                    mButtonNegative.getListener().onClick(null, mButtonNegative.getId());
                }
                dismiss();
            }

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {}
        };
        assert (mView == null || mMessage == null);

        String message = mMessage != null ? mMessage.toString() : null;
        String positiveButtonText = mButtonPositive != null ? mButtonPositive.getText() : null;
        String negativeButtonText = mButtonNegative != null ? mButtonNegative.getText() : null;

        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, controller)
                .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1, message)
                .with(ModalDialogProperties.CUSTOM_VIEW, mView)
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, positiveButtonText)
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, negativeButtonText)
                .build();
    }
}
