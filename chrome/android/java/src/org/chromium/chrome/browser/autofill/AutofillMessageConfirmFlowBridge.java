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
import org.chromium.chrome.browser.autofill.AutofillSaveCardConfirmFlowPrompt.AutofillSaveCardConfirmFlowPromptDelegate;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;

import java.util.LinkedList;

/**
 * Bridge for SaveCardMessageControllerAndroid to show a confirmation dialog of name or expiration
 * date.
 */
@JNINamespace("autofill")
public class AutofillMessageConfirmFlowBridge
        implements AutofillExpirationDateFixFlowPromptDelegate, AutofillNameFixFlowPromptDelegate,
                   AutofillSaveCardConfirmFlowPromptDelegate {
    @Nullable
    private AutofillSaveCardPromptBase mSaveCardPrompt;

    private long mNativeSaveCardMessageConfirmDelegate;
    private final WindowAndroid mWindowAndroid;
    private LinkedList<LegalMessageLine> mLegalMessageLines = new LinkedList<>();

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
        AutofillMessageConfirmFlowBridgeJni.get().dialogDismissed(
                mNativeSaveCardMessageConfirmDelegate);
    }

    @Override
    public void onUserConfirmedCard() {
        AutofillMessageConfirmFlowBridgeJni.get().onSaveCardConfirmed(
                mNativeSaveCardMessageConfirmDelegate);
    }

    @Override
    public void onUserAcceptCardholderName(String name) {
        AutofillMessageConfirmFlowBridgeJni.get().onNameConfirmed(
                mNativeSaveCardMessageConfirmDelegate, name);
    }

    @Override
    public void onUserAcceptExpirationDate(String month, String year) {
        AutofillMessageConfirmFlowBridgeJni.get().onDateConfirmed(
                mNativeSaveCardMessageConfirmDelegate, month, year);
    }

    // no-op
    @Override
    public void onUserDismiss() {}

    @Override
    public void onLinkClicked(String url) {
        AutofillMessageConfirmFlowBridgeJni.get().onLinkClicked(
                mNativeSaveCardMessageConfirmDelegate, url);
    }

    @CalledByNative
    private static AutofillMessageConfirmFlowBridge create(
            long nativeAutofillMessageConfirmDelegate, WindowAndroid windowAndroid) {
        return new AutofillMessageConfirmFlowBridge(
                nativeAutofillMessageConfirmDelegate, windowAndroid);
    }

    /**
     * @return False if dialog should not be displayed in the next steps, e.g. when activity
     *         has been destroyed.
     */
    private boolean prepareToShowDialog(Activity activity) {
        if (activity == null) {
            // Clean up the native counterpart. Post the dismissal to allow the native
            // caller to finish execution before we attempt to delete it.
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::onPromptDismissed);
            return false;
        }
        return true;
    }

    @CalledByNative
    private void fixDate(
            String title, String cardLabel, String cardholderAccount, String confirmButtonLabel) {
        Activity activity = mWindowAndroid.getActivity().get();
        if (!prepareToShowDialog(activity)) return;
        if (mSaveCardPrompt == null) {
            mSaveCardPrompt = AutofillExpirationDateFixFlowPrompt.createAsMessageFixFlowPrompt(
                    activity, this, title, cardLabel, cardholderAccount, confirmButtonLabel);
            for (LegalMessageLine line : mLegalMessageLines) {
                mSaveCardPrompt.addLegalMessageLine(line);
            }
        }
        mSaveCardPrompt.show(activity, mWindowAndroid.getModalDialogManager());
    }

    @CalledByNative
    private void fixName(String title, String inferredName, String cardLabel,
            String cardholderAccount, String confirmButtonLabel) {
        Activity activity = mWindowAndroid.getActivity().get();
        if (!prepareToShowDialog(activity)) return;
        if (mSaveCardPrompt == null) {
            mSaveCardPrompt = AutofillNameFixFlowPrompt.createAsMessageFixFlowPrompt(activity, this,
                    inferredName, title, cardLabel, cardholderAccount, confirmButtonLabel);
            for (LegalMessageLine line : mLegalMessageLines) {
                mSaveCardPrompt.addLegalMessageLine(line);
            }
        }
        mSaveCardPrompt.show(activity, mWindowAndroid.getModalDialogManager());
    }

    @CalledByNative
    private void confirmSaveCard(
            String title, String cardLabel, String cardholderAccount, String confirmButtonLabel) {
        Activity activity = mWindowAndroid.getActivity().get();
        if (!prepareToShowDialog(activity)) return;
        if (mSaveCardPrompt == null) {
            mSaveCardPrompt = AutofillSaveCardConfirmFlowPrompt.createPrompt(
                    activity, this, title, cardLabel, cardholderAccount, confirmButtonLabel);
            for (LegalMessageLine line : mLegalMessageLines) {
                mSaveCardPrompt.addLegalMessageLine(line);
            }
        }
        mSaveCardPrompt.show(activity, mWindowAndroid.getModalDialogManager());
    }

    @CalledByNative
    private void dismiss() {
        if (mSaveCardPrompt != null) {
            mSaveCardPrompt.dismiss(DialogDismissalCause.DISMISSED_BY_NATIVE);
        }
    }

    @CalledByNative
    private void nativeBridgeDestroyed() {
        mNativeSaveCardMessageConfirmDelegate = 0;
    }

    /**
     * Adds a line of legal message plain text to the dialog.
     *
     * @param text The legal message plain text.
     */
    @CalledByNative
    private void addLegalMessageLine(String text) {
        mLegalMessageLines.add(new LegalMessageLine(text));
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
        mLegalMessageLines.getLast().links.add(new LegalMessageLine.Link(start, end, url));
    }

    @NativeMethods
    interface Natives {
        void onDateConfirmed(long nativeSaveCardMessageConfirmDelegate, String month, String year);
        void onNameConfirmed(long nativeSaveCardMessageConfirmDelegate, String name);
        void onSaveCardConfirmed(long nativeSaveCardMessageConfirmDelegate);
        void onLinkClicked(long nativeSaveCardMessageConfirmDelegate, String url);
        void dialogDismissed(long nativeSaveCardMessageConfirmDelegate);
    }
}
