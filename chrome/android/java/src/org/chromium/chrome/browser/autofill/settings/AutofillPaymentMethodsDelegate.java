// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Delegate that allows for calling native payment related methods.
 *
 * This class owns the native object, so cleanup must be called by the Java side to avoid
 * leaking memory.
 */
@JNINamespace("autofill")
class AutofillPaymentMethodsDelegate {
    private long mNativeAutofillPaymentMethodsDelegate;

    /**
     * Creates an instance of the delegate, and its native counterpart.
     * @param profile An instance of {@link Profile}.
     */
    public AutofillPaymentMethodsDelegate(Profile profile) {
        mNativeAutofillPaymentMethodsDelegate =
                AutofillPaymentMethodsDelegateJni.get().init(profile);
    }

    /**
     * Calls the native initVirtualCardEnrollment method to start the Virtual Card enrollment.
     * process.
     * @param instrumentId The instrument ID of the {@link
     *         org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard} to enroll.
     * @param virtualCardEnrollmentFieldsLoadedCallback The callback to be triggered when the {@link
     *         VirtualCardEnrollmentFields} are fetched from the server.
     * @throws IllegalStateException when called after the native delegate has been cleaned up, or
     *         if an error occurred during initialization.
     */
    public void initVirtualCardEnrollment(
            long instrumentId,
            Callback<VirtualCardEnrollmentFields> virtualCardEnrollmentFieldsLoadedCallback) {
        if (mNativeAutofillPaymentMethodsDelegate == 0) {
            throw new IllegalStateException(
                    "The native delegate was cleaned up or not initialized.");
        }
        AutofillPaymentMethodsDelegateJni.get()
                .initVirtualCardEnrollment(
                        mNativeAutofillPaymentMethodsDelegate,
                        instrumentId,
                        virtualCardEnrollmentFieldsLoadedCallback);
    }

    /**
     * Enroll the card into virtual cards feature.
     * Note: This should only be called after the InitVirtualCardEnrollment is triggered. This is
     * because the VirtualCardEnrollmentManager stores some state when initVirtualCardEnrollment is
     * called, which is then reused for enrolling into the virtual cards feature.
     * @param virtualCardEnrollmentUpdateResponseCallback The callback to be triggered when the
     *         server response is received regarding enrollment success.
     */
    public void enrollOfferedVirtualCard(
            Callback<Boolean> virtualCardEnrollmentUpdateResponseCallback) {
        if (mNativeAutofillPaymentMethodsDelegate == 0) {
            throw new IllegalStateException(
                    "The native delegate was cleaned up or not initialized.");
        }
        AutofillPaymentMethodsDelegateJni.get()
                .enrollOfferedVirtualCard(
                        mNativeAutofillPaymentMethodsDelegate,
                        virtualCardEnrollmentUpdateResponseCallback);
    }

    /**
     * Calls the native unenrollVirtualCard to unenroll a card from Virtual Cards.
     * process.
     * @param instrumentId The instrument ID of the {@link
     *         org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard} to unenroll.
     * @param virtualCardEnrollmentUpdateResponseCallback The callback to be triggered when the
     *         server response is received regarding unenrollment success.
     * @throws IllegalStateException when called after the native delegate has been cleaned up, or
     *         if an error occurred during initialization.
     */
    public void unenrollVirtualCard(
            long instrumentId, Callback<Boolean> virtualCardEnrollmentUpdateResponseCallback) {
        if (mNativeAutofillPaymentMethodsDelegate == 0) {
            throw new IllegalStateException(
                    "The native delegate was cleaned up or not initialized.");
        }
        AutofillPaymentMethodsDelegateJni.get()
                .unenrollVirtualCard(
                        mNativeAutofillPaymentMethodsDelegate,
                        instrumentId,
                        virtualCardEnrollmentUpdateResponseCallback);
    }

    /**
     * Calls the native deleteSavedCvcs to delete saved CVCs.
     *
     * @throws IllegalStateException when called after the native delegate has been cleaned up, or
     *     if an error occurred during initialization.
     */
    public void deleteSavedCvcs() {
        if (mNativeAutofillPaymentMethodsDelegate == 0) {
            throw new IllegalStateException(
                    "The native delegate was cleaned up or not initialized.");
        }
        AutofillPaymentMethodsDelegateJni.get()
                .deleteSavedCvcs(mNativeAutofillPaymentMethodsDelegate);
    }

    /**
     * Calls the native cleanup method for this instance.
     * This must be called when the object is no longer needed, or a memory leak will result.
     */
    public void cleanup() {
        if (mNativeAutofillPaymentMethodsDelegate != 0) {
            AutofillPaymentMethodsDelegateJni.get().cleanup(mNativeAutofillPaymentMethodsDelegate);
            mNativeAutofillPaymentMethodsDelegate = 0;
        }
    }

    @NativeMethods
    interface Natives {
        long init(@JniType("Profile*") Profile profile);

        void cleanup(long nativeAutofillPaymentMethodsDelegate);

        void initVirtualCardEnrollment(
                long nativeAutofillPaymentMethodsDelegate,
                long instrumentId,
                Callback<VirtualCardEnrollmentFields> virtualCardEnrollmentFieldsCallback);

        void enrollOfferedVirtualCard(
                long nativeAutofillPaymentMethodsDelegate,
                Callback<Boolean> virtualCardEnrollmentUpdateResponseCallback);

        void unenrollVirtualCard(
                long nativeAutofillPaymentMethodsDelegate,
                long instrumentId,
                Callback<Boolean> virtualCardEnrollmentUpdateResponseCallback);

        void deleteSavedCvcs(long nativeAutofillPaymentMethodsDelegate);
    }
}
