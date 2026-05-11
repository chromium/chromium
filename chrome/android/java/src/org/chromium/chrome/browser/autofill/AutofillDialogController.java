// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.ConfirmationDialogParams;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.DialogDismissType;
import org.chromium.components.browser_ui.widget.ActionConfirmationDialog.DismissHandler;
import org.chromium.components.browser_ui.widget.StrictButtonPressController.ButtonClickResult;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

/** Controller that allows the native autofill code to show an {@link ActionConfirmationDialog}. */
@JNINamespace("autofill")
@NullMarked
public class AutofillDialogController {
    private long mNativeAutofillDialogView;
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final ActionConfirmationDialog mDialog;

    private AutofillDialogController(long nativeAutofillDialogView, Context context) {
        this(
                nativeAutofillDialogView,
                context,
                ((ModalDialogManagerHolder) context).getModalDialogManager(),
                new ActionConfirmationDialog(
                        context, ((ModalDialogManagerHolder) context).getModalDialogManager()));
    }

    @VisibleForTesting
    AutofillDialogController(
            long nativeAutofillDialogView,
            Context context,
            ModalDialogManager modalDialogManager,
            ActionConfirmationDialog dialog) {
        mContext = context;
        mNativeAutofillDialogView = nativeAutofillDialogView;
        mModalDialogManager = modalDialogManager;
        mDialog = dialog;
    }

    @CalledByNative
    static @Nullable AutofillDialogController create(
            long nativeAutofillDialogView, WindowAndroid windowAndroid) {
        Context context = windowAndroid.getContext().get();
        if (context == null) {
            return null;
        }
        return new AutofillDialogController(nativeAutofillDialogView, context);
    }

    /**
     * Show the dialog.
     *
     * @param title The title of the dialog.
     * @param description The description of the dialog.
     * @param buttonText The text of the positive button.
     */
    @CalledByNative
    void show(
            @JniType("std::u16string") String title,
            @JniType("std::u16string") String description,
            @JniType("std::u16string") String buttonText) {
        mDialog.show(
                new ConfirmationDialogParams.Builder(mContext)
                        .withTitle(title)
                        .withDescription(description)
                        .withPositiveButton(buttonText)
                        .build(),
                this::handleDialogAction);
    }

    /** Dismiss the autofill dialog if it's showing. No-op if it's not showing. */
    @CalledByNative
    @VisibleForTesting
    void dismiss() {
        if (mDialog != null) {
            mModalDialogManager.dismissAllDialogs(DialogDismissalCause.DISMISSED_BY_NATIVE);
            mNativeAutofillDialogView = 0;
        }
    }

    private @DialogDismissType int handleDialogAction(
            DismissHandler dismissHandler,
            @ButtonClickResult int buttonClickResult,
            boolean stopShowing) {
        if (mNativeAutofillDialogView == 0) {
            return DialogDismissType.DISMISS_IMMEDIATELY;
        }
        if (buttonClickResult == ButtonClickResult.POSITIVE) {
            AutofillDialogControllerJni.get().onPositiveButtonClicked(mNativeAutofillDialogView);
        }
        AutofillDialogControllerJni.get().onDismissed(mNativeAutofillDialogView);
        mNativeAutofillDialogView = 0;
        return DialogDismissType.DISMISS_IMMEDIATELY;
    }

    @NativeMethods
    public interface Natives {
        void onPositiveButtonClicked(long nativeAutofillDialogViewAndroid);

        void onDismissed(long nativeAutofillDialogViewAndroid);
    }
}
