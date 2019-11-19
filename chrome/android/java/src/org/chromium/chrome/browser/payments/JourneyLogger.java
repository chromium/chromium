// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/**
 * A class used to record journey metrics for the Payment Request feature.
 */
@JNINamespace("payments")
public class JourneyLogger {
    /**
     * Pointer to the native implementation.
     */
    private long mJourneyLoggerAndroid;

    private boolean mWasPaymentRequestTriggered;
    private boolean mHasRecorded;

    public JourneyLogger(boolean isIncognito, WebContents webContents) {
        // Note that this pointer could leak the native object. The called must call destroy() to
        // ensure that the native object is destroyed.
        mJourneyLoggerAndroid = JourneyLoggerJni.get().initJourneyLoggerAndroid(
                JourneyLogger.this, isIncognito, webContents);
    }

    /** Will destroy the native object. This class shouldn't be used afterwards. */
    public void destroy() {
        if (mJourneyLoggerAndroid != 0) {
            JourneyLoggerJni.get().destroy(mJourneyLoggerAndroid, JourneyLogger.this);
            mJourneyLoggerAndroid = 0;
        }
    }

    /**
     * Sets the number of suggestions shown for the specified section.
     *
     * @param section               The section for which to log.
     * @param number                The number of suggestions.
     * @param hasCompleteSuggestion Whether the section has at least one
     *                              complete suggestion.
     */
    public void setNumberOfSuggestionsShown(
            int section, int number, boolean hasCompleteSuggestion) {
        assert section < Section.MAX;
        JourneyLoggerJni.get().setNumberOfSuggestionsShown(
                mJourneyLoggerAndroid, JourneyLogger.this, section, number, hasCompleteSuggestion);
    }

    /**
     * Increments the number of selection changes for the specified section.
     *
     * @param section The section for which to log.
     */
    public void incrementSelectionChanges(int section) {
        assert section < Section.MAX;
        JourneyLoggerJni.get().incrementSelectionChanges(
                mJourneyLoggerAndroid, JourneyLogger.this, section);
    }

    /**
     * Increments the number of selection edits for the specified section.
     *
     * @param section The section for which to log.
     */
    public void incrementSelectionEdits(int section) {
        assert section < Section.MAX;
        JourneyLoggerJni.get().incrementSelectionEdits(
                mJourneyLoggerAndroid, JourneyLogger.this, section);
    }

    /**
     * Increments the number of selection adds for the specified section.
     *
     * @param section The section for which to log.
     */
    public void incrementSelectionAdds(int section) {
        assert section < Section.MAX;
        JourneyLoggerJni.get().incrementSelectionAdds(
                mJourneyLoggerAndroid, JourneyLogger.this, section);
    }

    /**
     * Records the fact that the merchant called CanMakePayment and records its return value.
     *
     * @param value The return value of the CanMakePayment call.
     */
    public void setCanMakePaymentValue(boolean value) {
        JourneyLoggerJni.get().setCanMakePaymentValue(
                mJourneyLoggerAndroid, JourneyLogger.this, value);
    }

    /**
     * Records the fact that the merchant called HasEnrolledInstrument and records its return value.
     *
     * @param value The return value of the HasEnrolledInstrument call.
     */
    public void setHasEnrolledInstrumentValue(boolean value) {
        JourneyLoggerJni.get().setHasEnrolledInstrumentValue(
                mJourneyLoggerAndroid, JourneyLogger.this, value);
    }

    /**
     * Records that an event occurred.
     *
     * @param event The event that occurred.
     */
    public void setEventOccurred(int event) {
        assert event >= 0;
        assert event < Event.ENUM_MAX;

        if (event == Event.SHOWN || event == Event.SKIPPED_SHOW) mWasPaymentRequestTriggered = true;

        JourneyLoggerJni.get().setEventOccurred(mJourneyLoggerAndroid, JourneyLogger.this, event);
    }

    /*
     * Records what user information were requested by the merchant to complete the Payment Request.
     *
     * @param requestShipping Whether the merchant requested a shipping address.
     * @param requestEmail    Whether the merchant requested an email address.
     * @param requestPhone    Whether the merchant requested a phone number.
     * @param requestName     Whether the merchant requestes a name.
     */
    public void setRequestedInformation(boolean requestShipping, boolean requestEmail,
            boolean requestPhone, boolean requestName) {
        JourneyLoggerJni.get().setRequestedInformation(mJourneyLoggerAndroid, JourneyLogger.this,
                requestShipping, requestEmail, requestPhone, requestName);
    }

