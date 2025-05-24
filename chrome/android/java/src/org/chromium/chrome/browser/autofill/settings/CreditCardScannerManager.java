// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.CreditCardScanner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.IntentRequestTracker;

import java.util.HashSet;
import java.util.Set;

/**
 * A wrapper for the credit card scanner used to help log additional metrics about its use and its
 * effect on saving payment information.
 */
@NullMarked
public class CreditCardScannerManager implements CreditCardScanner.Delegate {
    static final String SCAN_CARD_CLICKED_USER_ACTION =
            "Autofill.PaymentMethodsSettingsPage.ScanCardClicked";
    static final String SCANNER_CANNOT_SCAN_USER_ACTION =
            "Autofill.PaymentMethodsSettingsPage.ScannerCannotScan";
    static final String SCAN_CARD_RESULT_HISTOGRAM =
            "Autofill.PaymentMethodsSettingsPage.ScanCardResult";

    /**
     * Delegate for the credit card scanner manager. Required to make the callback when the scan is
     * completed.
     */
    interface Delegate {
        /**
         * Notifies the delegate that scanning was completed successfully.
         *
         * @param cardholderName The credit cardholder name.
         * @param cardNumber The credit card number.
         * @param expirationMonth The expiration month in the range [1, 12].
         * @param expirationYear The expiration year, e.g. 2000.
         */
        void onScanCompleted(
                String cardholderName, String cardNumber, int expirationMonth, int expirationYear);
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // Needs to stay in sync with AutofillScanCreditCardResult in enums.xml.
    @IntDef({
        ScanResult.UNKNOWN,
        ScanResult.SCANNED_WITHOUT_ADDITIONAL_USER_EDITS,
        ScanResult.SCANNED_WITH_USER_EDITS_TO_UNSCANNED_FIELDS,
        ScanResult.SCANNED_WITH_USER_EDITS_TO_SCANNED_FIELDS,
        ScanResult.SCANNED_BUT_USER_CLOSED_FORM,
        ScanResult.CANCELLED,
        ScanResult.IGNORED,
        ScanResult.COUNT
    })
    @VisibleForTesting
    @interface ScanResult {
        int UNKNOWN = 0;
        // Used when card was scanned and submitted without any edits made manually by
        // the user to the fields.
        int SCANNED_WITHOUT_ADDITIONAL_USER_EDITS = 1;
        // Used when card was scanned and the user made edits to un-scanned fields.
        int SCANNED_WITH_USER_EDITS_TO_UNSCANNED_FIELDS = 2;
        // Used when card was scanned and the user made edits to scanned fields.
        int SCANNED_WITH_USER_EDITS_TO_SCANNED_FIELDS = 3;
        // Used when card was scanned but the user closed the form without saving their card.
        int SCANNED_BUT_USER_CLOSED_FORM = 4;
        // Used when the card scan was started but was cancelled.
        int CANCELLED = 5;
        // Used when the card scan was available but not used by the user.
        int IGNORED = 6;
        int COUNT = 7;
    }

    /** Used to identify which fields were filled by the credit card scanner. */
    public enum FieldType {
        UNKNOWN,
        NUMBER,
        NAME,
        MONTH,
        YEAR
    }

    private final CreditCardScanner mScanner;
    private final Delegate mDelegate;
    private @ScanResult int mScanResult;
    private final Set<FieldType> mFieldsFilledByScanner;
    private boolean mScanResultLogged;

    public CreditCardScannerManager(Delegate delegate) {
        mDelegate = delegate;
        mScanner = CreditCardScanner.create(this);
        mFieldsFilledByScanner = new HashSet<>(/* initialCapacity= */ 5);
        mScanResultLogged = false;

        if (canScan()) {
            // Scanning is possible and offered to the user. The scan result is initialized with
            // IGNORED so if the user saves their card without interacting with the scanner, it will
            // be logged as such. If the user interacts with the scanner at all, the scan result
            // will be updated appropriately.
            mScanResult = ScanResult.IGNORED;
        } else if (!mScanner.canScan()) {
            // We only want to log this metric when the scanner cannot scan. Currently, a disabled
            // feature flag can block the scanner too so make sure that here that the feature flag
            // isn't the reason why the scanner cannot scan before logging the metric.
            RecordUserAction.record(SCANNER_CANNOT_SCAN_USER_ACTION);
        }
    }

