// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.autofill.settings.CreditCardScannerManager.SCANNER_CANNOT_SCAN_USER_ACTION;
import static org.chromium.chrome.browser.autofill.settings.CreditCardScannerManager.SCAN_CARD_CLICKED_USER_ACTION;
import static org.chromium.chrome.browser.autofill.settings.CreditCardScannerManager.SCAN_CARD_RESULT_HISTOGRAM;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.autofill.CreditCardScanner;
import org.chromium.chrome.browser.autofill.CreditCardScanner.Delegate;
import org.chromium.chrome.browser.autofill.settings.CreditCardScannerManager.FieldType;
import org.chromium.chrome.browser.autofill.settings.CreditCardScannerManager.ScanResult;
import org.chromium.ui.base.IntentRequestTracker;

import java.util.Set;

/** Unit tests for {@link CreditCardScannerManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CreditCardScannerManagerTest {
    @Rule public final MockitoRule mockito = MockitoJUnit.rule();

    @Mock private CreditCardScanner mScanner;
    @Mock private CreditCardScannerManager.Delegate mDelegate;
    @Mock private IntentRequestTracker mTracker;

    private UserActionTester mActionTester;

    @Before
    public void setUp() {
        mActionTester = new UserActionTester();

        CreditCardScanner.setFactory(
                new CreditCardScanner.Factory() {
                    @Override
                    public CreditCardScanner create(Delegate delegate) {
                        return mScanner;
                    }
                });
        when(mScanner.canScan()).thenReturn(true);
    }

    @After
    public void tearDown() {
        mActionTester.tearDown();
    }

    @Test
    @SmallTest
    public void onCreate() {
        CreditCardScannerManager manager = new CreditCardScannerManager(mDelegate);

        assertTrue(manager.canScan());
        assertEquals(ScanResult.IGNORED, manager.getScanResultForTesting());
        assertNotNull(manager.getFieldsFilledByScannerForTesting());
        assertEquals(0, manager.getFieldsFilledByScannerForTesting().size());
        assertFalse(
                "ScannerCannotScan user action should not be logged.",
                mActionTester.getActions().contains(SCANNER_CANNOT_SCAN_USER_ACTION));
    }

    @Test
    @SmallTest
    public void onCreateWithScanDisabled() {
        when(mScanner.canScan()).thenReturn(false);
        CreditCardScannerManager manager = new CreditCardScannerManager(mDelegate);

        assertFalse(manager.canScan());
        assertEquals(ScanResult.UNKNOWN, manager.getScanResultForTesting());
        assertTrue(
                "ScannerCannotScan user action should be logged.",
                mActionTester.getActions().contains(SCANNER_CANNOT_SCAN_USER_ACTION));
    }

    @Test
    @SmallTest
    public void onScan() {
        CreditCardScannerManager manager = new CreditCardScannerManager(mDelegate);

        manager.scan(mTracker);

        verify(mScanner).scan(mTracker);

        assertTrue(
                "ScanCardClicked user action should be logged.",
                mActionTester.getActions().contains(SCAN_CARD_CLICKED_USER_ACTION));
    }

    @Test
    @SmallTest
    public void onLogScanResultWithScanDisabled() {
        when(mScanner.canScan()).thenReturn(false);
        HistogramWatcher scanCardResultHistogram =
                HistogramWatcher.newBuilder().expectNoRecords(SCAN_CARD_RESULT_HISTOGRAM).build();
        CreditCardScannerManager manager = new CreditCardScannerManager(mDelegate);

        manager.logScanResult();

        scanCardResultHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void onLogScanResultWithScanEnabled() {
        for (@ScanResult int result = 0; result < ScanResult.COUNT; result++) {
            CreditCardScannerManager manager = new CreditCardScannerManager(mDelegate);
            HistogramWatcher scanCardResultHistogram =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(SCAN_CARD_RESULT_HISTOGRAM, result)
                            .build();
            manager.setScanResultForTesting(result);

            manager.logScanResult();

            scanCardResultHistogram.assertExpected();
        }
    }

    @Test
    @SmallTest
    public void onLogScanResultMultipleTimes() {
        CreditCardScannerManager manager = new CreditCardScannerManager(mDelegate);
        HistogramWatcher scanCardResultHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                SCAN_CARD_RESULT_HISTOGRAM,
                                ScanResult.SCANNED_WITHOUT_ADDITIONAL_USER_EDITS,
                                /* times= */ 1)
                        .build();
        manager.setScanResultForTesting(ScanResult.SCANNED_WITHOUT_ADDITIONAL_USER_EDITS);

        manager.logScanResult();
        manager.logScanResult();

        scanCardResultHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void onFieldEditWithScanDisabled() {
        when(mScanner.canScan()).thenReturn(false);
        CreditCardScannerManager manager = new CreditCardScannerManager(mDelegate);
        manager.setScanResultForTesting(ScanResult.SCANNED_WITHOUT_ADDITIONAL_USER_EDITS);

        manager.fieldEdited(FieldType.UNKNOWN);

        assertEquals(
                ScanResult.SCANNED_WITHOUT_ADDITIONAL_USER_EDITS,
                manager.getScanResultForTesting());
    }

    @Test
    @SmallTest
    public void onFieldEditToUnscannedField() {
        CreditCardScannerManager manager = new CreditCardScannerManager(mDelegate);
        manager.setScanResultForTesting(ScanResult.SCANNED_WITHOUT_ADDITIONAL_USER_EDITS);

        manager.fieldEdited(FieldType.UNKNOWN);

        assertEquals(
                ScanResult.SCANNED_WITH_USER_EDITS_TO_UNSCANNED_FIELDS,
                manager.getScanResultForTesting());
    }

    @Test
    @SmallTest
    public void onFieldEditToScannedField() {
        CreditCardScannerManager manager = new CreditCardScannerManager(mDelegate);
        manager.setScanResultForTesting(ScanResult.SCANNED_WITHOUT_ADDITIONAL_USER_EDITS);
        FieldType field = FieldType.NUMBER;
        manager.getFieldsFilledByScannerForTesting().add(field);

        manager.fieldEdited(field);

        assertEquals(
                ScanResult.SCANNED_WITH_USER_EDITS_TO_SCANNED_FIELDS,
                manager.getScanResultForTesting());
    }

    @Test
    @SmallTest
    public void onFormClosedWithScanResultScanned() {
        CreditCardScannerManager manager = new CreditCardScannerManager(mDelegate);
        HistogramWatcher scanCardResultHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SCAN_CARD_RESULT_HISTOGRAM, ScanResult.SCANNED_BUT_USER_CLOSED_FORM)
                        .build();
        manager.setScanResultForTesting(ScanResult.SCANNED_WITHOUT_ADDITIONAL_USER_EDITS);

        manager.formClosed();

        scanCardResultHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void onFormClosedWithScanResultIgnored() {
        CreditCardScannerManager manager = new CreditCardScannerManager(mDelegate);
        HistogramWatcher scanCardResultHistogram =
                HistogramWatcher.newBuilder().expectNoRecords(SCAN_CARD_RESULT_HISTOGRAM).build();

        manager.formClosed();

        scanCardResultHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void onFormClosedWhileAlreadyLogged() {
        CreditCardScannerManager manager = new CreditCardScannerManager(mDelegate);
        // Only the `SCANNED_WITHOUT_ADDITIONAL_USER_EDITS` should be logged and
        // `SCANNED_BUT_USER_CLOSED_FORM` should not. Note that this works because the histogram
        // watcher assert fails if there are extra unexpected histogram values.
        HistogramWatcher scanCardResultHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SCAN_CARD_RESULT_HISTOGRAM,
                                ScanResult.SCANNED_WITHOUT_ADDITIONAL_USER_EDITS)
                        .build();
        manager.setScanResultForTesting(ScanResult.SCANNED_WITHOUT_ADDITIONAL_USER_EDITS);
        manager.logScanResult();

        manager.formClosed();

        scanCardResultHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void onScanCancelled() {
        CreditCardScannerManager manager = new CreditCardScannerManager(mDelegate);

        assertEquals(ScanResult.IGNORED, manager.getScanResultForTesting());

        manager.onScanCancelled();

        assertEquals(ScanResult.CANCELLED, manager.getScanResultForTesting());
    }

    @Test
    @SmallTest
    public void onScanCompleted() {
        CreditCardScannerManager manager = new CreditCardScannerManager(mDelegate);

        String cardNumber = "4444333322221111";
        int expirationYear = 1995;
        manager.onScanCompleted(
                /* cardholderName= */ "", cardNumber, /* expirationMonth= */ 0, expirationYear);

        Set<FieldType> fieldTypes = manager.getFieldsFilledByScannerForTesting();

        // Only fields populated with real values (i.e. card number and expiration year) should be
        // in the set as fields filled by the scanner.
        assertFalse(fieldTypes.contains(FieldType.UNKNOWN));
        assertFalse(fieldTypes.contains(FieldType.NAME));
        assertTrue(fieldTypes.contains(FieldType.NUMBER));
        assertFalse(fieldTypes.contains(FieldType.MONTH));
        assertTrue(fieldTypes.contains(FieldType.YEAR));

        assertEquals(
                ScanResult.SCANNED_WITHOUT_ADDITIONAL_USER_EDITS,
                manager.getScanResultForTesting());
        verify(mDelegate)
                .onScanCompleted(
                        /* cardholderName= */ "",
                        cardNumber,
                        /* expirationMonth= */ 0,
                        expirationYear);
    }
}
