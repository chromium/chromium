// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.autofill.AutofillExpirationDateFixFlowPrompt.AutofillExpirationDateFixFlowPromptDelegate;
import org.chromium.chrome.browser.autofill.AutofillNameFixFlowPrompt.AutofillNameFixFlowPromptDelegate;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;

/**
 * Bridge for SaveCardMessageControllerAndroid to show a confirmation dialog of name or expiration
 * date.
 */
@JNINamespace("autofill")
public class AutofillMessageConfirmFlowBridge
        implements AutofillExpirationDateFixFlowPromptDelegate, AutofillNameFixFlowPromptDelegate {
    @Nullable
    private AutofillNameFixFlowPrompt mCardholderNameFixFlowPrompt;
    @Nullable
    private AutofillExpirationDateFixFlowPrompt mExpirationDateFixFlowPrompt;

    private long mNativeSaveCardMessageConfirmDelegate;
    private final WindowAndroid mWindowAndroid;
    private LegalMessageLine mLegalMessageLine;

    private AutofillMessageConfirmFlowBridge(
            long nativeSaveCardMessageConfirmDelegate, WindowAndroid windowAndroid) {
        mNativeSaveCardMessageConfirmDelegate = nativeSaveCardMessageConfirmDelegate;
        mWindowAndroid = windowAndroid;
    }

    @Override
    public void onPromptDismissed() {
        // In case native is destroyed before #onPromptDismissed is executed by post tasks.
        if (mNativeSaveCardMessageConfirmDelegate == 0) {
            return;
        }
        AutofillMessageConfirmFlowBridgeJni.get().promptDismissed(
                mNativeSaveCardMessageConfirmDelegate, AutofillMessageConfirmFlowBridge.this);
    }

    @Override
    public void onUserAccept(String name) {
        AutofillMessageConfirmFlowBridgeJni.get().onNameConfirmed(
                mNativeSaveCardMessageConfirmDelegate, AutofillMessageConfirmFlowBridge.this, name);
    }

    @Override
    public void onUserAccept(String month, String year) {
        AutofillMessageConfirmFlowBridgeJni.get().onDateConfirmed(
                mNativeSaveCardMessageConfirmDelegate, AutofillMessageConfirmFlowBridge.this, month,
                year);
    }

    // no-op
    @Override
    public void onUserDismiss() {}

    @Override
    public void onLinkClicked(String url) {
        AutofillMessageConfirmFlowBridgeJni.get().onLegalMessageLinkClicked(
                mNativeSaveCardMessageConfirmDelegate, AutofillMessageConfirmFlowBridge.this, url);
    }

    @CalledByNative
    private static AutofillMessageConfirmFlowBridge create(
            long nativeAutofillMessageConfirmDelegate, WindowAndroid windowAndroid) {
        return new AutofillMessageConfirmFlowBridge(
                nativeAutofillMessageConfirmDelegate, windowAndroid);
    }

    @CalledByNative
    private void confirmDate(
            String month, String year, String title, String confirmButtonLabel, String cardLabel) {
        Activity activity = mWindowAndroid.getActivity().get();
        if (activity == null) {
            // Clean up the native counterpart. Post the dismissal to allow the native
            // caller to finish execution before we attempt to delete it.
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::onPromptDismissed);
            return;
        }
        if (mExpirationDateFixFlowPrompt == null) {
            mExpirationDateFixFlowPrompt =
                    AutofillExpirationDateFixFlowPrompt.createAsMessageFixFlowPrompt(
                            activity, this, month, year, title, confirmButtonLabel, cardLabel);
            mExpirationDateFixFlowPrompt.setLegalMessageLine(mLegalMessageLine);
        }
        mExpirationDateFixFlowPrompt.show(activity, mWindowAndroid.getModalDialogManager());
    }

    @CalledByNative
    private void confirmName(
            String title, String inferredName, String confirmButtonLabel, String cardLabel) {
        Activity activity = mWindowAndroid.getActivity().get();
        if (activity == null) {
            // Clean up the native counterpart. Post the dismissal to allow the native
            // caller to finish execution before we attempt to delete it.
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::onPromptDismissed);
            return;
        }
        if (mCardholderNameFixFlowPrompt == null) {
            mCardholderNameFixFlowPrompt = AutofillNameFixFlowPrompt.createAsMessageFixFlowPrompt(
                    activity, this, title, inferredName, confirmButtonLabel, cardLabel);
            mCardholderNameFixFlowPrompt.setLegalMessageLine(mLegalMessageLine);
        }
        mCardholderNameFixFlowPrompt.show(activity, mWindowAndroid.getModalDialogManager());
    }

    @CalledByNative
    private void dismiss() {
        if (mExpirationDateFixFlowPrompt != null) {
            mExpirationDateFixFlowPrompt.dismiss(DialogDismissalCause.DISMISSED_BY_NATIVE);
        }
        if (mCardholderNameFixFlowPrompt != null) {
            mCardholderNameFixFlowPrompt.dismiss(DialogDismissalCause.DISMISSED_BY_NATIVE);
        }
    }

    @CalledByNative
    private void nativeBridgeDestroyed() {
        mNativeSaveCardMessageConfirmDelegate = 0;
    }

    /**
     * Sets a line of legal message plain text to the dialog.
     *
     * @param text The legal message plain text.
     */
    @CalledByNative
    private void setLegalMessageLine(String text) {
        mLegalMessageLine = new LegalMessageLine(text);
    }

    /**
     * Marks up the last added line of legal message text with a link.
     *
     * @param start The inclusive offset of the start of the link in the text.
     * @param end The exclusive offset of the end of the link in the text.
     * @param url The URL to open when the link is clicked.
     */
    @CalledByNative
    private void addLinkToLastLegalMessageLine(int start, int end, String url) {
        mLegalMessageLine.links.add(new LegalMessageLine.Link(start, end, url));
    }

    @NativeMethods
    interface Natives {
        void onDateConfirmed(long nativeSaveCardMessageConfirmDelegate,
                AutofillMessageConfirmFlowBridge caller, String month, String year);
        void promptDismissed(
                long nativeSaveCardMessageConfirmDelegate, AutofillMessageConfirmFlowBridge caller);
        void onNameConfirmed(long nativeSaveCardMessageConfirmDelegate,
                AutofillMessageConfirmFlowBridge caller, String name);
        void onLegalMessageLinkClicked(long nativeSaveCardMessageConfirmDelegate,
                AutofillMessageConfirmFlowBridge caller, String url);
    }
}
