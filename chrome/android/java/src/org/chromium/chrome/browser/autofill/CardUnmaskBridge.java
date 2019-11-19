// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.Activity;
import android.os.Handler;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.chrome.browser.autofill.CardUnmaskPrompt.CardUnmaskPromptDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;

/**
* JNI call glue for CardUnmaskPrompt C++ and Java objects.
*/
@JNINamespace("autofill")
public class CardUnmaskBridge implements CardUnmaskPromptDelegate {
    private final long mNativeCardUnmaskPromptViewAndroid;
    private final CardUnmaskPrompt mCardUnmaskPrompt;

    public CardUnmaskBridge(long nativeCardUnmaskPromptViewAndroid, String title,
            String instructions, String confirmButtonLabel, int iconId,
            boolean shouldRequestExpirationDate, boolean canStoreLocally,
            boolean defaultToStoringLocally, boolean defaultUseScreenlockChecked,
            long successMessageDurationMilliseconds, WindowAndroid windowAndroid) {
        mNativeCardUnmaskPromptViewAndroid = nativeCardUnmaskPromptViewAndroid;
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            mCardUnmaskPrompt = null;
            // Clean up the native counterpart.  This is posted to allow the native counterpart
            // to fully finish the construction of this glue object before we attempt to delete it.
            new Handler().post(() -> dismissed());
        } else {
            mCardUnmaskPrompt = new CardUnmaskPrompt(activity, this, title, instructions,
                    confirmButtonLabel, ResourceId.mapToDrawableId(iconId),
                    shouldRequestExpirationDate, canStoreLocally, defaultToStoringLocally,
                    defaultUseScreenlockChecked, successMessageDurationMilliseconds);
        }
    }

    @CalledByNative
    private static CardUnmaskBridge create(long nativeUnmaskPrompt, String title,
            String instructions, String confirmButtonLabel, int iconId,
            boolean shouldRequestExpirationDate, boolean canStoreLocally,
            boolean defaultToStoringLocally, boolean defaultUseScreenlockChecked,
            long successMessageDurationMilliseconds, WindowAndroid windowAndroid) {
        return new CardUnmaskBridge(nativeUnmaskPrompt, title, instructions, confirmButtonLabel,
                iconId, shouldRequestExpirationDate, canStoreLocally, defaultToStoringLocally,
                defaultUseScreenlockChecked, successMessageDurationMilliseconds, windowAndroid);
    }

    @Override
    public void dismissed() {
        CardUnmaskBridgeJni.get().promptDismissed(
                mNativeCardUnmaskPromptViewAndroid, CardUnmaskBridge.this);
    }

    @Override
    public boolean checkUserInputValidity(String userResponse) {
        return CardUnmaskBridgeJni.get().checkUserInputValidity(
                mNativeCardUnmaskPromptViewAndroid, CardUnmaskBridge.this, userResponse);
    }

    @Override
    public void onUserInput(String cvc, String month, String year, boolean shouldStoreLocally,
            boolean enableFidoAuth) {
        CardUnmaskBridgeJni.get().onUserInput(mNativeCardUnmaskPromptViewAndroid,
                CardUnmaskBridge.this, cvc, month, year, shouldStoreLocally, enableFidoAuth);
    }

    @Override
    public void onNewCardLinkClicked() {
        CardUnmaskBridgeJni.get().onNewCardLinkClicked(
                mNativeCardUnmaskPromptViewAndroid, CardUnmaskBridge.this);
    }

    @Override
    public int getExpectedCvcLength() {
        return CardUnmaskBridgeJni.get().getExpectedCvcLength(
                mNativeCardUnmaskPromptViewAndroid, CardUnmaskBridge.this);
    }

    /**
     * Shows a prompt for unmasking a Wallet credit card.
     */
    @CalledByNative
    private void show(WindowAndroid windowAndroid) {
        if (mCardUnmaskPrompt != null) {
            mCardUnmaskPrompt.show((ChromeActivity) (windowAndroid.getActivity().get()));
        }
    }

    /**
     * After a prompt is already showing, update some UI elements.
     * @param title The dialog title.
     * @param instructions Expository text.
     * @param shouldRequestExpirationDate Whether to show the Update + Verify UI or just the
     * Verify UI.
     */
    @CalledByNative
    private void update(String title, String instructions, boolean shouldRequestExpirationDate) {
        if (mCardUnmaskPrompt != null) {
            mCardUnmaskPrompt.update(title, instructions, shouldRequestExpirationDate);
        }
    }

    /**
     * Dismisses the prompt without returning any user response.
     */
    @CalledByNative
    private void dismiss() {
        if (mCardUnmaskPrompt != null) {
            mCardUnmaskPrompt.dismiss(DialogDismissalCause.DISMISSED_BY_NATIVE);
        }
    }

    /**
     * Disables input and visually indicates that verification is ongoing.
     */
    @CalledByNative
    private void disableAndWaitForVerification() {
        if (mCardUnmaskPrompt != null) mCardUnmaskPrompt.disableAndWaitForVerification();
    }

    /**
     * Indicate that verification failed, allow user to retry.
     * @param errorMessage The error to display, or null to signal success.
     * @param allowRetry If there was an error, indicates whether to allow another attempt.
     */
    @CalledByNative
    private void verificationFinished(String errorMessage, boolean allowRetry) {
        if (mCardUnmaskPrompt != null) {
            mCardUnmaskPrompt.verificationFinished(errorMessage, allowRetry);
        }
    }

    @NativeMethods
    interface Natives {
        void promptDismissed(long nativeCardUnmaskPromptViewAndroid, CardUnmaskBridge caller);
        boolean checkUserInputValidity(long nativeCardUnmaskPromptViewAndroid,
                CardUnmaskBridge caller, String userResponse);
        void onUserInput(long nativeCardUnmaskPromptViewAndroid, CardUnmaskBridge caller,
                String cvc, String month, String year, boolean shouldStoreLocally,
                boolean enableFidoAuth);
        void onNewCardLinkClicked(long nativeCardUnmaskPromptViewAndroid, CardUnmaskBridge caller);
        int getExpectedCvcLength(long nativeCardUnmaskPromptViewAndroid, CardUnmaskBridge caller);
    }
}