    /**
     * Returns true if the scanner ability is available.
     *
     * @return True if the scanner can scan.
     */
    public boolean canScan() {
        return mScanner.canScan()
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList
                                .AUTOFILL_ENABLE_PAYMENT_SETTINGS_CARD_PROMO_AND_SCAN_CARD);
    }

    /**
     * Opens the scanner activity for a user to scan a credit card.
     *
     * @param tracker The intent request tracker used to launch the scanner intent from.
     */
    public void scan(IntentRequestTracker tracker) {
        RecordUserAction.record(SCAN_CARD_CLICKED_USER_ACTION);
        mScanner.scan(tracker);
    }

    /**
     * Logs the scan card result. If the scanner is not available (so the user could not have used
     * it), the result is not recorded.
     */
    public void logScanResult() {
        // Only log the result if it hasn't already been logged and the scanner is available.
        if (!mScanResultLogged && canScan()) {
            mScanResultLogged = true;
            RecordHistogram.recordEnumeratedHistogram(
                    SCAN_CARD_RESULT_HISTOGRAM,
                    mScanResult,
                    CreditCardScannerManager.ScanResult.COUNT);
        }
    }

    /**
     * Update the scan result based on the field that was edited. If an UNKNOWN field is used, it is
     * assumed that the field was not a field that was filled by the scanner. The scan result can
     * change based on whether the field that was edited was filled or not filled by the scanner.
     *
     * @param field The field that was edited.
     */
    public void fieldEdited(FieldType field) {
        if (!canScan()) {
            return;
        }

        if (mFieldsFilledByScanner.contains(field)
                && (mScanResult == ScanResult.SCANNED_WITHOUT_ADDITIONAL_USER_EDITS
                        || mScanResult == ScanResult.SCANNED_WITH_USER_EDITS_TO_UNSCANNED_FIELDS)) {
            mScanResult = ScanResult.SCANNED_WITH_USER_EDITS_TO_SCANNED_FIELDS;
        } else if (mScanResult == ScanResult.SCANNED_WITHOUT_ADDITIONAL_USER_EDITS) {
            mScanResult = ScanResult.SCANNED_WITH_USER_EDITS_TO_UNSCANNED_FIELDS;
        }
    }

    public void formClosed() {
        if ((mScanResult == ScanResult.SCANNED_WITHOUT_ADDITIONAL_USER_EDITS)
                || (mScanResult == ScanResult.SCANNED_WITH_USER_EDITS_TO_UNSCANNED_FIELDS)
                || (mScanResult == ScanResult.SCANNED_WITH_USER_EDITS_TO_SCANNED_FIELDS)) {
            mScanResult = ScanResult.SCANNED_BUT_USER_CLOSED_FORM;
            logScanResult();
        }
    }

    @Override
    public void onScanCancelled() {
        mScanResult = ScanResult.CANCELLED;
    }

    @Override
    public void onScanCompleted(
            String cardholderName, String cardNumber, int expirationMonth, int expirationYear) {
        if (!TextUtils.isEmpty(cardholderName)) {
            mFieldsFilledByScanner.add(FieldType.NAME);
        }

        if (!TextUtils.isEmpty(cardNumber)) {
            mFieldsFilledByScanner.add(FieldType.NUMBER);
        }

        if (expirationMonth != 0) {
            mFieldsFilledByScanner.add(FieldType.MONTH);
        }

        if (expirationYear != 0) {
            mFieldsFilledByScanner.add(FieldType.YEAR);
        }

        mDelegate.onScanCompleted(cardholderName, cardNumber, expirationMonth, expirationYear);
        mScanResult = CreditCardScannerManager.ScanResult.SCANNED_WITHOUT_ADDITIONAL_USER_EDITS;
    }

    public @ScanResult int getScanResultForTesting() {
        return mScanResult;
    }

    public void setScanResultForTesting(@ScanResult int result) {
        mScanResult = result;
    }

    public Set<FieldType> getFieldsFilledByScannerForTesting() {
        return mFieldsFilledByScanner;
    }
}
