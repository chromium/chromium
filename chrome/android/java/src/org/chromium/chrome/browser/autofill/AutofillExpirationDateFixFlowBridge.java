// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.Activity;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.autofill.AutofillExpirationDateFixFlowPrompt.AutofillExpirationDateFixFlowPromptDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;

/** JNI call glue for AutofillExpirationDateFixFlowPrompt C++ and Java objects. */
@JNINamespace("autofill")
final class AutofillExpirationDateFixFlowBridge
        implements AutofillExpirationDateFixFlowPromptDelegate {
    private long mNativeCardExpirationDateFixFlowViewAndroid;
    private final String mTitle;
    private final String mConfirmButtonLabel;
    private final int mIconId;
    private final String mCardLabel;
    private AutofillExpirationDateFixFlowPrompt mExpirationDateFixFlowPrompt;

    private AutofillExpirationDateFixFlowBridge(
            long nativeCardExpirationDateFixFlowViewAndroid,
            String title,
            String confirmButtonLabel,
            int iconId,
            String cardLabel) {
        mNativeCardExpirationDateFixFlowViewAndroid = nativeCardExpirationDateFixFlowViewAndroid;
        mTitle = title;
        mConfirmButtonLabel = confirmButtonLabel;
        mIconId = iconId;
        mCardLabel = cardLabel;
    }

    @CalledByNative
    private static AutofillExpirationDateFixFlowBridge create(
            long nativeCardExpirationDateFixFlowViewAndroid,
            @JniType("std::u16string") String title,
            @JniType("std::u16string") String confirmButtonLabel,
            int iconId,
            @JniType("std::u16string") String cardLabel) {
        return new AutofillExpirationDateFixFlowBridge(
                nativeCardExpirationDateFixFlowViewAndroid,
                title,
                confirmButtonLabel,
                iconId,
                cardLabel);
    }

    @Override
    public void onPromptDismissed() {
        AutofillExpirationDateFixFlowBridgeJni.get()
                .promptDismissed(
                        mNativeCardExpirationDateFixFlowViewAndroid,
                        AutofillExpirationDateFixFlowBridge.this);
        mNativeCardExpirationDateFixFlowViewAndroid = 0;
    }

    @Override
    public void onUserAcceptExpirationDate(String month, String year) {
        AutofillExpirationDateFixFlowBridgeJni.get()
                .onUserAccept(
                        mNativeCardExpirationDateFixFlowViewAndroid,
                        AutofillExpirationDateFixFlowBridge.this,
                        month,
                        year);
    }

    @Override
    public void onUserDismiss() {
        AutofillExpirationDateFixFlowBridgeJni.get()
                .onUserDismiss(
                        mNativeCardExpirationDateFixFlowViewAndroid,
                        AutofillExpirationDateFixFlowBridge.this);
    }

    /* no-op. Legal lines aren't set. */
    @Override
    public void onLinkClicked(String url) {}

    /** Shows a prompt for expiration date fix flow. */
    @CalledByNative
    private void show(WindowAndroid windowAndroid) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            // Clean up the native counterpart. Post the dismissal to allow the native
            // caller to finish execution before we attempt to delete it.
            PostTask.postTask(TaskTraits.UI_DEFAULT, this::onPromptDismissed);
            return;
        }

        mExpirationDateFixFlowPrompt =
                AutofillExpirationDateFixFlowPrompt.createAsInfobarFixFlowPrompt(
                        activity, this, mTitle, mIconId, mCardLabel, mConfirmButtonLabel);
        mExpirationDateFixFlowPrompt.show(activity, windowAndroid.getModalDialogManager());
    }

    /** Dismisses the prompt without returning any user response. */
    @CalledByNative
    private void dismiss() {
        if (mExpirationDateFixFlowPrompt != null) {
            mExpirationDateFixFlowPrompt.dismiss(DialogDismissalCause.DISMISSED_BY_NATIVE);
        }
    }

    @NativeMethods
    interface Natives {
        void promptDismissed(
                long nativeCardExpirationDateFixFlowViewAndroid,
                AutofillExpirationDateFixFlowBridge caller);

        void onUserAccept(
                long nativeCardExpirationDateFixFlowViewAndroid,
                AutofillExpirationDateFixFlowBridge caller,
                @JniType("std::u16string") String month,
                @JniType("std::u16string") String year);

        void onUserDismiss(
                long nativeCardExpirationDateFixFlowViewAndroid,
                AutofillExpirationDateFixFlowBridge caller);
    }
}
