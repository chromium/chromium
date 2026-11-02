// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.AutofillNameFixFlowPrompt.AutofillNameFixFlowPromptDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;

/** JNI call glue for AutofillNameFixFlowPrompt C++ and Java objects. */
@JNINamespace("autofill")
@NullMarked
final class AutofillNameFixFlowBridge implements AutofillNameFixFlowPromptDelegate {
    @JniType("::autofill::CardNameFixFlowViewAndroid")
    interface NativeCardNameFixFlowViewAndroid extends JniTypeToken {}

    private @Nullable JniUniquePtr<NativeCardNameFixFlowViewAndroid>
            mNativeCardNameFixFlowViewAndroid;
    private final @Nullable Activity mActivity;
    private final String mTitle;
    private final String mInferredName;
    private final String mConfirmButtonLabel;
    private final int mIconId;
    private @Nullable AutofillNameFixFlowPrompt mNameFixFlowPrompt;

    @VisibleForTesting
    AutofillNameFixFlowBridge(
            JniUniquePtr<NativeCardNameFixFlowViewAndroid> nativeCardNameFixFlowViewAndroid,
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
            PostTask.postTask(TaskTraits.UI_DEFAULT, this::onPromptDismissed);
        }
    }

    @CalledByNative
    private static AutofillNameFixFlowBridge create(
            JniUniquePtr<NativeCardNameFixFlowViewAndroid> nativeNameFixFlowPrompt,
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
        if (mNativeCardNameFixFlowViewAndroid == null) return;
        JniUniquePtr<NativeCardNameFixFlowViewAndroid> nativePtr =
                mNativeCardNameFixFlowViewAndroid;
        // The native pointer is nulled out here before calling destroy() to ensure
        // that any subsequent UI events triggered during the dismissal flow (like focus
        // changes or text watcher events) are dropped instead of attempting to call
        // JNI methods on a dangling pointer.
        mNativeCardNameFixFlowViewAndroid = null;
        nativePtr.destroy();
    }

    @Override
    public void onUserDismiss() {
        if (mNativeCardNameFixFlowViewAndroid == null) return;
        AutofillNameFixFlowBridgeJni.get().onUserDismiss(mNativeCardNameFixFlowViewAndroid);
    }

    @Override
    public void onUserAcceptCardholderName(String name) {
        if (mNativeCardNameFixFlowViewAndroid == null) return;
        AutofillNameFixFlowBridgeJni.get().onUserAccept(mNativeCardNameFixFlowViewAndroid, name);
    }

    /* no-op. Legal lines aren't set. */
    @Override
    public void onLinkClicked(String url) {}

    /** Shows a prompt for name fix flow. */
    @CalledByNative
    private void show(WindowAndroid windowAndroid) {
        mNameFixFlowPrompt =
                AutofillNameFixFlowPrompt.createAsMessageFixFlowPrompt(
                        assertNonNull(mActivity),
                        this,
                        mInferredName,
                        mTitle,
                        mIconId,
                        mConfirmButtonLabel);

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
        void onUserDismiss(JniPtr<NativeCardNameFixFlowViewAndroid> self);

        void onUserAccept(
                JniPtr<NativeCardNameFixFlowViewAndroid> self,
                @JniType("std::u16string") String name);
    }
}
