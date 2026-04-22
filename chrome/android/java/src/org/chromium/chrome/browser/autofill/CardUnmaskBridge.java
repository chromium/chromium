// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.Activity;
import android.os.Handler;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.CardUnmaskPrompt.CardUnmaskPromptDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.url.GURL;

/** JNI call glue for CardUnmaskPrompt C++ and Java objects. */
@JNINamespace("autofill")
@NullMarked
public class CardUnmaskBridge implements CardUnmaskPromptDelegate {
    /**
     * This points to an owned C++ CardUnmaskPromptViewAndroid object.
     *
     * <p>This pointer is reset to zero during #dismissed().
     */
    private long mNativeCardUnmaskPromptViewAndroid;

    private final @Nullable CardUnmaskPrompt mCardUnmaskPrompt;

    @VisibleForTesting
    CardUnmaskBridge(
            long nativeCardUnmaskPromptViewAndroid,
            AutofillImageFetcher imageFetcher,
            String title,
            String instructions,
            int cardIconId,
            String cardName,
            String cardLastFourDigits,
            String cardExpiration,
            GURL cardArtUrl,
            String confirmButtonLabel,
            int cvcIconId,
            String cvcImageAnnouncement,
            boolean isVirtualCard,
            boolean shouldRequestExpirationDate,
            boolean shouldOfferWebauthn,
            boolean defaultUseScreenlockChecked,
            long successMessageDurationMilliseconds,
            WindowAndroid windowAndroid) {
        mNativeCardUnmaskPromptViewAndroid = nativeCardUnmaskPromptViewAndroid;
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            mCardUnmaskPrompt = null;
            // Clean up the native counterpart.  This is posted to allow the native counterpart
            // to fully finish the construction of this glue object before we attempt to delete it.
            new Handler().post(() -> dismissed());
        } else {
            mCardUnmaskPrompt =
                    new CardUnmaskPrompt(
                            activity,
                            this,
                            imageFetcher,
                            title,
                            instructions,
                            cardIconId,
                            cardName,
                            cardLastFourDigits,
                            cardExpiration,
                            cardArtUrl,
                            confirmButtonLabel,
                            cvcIconId,
                            cvcImageAnnouncement,
                            isVirtualCard,
                            shouldRequestExpirationDate,
                            shouldOfferWebauthn,
                            defaultUseScreenlockChecked,
                            successMessageDurationMilliseconds);
        }
    }

    // TODO (crbug.com/40236415): Sync down the credit card directly from native instead of adding
    // more and more arguments.
    @CalledByNative
    private static CardUnmaskBridge create(
            long nativeUnmaskPrompt,
            Profile profile,
            String title,
            String instructions,
            int cardIconId,
            String cardName,
            String cardLastFourDigits,
            String cardExpiration,
            GURL cardArtUrl,
            String confirmButtonLabel,
            int cvcIconId,
            String cvcImageAnnouncement,
            int unused_googlePayIconId,
            boolean isVirtualCard,
            boolean shouldRequestExpirationDate,
            boolean shouldOfferWebauthn,
            boolean defaultUseScreenlockChecked,
            long successMessageDurationMilliseconds,
            WindowAndroid windowAndroid) {
        return new CardUnmaskBridge(
                nativeUnmaskPrompt,
                AutofillImageFetcherFactory.getForProfile(profile),
                title,
                instructions,
                cardIconId,
                cardName,
                cardLastFourDigits,
                cardExpiration,
                cardArtUrl,
                confirmButtonLabel,
                cvcIconId,
                cvcImageAnnouncement,
                isVirtualCard,
                shouldRequestExpirationDate,
                shouldOfferWebauthn,
                defaultUseScreenlockChecked,
                successMessageDurationMilliseconds,
                windowAndroid);
    }

    @Override
    public void dismissed() {
        if (mNativeCardUnmaskPromptViewAndroid == 0) return;
        long nativePtr = mNativeCardUnmaskPromptViewAndroid;
        // The native pointer is zeroed out here before calling promptDismissed to ensure
        // that any subsequent asynchronous UI events triggered during the dismissal flow
        // (like focus changes or text watcher events) are dropped instead of attempting
        // to call JNI methods on a dangling pointer.
        mNativeCardUnmaskPromptViewAndroid = 0;
        CardUnmaskBridgeJni.get().promptDismissed(nativePtr);
    }

    @Override
    public boolean checkUserInputValidity(String userResponse) {
        if (mNativeCardUnmaskPromptViewAndroid == 0) return false;
        return CardUnmaskBridgeJni.get()
                .checkUserInputValidity(mNativeCardUnmaskPromptViewAndroid, userResponse);
    }

    @Override
    public void onUserInput(
            String cvc,
            String month,
            String year,
            boolean enableFidoAuth,
            boolean wasCheckboxVisible) {
        if (mNativeCardUnmaskPromptViewAndroid == 0) return;
        CardUnmaskBridgeJni.get()
                .onUserInput(
                        mNativeCardUnmaskPromptViewAndroid,
                        cvc,
                        month,
                        year,
                        enableFidoAuth,
                        wasCheckboxVisible);
    }

    @Override
    public void onNewCardLinkClicked() {
        if (mNativeCardUnmaskPromptViewAndroid == 0) return;
        CardUnmaskBridgeJni.get().onNewCardLinkClicked(mNativeCardUnmaskPromptViewAndroid);
    }

    @Override
    public int getExpectedCvcLength() {
        if (mNativeCardUnmaskPromptViewAndroid == 0) return 0;
        return CardUnmaskBridgeJni.get().getExpectedCvcLength(mNativeCardUnmaskPromptViewAndroid);
    }

    /** Shows a prompt for unmasking a Wallet credit card. */
    @CalledByNative
    private void show(WindowAndroid windowAndroid) {
        if (mCardUnmaskPrompt != null) {
            mCardUnmaskPrompt.show(
                    windowAndroid.getActivity().get(), windowAndroid.getModalDialogManager());
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

    /** Dismisses the prompt without returning any user response. */
    @CalledByNative
    private void dismiss() {
        if (mCardUnmaskPrompt != null) {
            mCardUnmaskPrompt.dismiss(DialogDismissalCause.DISMISSED_BY_NATIVE);
        }
    }

    /** Disables input and visually indicates that verification is ongoing. */
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
        /** Destroys the C++ CardUnmaskPromptViewAndroid object. */
        void promptDismissed(long nativeCardUnmaskPromptViewAndroid);

        boolean checkUserInputValidity(
                long nativeCardUnmaskPromptViewAndroid,
                @JniType("std::u16string") String userResponse);

        void onUserInput(
                long nativeCardUnmaskPromptViewAndroid,
                @JniType("std::u16string") String cvc,
                @JniType("std::u16string") String month,
                @JniType("std::u16string") String year,
                boolean enableFidoAuth,
                boolean wasCheckboxVisible);

        void onNewCardLinkClicked(long nativeCardUnmaskPromptViewAndroid);

        int getExpectedCvcLength(long nativeCardUnmaskPromptViewAndroid);
    }
}
