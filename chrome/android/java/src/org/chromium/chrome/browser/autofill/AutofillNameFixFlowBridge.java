// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.Activity;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.chrome.browser.autofill.AutofillNameFixFlowPrompt.AutofillNameFixFlowPromptDelegate;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;

/**
 * JNI call glue for AutofillNameFixFlowPrompt C++ and Java objects.
 */
@JNINamespace("autofill")
final class AutofillNameFixFlowBridge implements AutofillNameFixFlowPromptDelegate {
    private final long mNativeCardNameFixFlowViewAndroid;
    private final Activity mActivity;
    private final String mTitle;
    private final String mInferredName;
    private final String mConfirmButtonLabel;
    private final int mIconId;
    private AutofillNameFixFlowPrompt mNameFixFlowPrompt;

    private AutofillNameFixFlowBridge(long nativeCardNameFixFlowViewAndroid, String title,
            String inferredName, String confirmButtonLabel, int iconId,
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
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> onPromptDismissed());
        }
    }

    @CalledByNative
    private static AutofillNameFixFlowBridge create(long nativeNameFixFlowPrompt, String title,
            String inferredName, String confirmButtonLabel, int iconId,
            WindowAndroid windowAndroid) {
        return new AutofillNameFixFlowBridge(nativeNameFixFlowPrompt, title, inferredName,
                confirmButtonLabel, iconId, windowAndroid);
    }

    @Override
    public void onPromptDismissed() {
        AutofillNameFixFlowBridgeJni.get().promptDismissed(
                mNativeCardNameFixFlowViewAndroid, AutofillNameFixFlowBridge.this);
    }

    @Override
    public void onUserAccept(String name) {
        AutofillNameFixFlowBridgeJni.get().onUserAccept(
                mNativeCardNameFixFlowViewAndroid, AutofillNameFixFlowBridge.this, name);
    }

    /**
     * Shows a prompt for name fix flow.
     */
    @CalledByNative
    private void show(WindowAndroid windowAndroid) {
        mNameFixFlowPrompt = new AutofillNameFixFlowPrompt(mActivity, this, mTitle, mInferredName,
                mConfirmButtonLabel, ResourceId.mapToDrawableId(mIconId));

        if (mNameFixFlowPrompt != null) {
            mNameFixFlowPrompt.show((ChromeActivity) (windowAndroid.getActivity().get()));
        }
    }

    /**
     * Dismisses the prompt without returning any user response.
     */
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
        void onUserAccept(long nativeCardNameFixFlowViewAndroid, AutofillNameFixFlowBridge caller,
                String name);
    }
}
