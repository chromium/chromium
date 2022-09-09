// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.autofill.settings.AutofillVirtualCardEnrollmentDialog;
import org.chromium.chrome.browser.autofill.settings.VirtualCardEnrollmentFields;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;

/**
 * View that shows virtual card enrollment information in a modal dialog.
 */
@JNINamespace("autofill")
public class VirtualCardEnrollmentDialogViewBridge {
    private final String mAcceptButtonText;
    private final String mDeclineButtonText;
    private final VirtualCardEnrollmentDelegate mDelegate;
    private final VirtualCardEnrollmentFields mFields;
    private final WindowAndroid mWindowAndroid;
    private AutofillVirtualCardEnrollmentDialog mDialog;

    /**
     * Creates an instance of a {@link VirtualCardEnrollmentDialogViewBridge}.
     *
     * @param acceptButtonText   Text for the accept button.
     * @param declineButtonText  Text for the decline button.
     * @param delegate           The {@link VirtualCardEnrollmentDelegate} for making native calls.
     * @param fields             The {@link VirtualCardEnrollmentFields} for the card that is being
     *                           enrolled as a virtual card.
     * @param windowAndroid      The {@link WindowAndroid} for the containing activity.
     */
    private VirtualCardEnrollmentDialogViewBridge(String acceptButtonText, String declineButtonText,
            VirtualCardEnrollmentDelegate delegate, VirtualCardEnrollmentFields fields,
            WindowAndroid windowAndroid) {
        mAcceptButtonText = acceptButtonText;
        mDeclineButtonText = declineButtonText;
        mDelegate = delegate;
        mFields = fields;
        mWindowAndroid = windowAndroid;
    }

    @CalledByNative
    private static VirtualCardEnrollmentDialogViewBridge create(String acceptButtonText,
            String declineButtonText, VirtualCardEnrollmentDelegate delegate,
            VirtualCardEnrollmentFields fields, WindowAndroid windowAndroid) {
        if (delegate == null || fields == null || windowAndroid == null) {
            return null;
        }
        return new VirtualCardEnrollmentDialogViewBridge(
                acceptButtonText, declineButtonText, delegate, fields, windowAndroid);
    }

    /**
     * Create and show the {@link AutofillVirtualCardEnrollmentDialog}.
     */
    @CalledByNative
    private void showDialog() {
        Callback<Integer> resultHandler = dismissalCause -> {
            if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                mDelegate.onAccepted();
            } else if (dismissalCause == DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
                mDelegate.onDeclined();
            } else {
                mDelegate.onDismissed();
            }
        };
        mDialog = new AutofillVirtualCardEnrollmentDialog(mWindowAndroid.getActivity().get(),
                mWindowAndroid.getModalDialogManager(), mFields, mAcceptButtonText,
                mDeclineButtonText, mDelegate::onLinkClicked, resultHandler);
        mDialog.show();
    }

    /**
     * Lets the native controller dismiss the dialog.
     */
    @CalledByNative
    private void dismiss() {
        if (mDialog != null) {
            mDialog.dismiss(DialogDismissalCause.DISMISSED_BY_NATIVE);
        }
    }
}
