// Copyright 2018 The Chromium Authors
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
import org.chromium.chrome.browser.autofill.AutofillNameFixFlowPrompt.AutofillNameFixFlowPromptDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;

/** JNI call glue for AutofillNameFixFlowPrompt C++ and Java objects. */
@JNINamespace("autofill")
final class AutofillNameFixFlowBridge implements AutofillNameFixFlowPromptDelegate {
    private final long mNativeCardNameFixFlowViewAndroid;
    private final Activity mActivity;
    private final String mTitle;
    private final String mInferredName;
    private final String mConfirmButtonLabel;
    private final int mIconId;
    private AutofillNameFixFlowPrompt mNameFixFlowPrompt;

    private AutofillNameFixFlowBridge(
            long nativeCardNameFixFlowViewAndroid,
            String title,
            String inferredName,
            String confirmButtonLabel,
            int iconId,
            WindowAndroid windowAndroid) {
        mNativeCardNameFixFlowViewAndroid = nativeCardNameFixFlowViewAndroid;
        mTitle = title;
        mInferredName = inferredName;
        mConfirmButtonLabel = confirmButtonLabel;
        mIconId = iconId;

        mActivity = windowAndroid.getActivity().get();
        if (mActivity == null) {
            mNameFixFlowPrompt = null;
            // Clean up the native counterpart. This is posted to allow the native counterpart
            // to fully finish the construction of this glue object before we attempt to delete it.
            PostTask.postTask(TaskTraits.UI_DEFAULT, () -> onPromptDismissed());
        }
    }

    @CalledByNative
    private static AutofillNameFixFlowBridge create(
            long nativeNameFixFlowPrompt,
            @JniType("std::u16string") String title,
            @JniType("std::u16string") String inferredName,
            @JniType("std::u16string") String confirmButtonLabel,
            int iconId,
            WindowAndroid windowAndroid) {
        return new AutofillNameFixFlowBridge(
                nativeNameFixFlowPrompt,
                title,
                inferredName,
                confirmButtonLabel,
                iconId,
                windowAndroid);
    }

    @Override
    public void onPromptDismissed() {
        AutofillNameFixFlowBridgeJni.get()
                .promptDismissed(mNativeCardNameFixFlowViewAndroid, AutofillNameFixFlowBridge.this);
    }

    @Override
    public void onUserDismiss() {
        AutofillNameFixFlowBridgeJni.get().onUserDismiss(mNativeCardNameFixFlowViewAndroid);
    }

    @Override
    public void onUserAcceptCardholderName(String name) {
        AutofillNameFixFlowBridgeJni.get()
                .onUserAccept(
                        mNativeCardNameFixFlowViewAndroid, AutofillNameFixFlowBridge.this, name);
    }

    /* no-op. Legal lines aren't set. */
    @Override
    public void onLinkClicked(String url) {}

    /** Shows a prompt for name fix flow. */
    @CalledByNative
    private void show(WindowAndroid windowAndroid) {
        mNameFixFlowPrompt =
                AutofillNameFixFlowPrompt.createAsInfobarFixFlowPrompt(
                        mActivity, this, mInferredName, mTitle, mIconId, mConfirmButtonLabel);

        if (mNameFixFlowPrompt != null) {
            mNameFixFlowPrompt.show(
                    windowAndroid.getActivity().get(), windowAndroid.getModalDialogManager());
        }
    }

    /** Dismisses the prompt without returning any user response. */
    @CalledByNative
    private void dismiss() {
        if (mNameFixFlowPrompt != null) {
            mNameFixFlowPrompt.dismiss(DialogDismissalCause.DISMISSED_BY_NATIVE);
        }
    }

    @NativeMethods
    interface Natives {
        void promptDismissed(
                long nativeCardNameFixFlowViewAndroid, AutofillNameFixFlowBridge caller);

        void onUserDismiss(long nativeCardNameFixFlowViewAndroid);

        void onUserAccept(
                long nativeCardNameFixFlowViewAndroid,
                AutofillNameFixFlowBridge caller,
                @JniType("std::u16string") String name);
    }
}