    /*
     * Records what types of payment methods were requested by the merchant in the Payment Request.
     *
     * @param requestedBasicCard    Whether the merchant requested basic-card.
     * @param requestedMethodGoogle Whether the merchant requested a Google payment method.
     * @param requestedMethodOther  Whether the merchant requested a non basic-card, non-Google
     *                              payment method.
     */
    public void setRequestedPaymentMethodTypes(boolean requestedBasicCard,
            boolean requestedMethodGoogle, boolean requestedMethodOther) {
        JourneyLoggerJni.get().setRequestedPaymentMethodTypes(mJourneyLoggerAndroid,
                JourneyLogger.this, requestedBasicCard, requestedMethodGoogle,
                requestedMethodOther);
    }

    /**
     * Records that the Payment Request was completed sucessfully. Also starts the logging of
     * all the journey logger metrics.
     */
    public void setCompleted() {
        assert !mHasRecorded;

        if (!mHasRecorded) {
            mHasRecorded = true;
            JourneyLoggerJni.get().setCompleted(mJourneyLoggerAndroid, JourneyLogger.this);
        }
    }

    /**
     * Records that the Payment Request was aborted and for what reason. Also starts the logging of
     * all the journey logger metrics.
     *
     * @param reason An int indicating why the payment request was aborted.
     */
    public void setAborted(int reason) {
        assert reason < AbortReason.MAX;

        // The abort reasons on Android cascade into each other, so only the first one should be
        // recorded.
        if (!mHasRecorded) {
            mHasRecorded = true;
            JourneyLoggerJni.get().setAborted(mJourneyLoggerAndroid, JourneyLogger.this, reason);
        }
    }

    /**
     * Records that the Payment Request was not shown to the user and for what reason.
     *
     * @param reason An int indicating why the payment request was not shown.
     */
    public void setNotShown(int reason) {
        assert reason < NotShownReason.MAX;
        assert !mHasRecorded;

        if (!mHasRecorded) {
            mHasRecorded = true;
            JourneyLoggerJni.get().setNotShown(mJourneyLoggerAndroid, JourneyLogger.this, reason);
        }
    }

    /**
     * Records amount of completed/triggered transactions separated by currency.
     *
     * @param curreny A string indicating the curreny of the transaction.
     * @param value A string indicating the value of the transaction.
     * @param completed A boolean indicating whether the transaction has completed or not.
     */
    public void recordTransactionAmount(String currency, String value, boolean completed) {
        JourneyLoggerJni.get().recordTransactionAmount(
                mJourneyLoggerAndroid, JourneyLogger.this, currency, value, completed);
    }

    /**
     * Records the time when request.show() is called.
     */
    public void setTriggerTime() {
        JourneyLoggerJni.get().setTriggerTime(mJourneyLoggerAndroid, JourneyLogger.this);
    }

    @NativeMethods
    interface Natives {
        long initJourneyLoggerAndroid(
                JourneyLogger caller, boolean isIncognito, WebContents webContents);
        void destroy(long nativeJourneyLoggerAndroid, JourneyLogger caller);
        void setNumberOfSuggestionsShown(long nativeJourneyLoggerAndroid, JourneyLogger caller,
                int section, int number, boolean hasCompleteSuggestion);
        void incrementSelectionChanges(
                long nativeJourneyLoggerAndroid, JourneyLogger caller, int section);
        void incrementSelectionEdits(
                long nativeJourneyLoggerAndroid, JourneyLogger caller, int section);
        void incrementSelectionAdds(
                long nativeJourneyLoggerAndroid, JourneyLogger caller, int section);
        void setCanMakePaymentValue(
                long nativeJourneyLoggerAndroid, JourneyLogger caller, boolean value);
        void setHasEnrolledInstrumentValue(
                long nativeJourneyLoggerAndroid, JourneyLogger caller, boolean value);
        void setEventOccurred(long nativeJourneyLoggerAndroid, JourneyLogger caller, int event);
        void setRequestedInformation(long nativeJourneyLoggerAndroid, JourneyLogger caller,
                boolean requestShipping, boolean requestEmail, boolean requestPhone,
                boolean requestName);
        void setRequestedPaymentMethodTypes(long nativeJourneyLoggerAndroid, JourneyLogger caller,
                boolean requestedBasicCard, boolean requestedMethodGoogle,
                boolean requestedMethodOther);
        void setCompleted(long nativeJourneyLoggerAndroid, JourneyLogger caller);
        void setAborted(long nativeJourneyLoggerAndroid, JourneyLogger caller, int reason);
        void setNotShown(long nativeJourneyLoggerAndroid, JourneyLogger caller, int reason);
        void recordTransactionAmount(long nativeJourneyLoggerAndroid, JourneyLogger caller,
                String currency, String value, boolean completed);
        void setTriggerTime(long nativeJourneyLoggerAndroid, JourneyLogger caller);
    }
}
