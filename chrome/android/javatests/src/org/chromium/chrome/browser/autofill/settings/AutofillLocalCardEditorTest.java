// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Bundle;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.autofill.AutofillEditorBase;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.CreditCardScanner;
import org.chromium.chrome.browser.autofill.CreditCardScanner.Delegate;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.autofill.settings.CreditCardScannerManager.FieldType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/**
 * Instrumentation tests for {@link AutofillLocalCardEditor}.
 *
 * <p>TODO: crbug.com/391100396 - Rewrite these tests using Robolectric.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillLocalCardEditorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public final AutofillTestRule rule = new AutofillTestRule();

    @Rule
    public final SettingsActivityTestRule<AutofillLocalCardEditor> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillLocalCardEditor.class);

    private static final CreditCard SAMPLE_LOCAL_CARD =
            new CreditCard(
                    /* guid= */ "",
                    /* origin= */ "",
                    /* isLocal= */ true,
                    /* name= */ "John Doe",
                    /* number= */ "4444333322221111",
                    /* networkAndLastFourDigits= */ "",
                    /* month= */ "5",
                    AutofillTestHelper.nextYear(),
                    /* basicCardIssuerNetwork= */ "visa",
                    /* issuerIconDrawableId= */ 0,
                    /* billingAddressId= */ "",
                    /* serverId= */ "");
    private static final CreditCard SAMPLE_LOCAL_CARD_WITH_CVC =
            new CreditCard(
                    /* guid= */ "",
                    /* origin= */ "",
                    /* isLocal= */ true,
                    /* isVirtual= */ false,
                    /* name= */ "John Doe",
                    /* number= */ "4444111111111111",
                    /* networkAndLastFourDigits= */ "",
                    /* month= */ "5",
                    AutofillTestHelper.nextYear(),
                    /* basicCardIssuerNetwork= */ "visa",
                    /* issuerIconDrawableId= */ 0,
                    /* billingAddressId= */ "",
                    /* serverId= */ "",
                    /* instrumentId= */ 0,
                    /* cardLabel= */ "",
                    /* nickname= */ "",
                    /* cardArtUrl= */ null,
                    /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState
                            .UNENROLLED_AND_ELIGIBLE,
                    /* productDescription= */ "",
                    /* cardNameForAutofillDisplay= */ "",
                    /* obfuscatedLastFourDigits= */ "",
                    /* cvc= */ "123",
                    /* issuerId= */ "",
                    /* productTermsUrl= */ null);
    private static final CreditCard SAMPLE_AMEX_CARD_WITH_CVC =
            new CreditCard(
                    /* guid= */ "",
                    /* origin= */ "",
                    /* isLocal= */ true,
                    /* isVirtual= */ false,
                    /* name= */ "John Doe",
                    /* number= */ "378282246310005",
                    /* networkAndLastFourDigits= */ "",
                    /* month= */ "5",
                    AutofillTestHelper.nextYear(),
                    /* basicCardIssuerNetwork= */ "amex",
                    /* issuerIconDrawableId= */ 0,
                    /* billingAddressId= */ "",
                    /* serverId= */ "",
                    /* instrumentId= */ 0,
                    /* cardLabel= */ "",
                    /* nickname= */ "",
                    /* cardArtUrl= */ null,
                    /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState
                            .UNENROLLED_AND_ELIGIBLE,
                    /* productDescription= */ "",
                    /* cardNameForAutofillDisplay= */ "",
                    /* obfuscatedLastFourDigits= */ "",
                    /* cvc= */ "1234",
                    /* issuerId= */ "",
                    /* productTermsUrl= */ null);

    private static final CreditCard SAMPLE_MASKED_SERVER_CARD =
            new CreditCard(
                    /* guid= */ "1",
                    /* origin= */ "",
                    /* isLocal= */ false,
                    /* isVirtual= */ false,
                    /* name= */ "John Doe",
                    /* number= */ "4444222211111111",
                    /* networkAndLastFourDigits= */ "",
                    /* month= */ "5",
                    AutofillTestHelper.nextYear(),
                    /* basicCardIssuerNetwork= */ "visa",
                    /* issuerIconDrawableId= */ 0,
                    /* billingAddressId= */ "",
                    /* serverId= */ "",
                    /* instrumentId= */ 123,
                    /* cardLabel= */ "",
                    /* nickname= */ "",
                    /* cardArtUrl= */ null,
                    /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState.ENROLLED,
                    /* productDescription= */ "",
                    /* cardNameForAutofillDisplay= */ "",
                    /* obfuscatedLastFourDigits= */ "• • • • 1111",
                    /* cvc= */ "",
                    /* issuerId= */ "",
                    /* productTermsUrl= */ null);

    private static final String AMEX_CARD_NUMBER = "378282246310005";
    private static final String AMEX_CARD_NUMBER_PREFIX = "37";
    private static final String NON_AMEX_CARD_NUMBER = "4111111111111111";
    private static final String NON_AMEX_CARD_NUMBER_PREFIX = "41";

    @Mock private ObservableSupplierImpl<ModalDialogManager> mModalDialogManagerSupplierMock;
    @Mock private PersonalDataManager mPersonalDataManagerMock;
    private AutofillTestHelper mAutofillTestHelper;
    private UserActionTester mActionTester;
    @Mock private CreditCardScanner mScanner;
    @Mock private CreditCardScannerManager mScannerManager;

    @Before
    public void setUp() {
        mAutofillTestHelper = new AutofillTestHelper();
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

    private Bundle fragmentArgs(String guid) {
        Bundle args = new Bundle();
        args.putString(AutofillEditorBase.AUTOFILL_GUID, guid);
        return args;
    }

    @Test
    @MediumTest
    public void deleteCreditCardConfirmationDialog_deleteEntryCanceled_dialogDismissed()
            throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();

        PersonalDataManagerFactory.setInstanceForTesting(mPersonalDataManagerMock);

        FakeModalDialogManager fakeModalDialogManager =
                new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
        openDeletePaymentMethodConfirmationDialog(
                autofillLocalCardEditorFragment, fakeModalDialogManager);

        // Verify the dialog is open
        assertNotNull(fakeModalDialogManager.getShownDialogModel());
        ThreadUtils.runOnUiThreadBlocking(() -> fakeModalDialogManager.clickNegativeButton());

        // Verify the dialog is closed
        assertNull(fakeModalDialogManager.getShownDialogModel());

        // Verify the card entry is not deleted
        verify(mPersonalDataManagerMock, never()).deleteCreditCard(guid);
    }

    @Test
    @MediumTest
    public void
            deleteCreditCardConfirmationDialog_deleteEntryConfirmed_dialogDismissedAndEntryDeleted()
                    throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();

        PersonalDataManagerFactory.setInstanceForTesting(mPersonalDataManagerMock);

        FakeModalDialogManager fakeModalDialogManager =
                new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
        openDeletePaymentMethodConfirmationDialog(
                autofillLocalCardEditorFragment, fakeModalDialogManager);

        // Verify the dialog is open
        assertNotNull(fakeModalDialogManager.getShownDialogModel());
        ThreadUtils.runOnUiThreadBlocking(() -> fakeModalDialogManager.clickPositiveButton());

        // Verify the dialog is closed
        assertNull(fakeModalDialogManager.getShownDialogModel());

        // Verify the card entry is deleted
        verify(mPersonalDataManagerMock, times(1)).deleteCreditCard(guid);
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testRecordHistogram_whenNewCreditCardIsAddedWithoutCvc() throws Exception {
        // Set 4 existing cards.
        mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);
        mAutofillTestHelper.setCreditCard(SAMPLE_AMEX_CARD_WITH_CVC);
        mAutofillTestHelper.addServerCreditCard(SAMPLE_MASKED_SERVER_CARD);

        // Expect histogram to record 4 for 4 existing cards.
        HistogramWatcher saveCardCountHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AutofillLocalCardEditor.CARD_COUNT_BEFORE_ADDING_NEW_CARD_HISTOGRAM,
                                4)
                        .build();

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setCardNumberOnEditor(autofillLocalCardEditorFragment, NON_AMEX_CARD_NUMBER);
        setExpirationMonthSelectionOnEditor(
                autofillLocalCardEditorFragment, /* monthSelection= */ 1);
        setExpirationYearSelectionOnEditor(autofillLocalCardEditorFragment, /* yearSelection= */ 1);
        performButtonClickOnEditor(autofillLocalCardEditorFragment.mDoneButton);

        saveCardCountHistogram.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testRecordHistogram_whenNewCreditCardIsAddedWithCvc() throws Exception {
        // Set 4 existing cards.
        mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);
        mAutofillTestHelper.setCreditCard(SAMPLE_AMEX_CARD_WITH_CVC);
        mAutofillTestHelper.addServerCreditCard(SAMPLE_MASKED_SERVER_CARD);

        // Expect histogram to record 4 for 4 existing cards.
        HistogramWatcher saveCardCountHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AutofillLocalCardEditor.CARD_COUNT_BEFORE_ADDING_NEW_CARD_HISTOGRAM,
                                4)
                        .build();

        // Expect histogram to record false for adding a with existing cards.
        HistogramWatcher saveCardWithoutExistingCardsHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                AutofillLocalCardEditor.CARD_ADDED_WITHOUT_EXISTING_CARDS_HISTOGRAM,
                                false)
                        .build();

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setCardNumberOnEditor(autofillLocalCardEditorFragment, NON_AMEX_CARD_NUMBER);
        setExpirationDateOnEditor(
                autofillLocalCardEditorFragment,
                String.format("12/%s", AutofillTestHelper.nextYear().substring(2)));
        setSecurityCodeOnEditor(autofillLocalCardEditorFragment, /* code= */ "321");
        performButtonClickOnEditor(autofillLocalCardEditorFragment.mDoneButton);

        saveCardCountHistogram.assertExpected();
        saveCardWithoutExistingCardsHistogram.assertExpected();
    }

    private void openDeletePaymentMethodConfirmationDialog(
            AutofillLocalCardEditor autofillLocalCardEditorFragment,
            ModalDialogManager modalDialogManager) {
        when(mModalDialogManagerSupplierMock.get()).thenReturn(modalDialogManager);
        autofillLocalCardEditorFragment.setModalDialogManagerSupplier(
                mModalDialogManagerSupplierMock);

        MenuItem deleteButton = mock(MenuItem.class);
        when(deleteButton.getItemId()).thenReturn(R.id.delete_menu_id);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    autofillLocalCardEditorFragment.onOptionsItemSelected(deleteButton);
                });
    }

    private void setExpirationMonthSelectionOnEditor(
            AutofillLocalCardEditor autofillLocalCardEditorFragment, int monthSelection) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mExpirationMonth.setSelection(
                                monthSelection);
                    } catch (Exception e) {
                        throw new AssertionError("Failed to set the Expiration Month", e);
                    }
                });
    }

    private void setExpirationYearSelectionOnEditor(
            AutofillLocalCardEditor autofillLocalCardEditorFragment, int yearSelection) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mExpirationYear.setSelection(yearSelection);
                    } catch (Exception e) {
                        throw new AssertionError("Failed to set the Expiration Year", e);
                    }
                });
    }

    private void setExpirationDateOnEditor(
            AutofillLocalCardEditor autofillLocalCardEditorFragment, String date) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mExpirationDate.setText(date);
                    } catch (Exception e) {
                        throw new AssertionError("Failed to set the Expiration Date", e);
                    }
                });
    }

    @Test
    @SmallTest
    public void getExpirationMonth_whenDoubleDigitMonth_returnsMonth() throws Exception {
        assertThat(AutofillLocalCardEditor.getExpirationMonth("12/23")).isEqualTo("12");
    }

    @Test
    @SmallTest
    public void getExpirationMonth_whenSingleDigitMonth_returnsMonthWithoutLeadingZero()
            throws Exception {
        assertThat(AutofillLocalCardEditor.getExpirationMonth("02/23")).isEqualTo("2");
    }

    @Test
    @SmallTest
    public void getExpirationYear_returnsYearWithPrefix() throws Exception {
        assertThat(AutofillLocalCardEditor.getExpirationYear("12/23")).isEqualTo("2023");
    }

    @Test
    @SmallTest
    public void testIsAmExCard_whenAmExCardNumberPrefixIsEntered_returnsTrue() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        assertThat(AutofillLocalCardEditor.isAmExCard(AMEX_CARD_NUMBER_PREFIX))
                                .isTrue();
                    } catch (Exception e) {
                        throw new AssertionError("Failed to verify AmEx card.", e);
                    }
                });
    }

    @Test
    @SmallTest
    public void testIsAmExCard_whenNonAmExCardNumberPrefixIsEntered_returnsFalse() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        assertThat(AutofillLocalCardEditor.isAmExCard(NON_AMEX_CARD_NUMBER_PREFIX))
                                .isFalse();
                    } catch (Exception e) {
                        throw new AssertionError("Failed to verify AmEx card.", e);
                    }
                });
    }

    private void setCardNumberOnEditor(
            AutofillLocalCardEditor autofillLocalCardEditorFragment, String cardNumber) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mNumberText.setText(cardNumber);
                    } catch (Exception e) {
                        throw new AssertionError("Failed to set the card number", e);
                    }
                });
    }

    private void setSecurityCodeOnEditor(
            AutofillLocalCardEditor autofillLocalCardEditorFragment, String code) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mCvc.setText(code);
                    } catch (Exception e) {
                        throw new AssertionError("Failed to set the security code", e);
                    }
                });
    }

    private void setCardNameOnEditor(
            AutofillLocalCardEditor autofillLocalCardEditorFragment, String cardName) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mNameText.setText(cardName);
                    } catch (Exception e) {
                        throw new AssertionError("Failed to set the card name", e);
                    }
                });
    }

    private void performButtonClickOnEditor(Button button) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        button.performClick();
                    } catch (Exception e) {
                        throw new AssertionError("Failed to click the button", e);
                    }
                });
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_PAYMENT_SETTINGS_CARD_PROMO_AND_SCAN_CARD})
    public void scannerFeatureDisabled_scanButtonIsHidden() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();

        assertEquals(View.GONE, autofillLocalCardEditorFragment.mScanButton.getVisibility());
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_PAYMENT_SETTINGS_CARD_PROMO_AND_SCAN_CARD})
    public void scannerFeatureEnabled_scanButtonIsVisible() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();

        assertEquals(View.VISIBLE, autofillLocalCardEditorFragment.mScanButton.getVisibility());
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_PAYMENT_SETTINGS_CARD_PROMO_AND_SCAN_CARD})
    public void scannerCannotScan_scanButtonIsHidden() {
        when(mScanner.canScan()).thenReturn(false);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();

        assertEquals(View.GONE, autofillLocalCardEditorFragment.mScanButton.getVisibility());
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_PAYMENT_SETTINGS_CARD_PROMO_AND_SCAN_CARD})
    public void scannerButtonClicked_scanIsCalled() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();

        performButtonClickOnEditor(autofillLocalCardEditorFragment.mScanButton);
        verify(mScanner).scan(activity.getIntentRequestTracker());
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE
    }) // Enabled for the expiration date to be used.
    public void onScanCompleted_cardDataIsAdded() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        CreditCard card = SAMPLE_LOCAL_CARD;
        // Explicitly set the month to single digit to test padding.
        card.setMonth("5");

        assertTrue(autofillLocalCardEditorFragment.mNameText.getText().toString().isEmpty());
        assertTrue(autofillLocalCardEditorFragment.mNumberText.getText().toString().isEmpty());
        assertTrue(autofillLocalCardEditorFragment.mExpirationDate.getText().toString().isEmpty());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.onScanCompleted(
                                card.getName(),
                                card.getNumber(),
                                Integer.parseInt(card.getMonth()),
                                Integer.parseInt(card.getYear()));
                    } catch (Exception e) {
                        throw new AssertionError("Failed to run on scan completed", e);
                    }
                });

        assertThat(autofillLocalCardEditorFragment.mNameText.getText().toString())
                .isEqualTo(card.getName());
        assertThat(
                        autofillLocalCardEditorFragment
                                .mNumberText
                                .getText()
                                .toString()
                                .replaceAll(" ", ""))
                .isEqualTo(card.getNumber());
        assertThat(autofillLocalCardEditorFragment.mExpirationDate.getText().toString())
                .isEqualTo(String.format("0%s/%s", card.getMonth(), card.getYear().substring(2)));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void paymentSettingsOnScanCompleted_twoDigitMonth() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        CreditCard card = SAMPLE_LOCAL_CARD;
        // Ensure a two-digit month is formatted as-is.
        card.setMonth("10");

        assertTrue(autofillLocalCardEditorFragment.mExpirationDate.getText().toString().isEmpty());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.onScanCompleted(
                                card.getName(),
                                card.getNumber(),
                                Integer.parseInt(card.getMonth()),
                                Integer.parseInt(card.getYear()));
                    } catch (Exception e) {
                        throw new AssertionError("Failed to run on scan completed", e);
                    }
                });

        assertThat(autofillLocalCardEditorFragment.mExpirationDate.getText().toString())
                .isEqualTo(String.format("%s/%s", card.getMonth(), card.getYear().substring(2)));
    }

    @Test
    @MediumTest
    @DisableFeatures({
        ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE
    }) // Disabled so cards without expiration dates can be saved.
    public void onCardSave_scannerManagerLogScanResultIsCalled() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        autofillLocalCardEditorFragment.setCreditCardScannerManagerForTesting(mScannerManager);

        setCardNumberOnEditor(autofillLocalCardEditorFragment, SAMPLE_LOCAL_CARD.getNumber());
        performButtonClickOnEditor(autofillLocalCardEditorFragment.mDoneButton);

        verify(mScannerManager).logScanResult();
    }

    @Test
    @MediumTest
    public void onFinishPage_scannerManagerFormClosedIsCalled() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        autofillLocalCardEditorFragment.setCreditCardScannerManagerForTesting(mScannerManager);

        autofillLocalCardEditorFragment.finishPage();

        verify(mScannerManager).formClosed();
    }

    @Test
    @MediumTest
    public void nameFieldEdited_scannerManagerFieldEditedIsCalledWithName() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        autofillLocalCardEditorFragment.setCreditCardScannerManagerForTesting(mScannerManager);

        setCardNameOnEditor(autofillLocalCardEditorFragment, "Okarun");

        verify(mScannerManager).fieldEdited(FieldType.NAME);
    }

    @Test
    @MediumTest
    public void numberFieldEdited_scannerManagerFieldEditedIsCalledWithNumber() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        autofillLocalCardEditorFragment.setCreditCardScannerManagerForTesting(mScannerManager);

        setCardNumberOnEditor(autofillLocalCardEditorFragment, NON_AMEX_CARD_NUMBER);

        // Field edit may be called more than once because there are other listeners for the number
        // field that format the number as it's entered into the text field.
        verify(mScannerManager, atLeastOnce()).fieldEdited(FieldType.NUMBER);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void expirationDateFieldEdited_scannerManagerFieldEditedIsCalledWithMonthAndYear() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        autofillLocalCardEditorFragment.setCreditCardScannerManagerForTesting(mScannerManager);

        setExpirationDateOnEditor(autofillLocalCardEditorFragment, "10/95");

        verify(mScannerManager).fieldEdited(FieldType.MONTH);
        verify(mScannerManager).fieldEdited(FieldType.YEAR);
    }

    @Test
    @MediumTest
    public void cvcFieldEdited_scannerManagerFieldEditedIsCalledWithUnknown() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        autofillLocalCardEditorFragment.setCreditCardScannerManagerForTesting(mScannerManager);

        setSecurityCodeOnEditor(autofillLocalCardEditorFragment, "101");

        verify(mScannerManager).fieldEdited(FieldType.UNKNOWN);
    }
}
