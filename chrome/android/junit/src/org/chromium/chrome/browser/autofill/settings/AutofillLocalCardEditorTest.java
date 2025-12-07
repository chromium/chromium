// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Intent;
import android.graphics.drawable.BitmapDrawable;
import android.os.Bundle;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.Spinner;

import androidx.annotation.DrawableRes;
import androidx.test.core.app.ActivityScenario;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import com.google.android.material.textfield.TextInputLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.build.NullUtil;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.CreditCardScanner;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.autofill.PersonalDataManagerJni;
import org.chromium.chrome.browser.autofill.settings.CreditCardScannerManager.FieldType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileManagerUtilsJni;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsIntentUtil;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

import java.util.List;

/** Unit tests for {@link AutofillLocalCardEditorTest}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
@DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
public class AutofillLocalCardEditorTest {
    // This is a non-amex card without a CVC code.
    private static CreditCard getSampleLocalCard() {
        return new CreditCard(
                /* guid= */ "",
                /* origin= */ "",
                /* isLocal= */ true,
                /* name= */ "John Doe",
                /* number= */ NON_AMEX_CARD_NUMBER,
                /* networkAndLastFourDigits= */ "",
                /* month= */ "05",
                AutofillTestHelper.nextYear(),
                /* basicCardIssuerNetwork= */ "visa",
                /* issuerIconDrawableId= */ 0,
                /* billingAddressId= */ "",
                /* serverId= */ "");
    }

    // This is a non-amex card with a CVC code.
    private static CreditCard getSampleLocalCardWithCvc() {
        return new CreditCard(
                /* guid= */ "",
                /* origin= */ "",
                /* isLocal= */ true,
                /* isVirtual= */ false,
                /* name= */ "John Doe",
                /* number= */ NON_AMEX_CARD_NUMBER,
                /* networkAndLastFourDigits= */ "",
                /* month= */ "05",
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
                /* benefitSource= */ "",
                /* productTermsUrl= */ null);
    }

    // This is an amex card with a CVC code.
    private static CreditCard getSampleAmexCardWithCvc() {
        return new CreditCard(
                /* guid= */ "",
                /* origin= */ "",
                /* isLocal= */ true,
                /* isVirtual= */ false,
                /* name= */ "John Doe",
                /* number= */ AMEX_CARD_NUMBER,
                /* networkAndLastFourDigits= */ "",
                /* month= */ "05",
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
                /* benefitSource= */ "",
                /* productTermsUrl= */ null);
    }

    private static final String AMEX_CARD_NUMBER = "378282246310005";
    private static final String AMEX_CARD_NUMBER_PREFIX = "37";
    private static final String NON_AMEX_CARD_NUMBER = "4111111111111111";
    private static final String NON_AMEX_CARD_NUMBER_PREFIX = "41";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mMockProfile;
    @Mock private PersonalDataManager mMockPersonalDataManager;
    @Mock private PersonalDataManager.Natives mMockPersonalDataManagerJni;
    @Mock private SettingsNavigation mMockSettingsNavigation;
    @Mock private ChromeBrowserInitializer mMockInitializer;
    @Mock private CreditCardScanner mMockScanner;
    @Mock private CreditCardScannerManager mMockScannerManager;
    @Mock private ProfileManagerUtilsJni mMockProfileManagerUtilsJni;

    private UserActionTester mActionTester;

    private Button mDoneButton;
    private Button mScanButton;
    private EditText mNicknameText;
    private TextInputLayout mNicknameLabel;

    private Spinner mExpirationMonth;
    private Spinner mExpirationYear;
    private EditText mExpirationDate;
    private EditText mCvc;

    private EditText mNumberText;
    private EditText mNameText;

    private ImageView mCvcHintImage;

    private String mNicknameInvalidError;
    private String mExpirationDateInvalidError;
    private String mExpiredCardError;

    private ActivityScenario<SettingsActivity> mActivityScenario;
    private SettingsActivity mSettingsActivity;
    private AutofillLocalCardEditor mCardEditor;

    @Before
    public void setUp() {
        PersonalDataManagerJni.setInstanceForTesting(mMockPersonalDataManagerJni);
        // Mock a card recognition logic
        when(mMockPersonalDataManagerJni.getBasicCardIssuerNetwork(anyString(), anyBoolean()))
                .thenAnswer(
                        new Answer<>() {
                            @Override
                            public String answer(InvocationOnMock invocation) throws Throwable {
                                String cardNumber = invocation.getArgument(0);
                                if (cardNumber.startsWith(AMEX_CARD_NUMBER_PREFIX)) {
                                    return "amex";
                                }
                                return "visa";
                            }
                        });
        when(mMockPersonalDataManager.getCreditCardForNumber(NON_AMEX_CARD_NUMBER))
                .thenReturn(getSampleLocalCard());
        when(mMockScanner.canScan()).thenReturn(true);

        PersonalDataManagerFactory.setInstanceForTesting(mMockPersonalDataManager);
        SettingsNavigationFactory.setInstanceForTesting(mMockSettingsNavigation);
        ProfileManagerUtilsJni.setInstanceForTesting(mMockProfileManagerUtilsJni);
        ChromeBrowserInitializer.setForTesting(mMockInitializer);
        ProfileManager.setLastUsedProfileForTesting(mMockProfile);
        mActionTester = new UserActionTester();

        CreditCardScanner.setFactory(delegate -> mMockScanner);
    }

    @After
    public void tearDown() {
        if (mActivityScenario != null) {
            mActivityScenario.close();
        }
        mActionTester.tearDown();
    }

    private void initFragment(CreditCard card) {
        Bundle arguments = new Bundle();
        if (card != null) {
            String guid = card.getGUID();
            arguments.putString("guid", guid);
            when(mMockPersonalDataManager.getCreditCard(guid)).thenReturn(card);
        }

        Intent intent =
                SettingsIntentUtil.createIntent(
                        ContextUtils.getApplicationContext(),
                        AutofillLocalCardEditor.class.getName(),
                        arguments);

        mActivityScenario = ActivityScenario.launch(intent);
        mActivityScenario.onActivity(
                activity -> {
                    mSettingsActivity = activity;
                    mSettingsActivity.setTheme(R.style.Theme_MaterialComponents);
                });

        mCardEditor = (AutofillLocalCardEditor) mSettingsActivity.getMainFragment();

        mDoneButton = mSettingsActivity.findViewById(R.id.button_primary);
        mNicknameText = mSettingsActivity.findViewById(R.id.credit_card_nickname_edit);
        mNicknameLabel = mSettingsActivity.findViewById(R.id.credit_card_nickname_label);
        mNicknameInvalidError =
                mSettingsActivity.getString(R.string.autofill_credit_card_editor_invalid_nickname);
        mExpirationMonth =
                mSettingsActivity.findViewById(R.id.autofill_credit_card_editor_month_spinner);
        mExpirationYear =
                mSettingsActivity.findViewById(R.id.autofill_credit_card_editor_year_spinner);
        mExpirationDate = mSettingsActivity.findViewById(R.id.expiration_month_and_year);

        View cvcLegacyContainer = mSettingsActivity.findViewById(R.id.cvc_legacy_container);
        TextInputLayout cvcMaterialLabel =
                mSettingsActivity.findViewById(R.id.credit_card_security_code_label_material);

        if (ChromeFeatureList.sAndroidSettingsContainment.isEnabled()) {
            cvcLegacyContainer.setVisibility(View.GONE);
            cvcMaterialLabel.setVisibility(View.VISIBLE);
            mCvc = NullUtil.assertNonNull(cvcMaterialLabel.getEditText());
        } else {
            cvcLegacyContainer.setVisibility(View.VISIBLE);
            cvcMaterialLabel.setVisibility(View.GONE);
            mCvc = mSettingsActivity.findViewById(R.id.cvc);
        }
        mCvcHintImage = mSettingsActivity.findViewById(R.id.cvc_hint_image);
        mNumberText = mSettingsActivity.findViewById(R.id.credit_card_number_edit);
        mExpirationDateInvalidError =
                mSettingsActivity.getString(
                        R.string.autofill_credit_card_editor_invalid_expiration_date);
        mExpiredCardError =
                mSettingsActivity.getString(R.string.autofill_credit_card_editor_expired_card);
        mScanButton = mSettingsActivity.findViewById(R.id.scan_card_button);
        mNameText = mSettingsActivity.findViewById(R.id.credit_card_name_edit);
    }

    private void openDeletePaymentMethodConfirmationDialog(ModalDialogManager modalDialogManager) {
        mCardEditor.setModalDialogManagerSupplier(() -> modalDialogManager);

        MenuItem deleteButton = mock(MenuItem.class);
        when(deleteButton.getItemId()).thenReturn(R.id.delete_menu_id);
        mCardEditor.onOptionsItemSelected(deleteButton);
    }

    /**
     * Verifies that a current {@code mCvcHintImage} is equal to the {@code expectedImage}.
     *
     * @param expectedImage The image resource id of the expected cvc image.
     */
    private void verifyCvcHintImage(@DrawableRes int expectedImage) {
        ImageView expectedCvcHintImage = new ImageView(ContextUtils.getApplicationContext());
        expectedCvcHintImage.setImageResource(expectedImage);
        BitmapDrawable expectedDrawable = (BitmapDrawable) expectedCvcHintImage.getDrawable();
        BitmapDrawable actualDrawable = (BitmapDrawable) mCvcHintImage.getDrawable();

        assertThat(actualDrawable).isNotNull();
        assertThat(expectedDrawable).isNotNull();

        assertTrue(expectedDrawable.getBitmap().sameAs(actualDrawable.getBitmap()));
    }

    @Test
    @MediumTest
    public void nicknameFieldEmpty_cardDoesNotHaveNickname() {
        initFragment(getSampleLocalCard());

        assertThat(mNicknameText.getText().toString()).isEmpty();
        assertTrue(mCardEditor.validateFormAndUpdateErrorAndFocusErrorField());
    }

    @Test
    @MediumTest
    public void nicknameFieldSet_cardHasNickname() {
        CreditCard card = getSampleLocalCard();
        String nickname = "test nickname";
        card.setNickname(nickname);
        initFragment(card);

        assertThat(mNicknameText.getText().toString()).isEqualTo(nickname);
        assertTrue(mCardEditor.validateFormAndUpdateErrorAndFocusErrorField());
    }

    @Test
    @MediumTest
    public void testNicknameFieldIsShown() {
        initFragment(getSampleLocalCard());
        // By default nickname label should be visible.
        assertThat(mNicknameLabel.getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    @MediumTest
    public void testInvalidNicknameShowsErrorMessage() {
        initFragment(getSampleLocalCard());
        // "Nickname 123" is an incorrect nickname because it contains digits.
        mNicknameText.setText("Nickname 123");

        assertThat(mNicknameLabel.getError()).isEqualTo(mNicknameInvalidError);
        // Since the nickname has an error, the form should not be valid.
        assertFalse(mCardEditor.validateFormAndUpdateErrorAndFocusErrorField());
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testErrorMessageHiddenAfterNicknameIsEditedFromInvalidToValid() {
        initFragment(getSampleLocalCard());
        // "Nickname 123" is an incorrect nickname because it contains digits.
        mNicknameText.setText("Nickname 123");

        assertThat(mNicknameLabel.getError()).isEqualTo(mNicknameInvalidError);

        // Set the nickname to valid one.
        mNicknameText.setText("Valid Nickname");
        assertThat(mNicknameLabel.getError()).isNull();
        assertTrue(mCardEditor.validateFormAndUpdateErrorAndFocusErrorField());
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testErrorMessageHiddenAfterNicknameIsEditedFromInvalidToEmpty() {
        initFragment(getSampleLocalCard());
        // "Nickname 123" is an incorrect nickname because it contains digits.
        mNicknameText.setText("Nickname 123");

        assertThat(mNicknameLabel.getError()).isEqualTo(mNicknameInvalidError);

        // Set the nickname to null.
        mNicknameText.setText(null);

        assertThat(mNicknameLabel.getError()).isNull();
        assertTrue(mCardEditor.validateFormAndUpdateErrorAndFocusErrorField());
    }

    @Test
    @MediumTest
    public void testNicknameLengthCappedAt25Characters() {
        initFragment(getSampleLocalCard());
        String veryLongNickname = "This is a very very long nickname";
        mNicknameText.setText(veryLongNickname);

        // The maximum nickname length is 25.
        assertThat(mNicknameText.getText().toString()).isEqualTo(veryLongNickname.substring(0, 25));
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testExpirationDateSpinnerAreShownWhenCvcFlagOff() {
        initFragment(getSampleLocalCardWithCvc());

        // When the flag is off, month and year fields should be visible.
        assertThat(mExpirationMonth.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(mExpirationYear.getVisibility()).isEqualTo(View.VISIBLE);

        assertTrue(mExpirationMonth.isShown());
        assertTrue(mExpirationYear.isShown());

        // The expiration date and the cvc fields should not be shown to the user.
        assertFalse(mExpirationDate.isShown());
        assertFalse(mCvc.isShown());
    }

    @Test
    @MediumTest
    public void testExpirationDateAndSecurityCodeFieldsAreShown() {
        initFragment(getSampleLocalCardWithCvc());

        // When the flag is on, expiration date and cvc fields should be visible.
        assertThat(mExpirationDate.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(mCvc.getVisibility()).isEqualTo(View.VISIBLE);

        assertTrue(mExpirationDate.isShown());
        assertTrue(mCvc.isShown());

        // When the flag is on, month and year fields shouldn't be visible.
        assertFalse(mExpirationMonth.isShown());
        assertFalse(mExpirationYear.isShown());
    }

    @Test
    @MediumTest
    public void securityCodeFieldSet_cardHasCvc() {
        CreditCard card = getSampleLocalCardWithCvc();
        String cvc = "234";
        card.setCvc(cvc);
        initFragment(card);

        assertThat(mCvc.getText().toString()).isEqualTo(cvc);
        assertTrue(mCardEditor.validateFormAndUpdateErrorAndFocusErrorField());
    }

    @Test
    @MediumTest
    public void testSecurityCode_whenAmExCardIsSet_usesAmExCvcHintImage() {
        initFragment(getSampleAmexCardWithCvc());

        verifyCvcHintImage(/* expectedImage= */ R.drawable.cvc_icon_amex);
    }

    @Test
    @MediumTest
    public void testSecurityCode_whenNonAmExCardIsSet_usesDefaultCvcHintImage() {
        initFragment(getSampleLocalCardWithCvc());

        verifyCvcHintImage(/* expectedImage= */ R.drawable.cvc_icon);
    }

    @Test
    @MediumTest
    public void testSecurityCode_whenCardIsNotSet_usesDefaultCvcHintImage() {
        initFragment(null);

        verifyCvcHintImage(/* expectedImage= */ R.drawable.cvc_icon);
    }

    @Test
    @MediumTest
    public void testSecurityCode_whenAmExCardNumberIsEntered_usesAmExCvcHintImage() {
        initFragment(getSampleLocalCardWithCvc());

        verifyCvcHintImage(/* expectedImage= */ R.drawable.cvc_icon);
        mNumberText.setText(AMEX_CARD_NUMBER);
        verifyCvcHintImage(/* expectedImage= */ R.drawable.cvc_icon_amex);
    }

    @Test
    @MediumTest
    public void testSecurityCode_whenNonAmExCardNumberIsEntered_usesDefaultCvcHintImage() {
        initFragment(getSampleAmexCardWithCvc());

        verifyCvcHintImage(/* expectedImage= */ R.drawable.cvc_icon_amex);
        mNumberText.setText(NON_AMEX_CARD_NUMBER);
        verifyCvcHintImage(/* expectedImage= */ R.drawable.cvc_icon);
    }

    @Test
    @MediumTest
    public void testSecurityCode_whenNumberIsChangedFromNonAmExToAmEx_usesAmExCvcHintImage() {
        initFragment(getSampleLocalCardWithCvc());

        mNumberText.setText(NON_AMEX_CARD_NUMBER);
        verifyCvcHintImage(/* expectedImage= */ R.drawable.cvc_icon);
        mNumberText.setText(AMEX_CARD_NUMBER);
        verifyCvcHintImage(/* expectedImage= */ R.drawable.cvc_icon_amex);
    }

    @Test
    @MediumTest
    public void testSecurityCode_whenNumberIsChangedFromAmExToNonAmEx_usesDefaultCvcHintImage() {
        initFragment(getSampleLocalCardWithCvc());

        mNumberText.setText(AMEX_CARD_NUMBER);
        verifyCvcHintImage(/* expectedImage= */ R.drawable.cvc_icon_amex);
        mNumberText.setText(NON_AMEX_CARD_NUMBER);
        verifyCvcHintImage(/* expectedImage= */ R.drawable.cvc_icon);
    }

    @Test
    @MediumTest
    public void expirationDateFieldSet_cardHasExpirationDate() {
        CreditCard card = getSampleLocalCardWithCvc();
        String validExpirationMonth = "12";
        String validExpirationYear = AutofillTestHelper.nextYear();
        card.setMonth(validExpirationMonth);
        card.setYear(validExpirationYear);
        initFragment(card);

        assertThat(mExpirationDate.getText().toString())
                .isEqualTo(
                        String.format(
                                "%s/%s", validExpirationMonth, validExpirationYear.substring(2)));
        assertTrue(mCardEditor.validateFormAndUpdateErrorAndFocusErrorField());
    }

    @Test
    @MediumTest
    public void testExpirationDate_whenInvalidDate_showsErrorMessage() {
        initFragment(getSampleLocalCardWithCvc());
        String invalidExpirationMonth = "14";
        String validExpirationYear = AutofillTestHelper.nextYear();

        mExpirationDate.setText(
                String.format("%s/%s", invalidExpirationMonth, validExpirationYear.substring(2)));

        assertThat(mExpirationDate.getError()).isEqualTo(mExpirationDateInvalidError);
        assertFalse(mCardEditor.validateFormAndUpdateErrorAndFocusErrorField());
    }

    @Test
    @MediumTest
    public void testExpirationDate_whenDateInPast_showsErrorMessage() {
        initFragment(getSampleLocalCardWithCvc());
        String validExpirationMonth = "12";
        String invalidPastExpirationYear = "2020";

        mExpirationDate.setText(
                String.format(
                        "%s/%s", validExpirationMonth, invalidPastExpirationYear.substring(2)));

        assertThat(mExpirationDate.getError()).isEqualTo(mExpiredCardError);
        assertFalse(mCardEditor.validateFormAndUpdateErrorAndFocusErrorField());
    }

    @Test
    @MediumTest
    public void testExpirationDate_whenDateIsCorrected_removesErrorMessage() {
        CreditCard card = getSampleLocalCardWithCvc();
        String validExpirationMonth = "12";
        String invalidPastExpirationYear = "2020";
        String validExpirationYear = AutofillTestHelper.nextYear();
        card.setMonth(validExpirationMonth);
        card.setYear(invalidPastExpirationYear);

        initFragment(card);

        assertThat(mExpirationDate.getError()).isEqualTo(mExpiredCardError);
        assertFalse(mCardEditor.validateFormAndUpdateErrorAndFocusErrorField());

        mExpirationDate.setText(
                String.format("%s/%s", validExpirationMonth, validExpirationYear.substring(2)));

        assertThat(mExpirationDate.getError()).isNull();
        assertTrue(mCardEditor.validateFormAndUpdateErrorAndFocusErrorField());
    }

    @Test
    @MediumTest
    public void testExpirationDate_whenDateIsEditedFromValidToIncomplete_disablesSaveButton() {
        initFragment(getSampleLocalCardWithCvc());
        String validExpirationMonth = "12";
        String validExpirationYear = AutofillTestHelper.nextYear();

        mExpirationDate.setText(
                String.format("%s/%s", validExpirationMonth, validExpirationYear.substring(2)));

        assertThat(mExpirationDate.getError()).isNull();
        assertTrue(mCardEditor.validateFormAndUpdateErrorAndFocusErrorField());

        mExpirationDate.setText(
                String.format("%s/%s", validExpirationMonth, /* expiration year */ ""));

        // Empty expiration date should make the form invalid.
        assertFalse(mCardEditor.validateFormAndUpdateErrorAndFocusErrorField());
        assertThat(mDoneButton.getError()).isNull();
    }

    @Test
    @MediumTest
    public void testExpirationDate_whenDateIsEditedFromValidToEmpty_disablesSaveButton() {
        initFragment(getSampleLocalCardWithCvc());
        String validExpirationMonth = "12";
        String validExpirationYear = AutofillTestHelper.nextYear();

        mExpirationDate.setText(
                String.format("%s/%s", validExpirationMonth, validExpirationYear.substring(2)));

        assertThat(mExpirationDate.getError()).isNull();
        assertTrue(mCardEditor.validateFormAndUpdateErrorAndFocusErrorField());

        mExpirationDate.setText(/* date= */ "");

        // Clearing the expiration data should make the form invalid.
        assertFalse(mCardEditor.validateFormAndUpdateErrorAndFocusErrorField());
        assertThat(mDoneButton.getError()).isNull();
    }

    @Test
    @MediumTest
    public void
            testExpirationDate_whenCorrectingOnlyNickname_keepsSaveButtonDisabledDueToInvalidDate() {
        initFragment(getSampleLocalCardWithCvc());
        String validNickname = "valid";
        String invalidNickname = "Invalid 123";
        String invalidPastExpirationYear = "2020";
        String validExpirationMonth = "12";
        String validExpirationYear = AutofillTestHelper.nextYear();

        mExpirationDate.setText(
                String.format("%s/%s", validExpirationMonth, validExpirationYear.substring(2)));
        mNicknameText.setText(validNickname);

        assertTrue(mCardEditor.validateFormAndUpdateErrorAndFocusErrorField());

        mExpirationDate.setText(
                String.format(
                        "%s/%s", validExpirationMonth, invalidPastExpirationYear.substring(2)));
        mNicknameText.setText(invalidNickname);

        // Invalid nickname and expiration year should make the form invalid.
        assertFalse(mCardEditor.validateFormAndUpdateErrorAndFocusErrorField());
        assertThat(mDoneButton.getError()).isNull();

        mNicknameText.setText(validNickname);

        // Invalid expiration year keeps the form invalid.
        assertFalse(mCardEditor.validateFormAndUpdateErrorAndFocusErrorField());
        assertThat(mDoneButton.getError()).isNull();
    }

    @Test
    @MediumTest
    public void deleteCreditCardConfirmationDialog_deleteEntryCanceled_dialogDismissed() {
        CreditCard card = getSampleLocalCard();
        initFragment(card);

        FakeModalDialogManager fakeModalDialogManager =
                new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
        openDeletePaymentMethodConfirmationDialog(fakeModalDialogManager);

        // Verify the dialog is open.
        assertNotNull(fakeModalDialogManager.getShownDialogModel());
        fakeModalDialogManager.clickNegativeButton();

        // Verify the dialog is closed.
        assertNull(fakeModalDialogManager.getShownDialogModel());

        // Verify the card entry is not deleted.
        verify(mMockPersonalDataManager, never()).deleteCreditCard(card.getGUID());
    }

    @Test
    @MediumTest
    public void
            deleteCreditCardConfirmationDialog_deleteEntryConfirmed_dialogDismissedAndEntryDeleted() {
        CreditCard card = getSampleLocalCard();
        initFragment(card);

        FakeModalDialogManager fakeModalDialogManager =
                new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
        openDeletePaymentMethodConfirmationDialog(fakeModalDialogManager);

        // Verify the dialog is open.
        assertNotNull(fakeModalDialogManager.getShownDialogModel());
        fakeModalDialogManager.clickPositiveButton();

        // Verify the dialog is closed.
        assertNull(fakeModalDialogManager.getShownDialogModel());

        // Verify the card entry is deleted.
        verify(mMockPersonalDataManager).deleteCreditCard(card.getGUID());
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testRecordHistogram_whenNewCreditCardIsAddedWithoutCvc() {
        initFragment(null);
        // Mock that there are already 4 cards saved.
        when(mMockPersonalDataManager.getCreditCardCountForSettings()).thenReturn(4);

        // Expect histogram to record 4 for 4 existing cards.
        HistogramWatcher saveCardCountHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AutofillLocalCardEditor.CARD_COUNT_BEFORE_ADDING_NEW_CARD_HISTOGRAM,
                                4)
                        .build();

        mNumberText.setText(NON_AMEX_CARD_NUMBER);
        mExpirationMonth.setSelection(/* monthSelection= */ 1);
        mExpirationYear.setSelection(/* yearSelection= */ 1);
        mDoneButton.performClick();

        saveCardCountHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testRecordHistogram_whenNewCreditCardIsAddedWithCvc() {
        initFragment(null);

        // Mock that there are already 4 cards saved.
        when(mMockPersonalDataManager.getCreditCardCountForSettings()).thenReturn(4);

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

        mNumberText.setText(NON_AMEX_CARD_NUMBER);
        mExpirationDate.setText(String.format("12/%s", AutofillTestHelper.nextYear().substring(2)));
        mCvc.setText(/* code= */ "321");
        mDoneButton.performClick();

        saveCardCountHistogram.assertExpected();
        saveCardWithoutExistingCardsHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testRecordHistogram_whenNewCreditCardIsAddedWithoutExistingCards() {
        initFragment(null);
        // Expect histogram to record true for adding a card without existing cards.
        HistogramWatcher saveCardWithoutExistingCardsHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                AutofillLocalCardEditor.CARD_ADDED_WITHOUT_EXISTING_CARDS_HISTOGRAM,
                                true)
                        .build();

        mNumberText.setText(NON_AMEX_CARD_NUMBER);
        mExpirationDate.setText(String.format("12/%s", AutofillTestHelper.nextYear().substring(2)));
        mCvc.setText(/* code= */ "321");
        mDoneButton.performClick();

        saveCardWithoutExistingCardsHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testRecordUserAction_whenNewCreditCardIsAddedWithCvc() {
        initFragment(null);
        String validExpirationYear = AutofillTestHelper.nextYear();
        String validExpirationMonth = "12";

        mNumberText.setText(NON_AMEX_CARD_NUMBER);
        mExpirationDate.setText(
                String.format("%s/%s", validExpirationMonth, validExpirationYear.substring(2)));
        mCvc.setText(/* code= */ "321");
        mDoneButton.performClick();

        assertTrue(mActionTester.getActions().contains("AutofillCreditCardsAddedWithCvc"));
    }

    @Test
    @MediumTest
    public void testRecordUserAction_whenExistingCreditCardWithoutCvcIsEditedAndCvcIsLeftBlank() {
        initFragment(getSampleLocalCard());
        String validExpirationYear = AutofillTestHelper.nextYear();
        String validExpirationMonth = "12";

        mExpirationDate.setText(
                String.format("%s/%s", validExpirationMonth, validExpirationYear.substring(2)));
        mDoneButton.performClick();

        assertTrue(
                mActionTester.getActions().contains("AutofillCreditCardsEditedAndCvcWasLeftBlank"));
    }

    @Test
    @MediumTest
    public void testRecordUserAction_whenExistingCreditCardWithoutCvcIsEditedAndCvcIsAdded() {
        initFragment(getSampleLocalCard());

        mCvc.setText(/* code= */ "321");
        mDoneButton.performClick();

        assertTrue(mActionTester.getActions().contains("AutofillCreditCardsEditedAndCvcWasAdded"));
    }

    @Test
    @MediumTest
    public void testRecordUserAction_whenExistingCreditCardWithCvcIsEditedAndCvcIsRemoved() {
        initFragment(getSampleLocalCardWithCvc());

        mCvc.setText(/* code= */ "");
        mDoneButton.performClick();

        assertTrue(
                mActionTester.getActions().contains("AutofillCreditCardsEditedAndCvcWasRemoved"));
    }

    @Test
    @MediumTest
    public void testRecordUserAction_whenExistingCreditCardWithCvcIsEditedAndCvcIsUpdated() {
        initFragment(getSampleLocalCardWithCvc());

        mCvc.setText(/* code= */ "321");
        mDoneButton.performClick();

        assertTrue(
                mActionTester.getActions().contains("AutofillCreditCardsEditedAndCvcWasUpdated"));
    }

    @Test
    @MediumTest
    public void testRecordUserAction_whenExistingCreditCardWithCvcIsEditedAndCvcIsUnchanged() {
        initFragment(getSampleLocalCardWithCvc());
        String validExpirationYear = AutofillTestHelper.nextYear();
        String validExpirationMonth = "12";

        mExpirationDate.setText(
                String.format("%s/%s", validExpirationMonth, validExpirationYear.substring(2)));
        mDoneButton.performClick();

        assertTrue(
                mActionTester.getActions().contains("AutofillCreditCardsEditedAndCvcWasUnchanged"));
    }

    @Test
    @MediumTest
    public void testRecordHistogram_whenAddCardFlowStarted() {
        // Expect histogram to record add card flow.
        HistogramWatcher addCardFlowHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(AutofillLocalCardEditor.ADD_CARD_FLOW_HISTOGRAM, true)
                        .build();
        initFragment(null);

        addCardFlowHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testRecordHistogram_whenAddCardFlowStartedWithoutExistingCards() {
        // Expect histogram to record true for entering the add card flow without existing cards.
        HistogramWatcher addCardFlowWithoutExistingCardsHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                AutofillLocalCardEditor
                                        .ADD_CARD_FLOW_WITHOUT_EXISTING_CARDS_HISTOGRAM,
                                true)
                        .build();
        initFragment(null);

        addCardFlowWithoutExistingCardsHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testRecordHistogram_whenAddCardFlowStartedWithExistingCards() {
        when(mMockPersonalDataManager.getCreditCardsForSettings())
                .thenReturn(List.of(getSampleLocalCard()));
        // Expect histogram to record false for entering the card added with existing cards.
        HistogramWatcher addCardFlowWithoutExistingCardsHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                AutofillLocalCardEditor
                                        .ADD_CARD_FLOW_WITHOUT_EXISTING_CARDS_HISTOGRAM,
                                false)
                        .build();
        initFragment(null);

        addCardFlowWithoutExistingCardsHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testRecordHistogram_notRecordedWhenCardEditFlowStarted() {
        // If the editor is opened for editing an existing card, the 'add card' histograms should
        // not be recorded.
        HistogramWatcher addCardFlowHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(AutofillLocalCardEditor.ADD_CARD_FLOW_HISTOGRAM)
                        .expectNoRecords(
                                AutofillLocalCardEditor
                                        .ADD_CARD_FLOW_WITHOUT_EXISTING_CARDS_HISTOGRAM)
                        .build();
        initFragment(getSampleLocalCard());

        addCardFlowHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void getExpirationMonth_whenDoubleDigitMonth_returnsMonth() {
        assertThat(AutofillLocalCardEditor.getExpirationMonth("12/23")).isEqualTo("12");
    }

    @Test
    @SmallTest
    public void getExpirationMonth_whenSingleDigitMonth_returnsMonthWithoutLeadingZero() {
        assertThat(AutofillLocalCardEditor.getExpirationMonth("02/23")).isEqualTo("2");
    }

    @Test
    @SmallTest
    public void getExpirationYear_returnsYearWithPrefix() {
        assertThat(AutofillLocalCardEditor.getExpirationYear("12/23")).isEqualTo("2023");
    }

    @Test
    @SmallTest
    public void testIsAmExCard_whenAmExCardNumberPrefixIsEntered_returnsTrue() {
        // Underlying JNI call is mocked for `isAmExCard` method.
        assertTrue(AutofillLocalCardEditor.isAmExCard(AMEX_CARD_NUMBER_PREFIX));
    }

    @Test
    @SmallTest
    public void testIsAmExCard_whenNonAmExCardNumberPrefixIsEntered_returnsFalse() {
        // Underlying JNI call is mocked for `isAmExCard` method.
        assertFalse(AutofillLocalCardEditor.isAmExCard(NON_AMEX_CARD_NUMBER_PREFIX));
    }

    @Test
    @MediumTest
    public void scanButtonIsVisible() {
        initFragment(null);
        assertEquals(View.VISIBLE, mScanButton.getVisibility());
    }

    @Test
    @MediumTest
    public void scannerCannotScan_scanButtonIsHidden() {
        when(mMockScanner.canScan()).thenReturn(false);
        initFragment(null);

        assertEquals(View.GONE, mScanButton.getVisibility());
    }

    @Test
    @MediumTest
    public void scannerButtonClicked_scanIsCalled() {
        initFragment(null);

        mScanButton.performClick();
        verify(mMockScanner).scan(mSettingsActivity.getIntentRequestTracker());
    }

    @Test
    @MediumTest
    public void onScanCompleted_cardDataIsAdded() {
        initFragment(null);
        CreditCard card = getSampleLocalCard();
        // Explicitly set the month to single digit to test padding.
        card.setMonth("5");

        assertTrue(mNameText.getText().toString().isEmpty());
        assertTrue(mNumberText.getText().toString().isEmpty());
        assertTrue(mExpirationDate.getText().toString().isEmpty());

        mCardEditor.onScanCompleted(
                card.getName(),
                card.getNumber(),
                Integer.parseInt(card.getMonth()),
                Integer.parseInt(card.getYear()));

        assertThat(mNameText.getText().toString()).isEqualTo(card.getName());
        assertThat(mNumberText.getText().toString().replaceAll(" ", ""))
                .isEqualTo(card.getNumber());
        assertThat(mExpirationDate.getText().toString())
                .isEqualTo(String.format("0%s/%s", card.getMonth(), card.getYear().substring(2)));
    }

    @Test
    @MediumTest
    public void paymentSettingsOnScanCompleted_twoDigitMonth() {
        initFragment(null);
        CreditCard card = getSampleLocalCard();
        // Ensure a two-digit month is formatted as-is.
        card.setMonth("10");

        assertTrue(mExpirationDate.getText().toString().isEmpty());

        mCardEditor.onScanCompleted(
                card.getName(),
                card.getNumber(),
                Integer.parseInt(card.getMonth()),
                Integer.parseInt(card.getYear()));

        assertThat(mExpirationDate.getText().toString())
                .isEqualTo(String.format("%s/%s", card.getMonth(), card.getYear().substring(2)));
    }

    @Test
    @MediumTest
    @DisableFeatures({
        ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE
    }) // Feature disabled to allow saving cards without expiration dates.
    public void onCardSave_scannerManagerLogScanResultIsCalled() {
        initFragment(null);
        mCardEditor.setCreditCardScannerManagerForTesting(mMockScannerManager);
        mNumberText.setText(NON_AMEX_CARD_NUMBER);

        mDoneButton.performClick();

        verify(mMockScannerManager).logScanResult();
    }

    @Test
    @MediumTest
    public void onFinishPage_scannerManagerFormClosedIsCalled() {
        initFragment(null);
        mCardEditor.setCreditCardScannerManagerForTesting(mMockScannerManager);

        mCardEditor.finishPage();

        verify(mMockScannerManager).formClosed();
    }

    @Test
    @MediumTest
    public void nameFieldEdited_scannerManagerFieldEditedIsCalledWithName() {
        initFragment(null);
        mCardEditor.setCreditCardScannerManagerForTesting(mMockScannerManager);
        mNameText.setText("Okarun");

        verify(mMockScannerManager).fieldEdited(FieldType.NAME);
    }

    @Test
    @MediumTest
    public void numberFieldEdited_scannerManagerFieldEditedIsCalledWithNumber() {
        initFragment(null);
        mCardEditor.setCreditCardScannerManagerForTesting(mMockScannerManager);
        mNumberText.setText(NON_AMEX_CARD_NUMBER);

        // Field edit may be called more than once because there are other listeners for the number
        // field that format the number as it's entered into the text field.
        verify(mMockScannerManager, atLeastOnce()).fieldEdited(FieldType.NUMBER);
    }

    @Test
    @MediumTest
    public void expirationDateFieldEdited_scannerManagerFieldEditedIsCalledWithMonthAndYear() {
        initFragment(null);
        mCardEditor.setCreditCardScannerManagerForTesting(mMockScannerManager);

        mExpirationDate.setText("10/95");

        verify(mMockScannerManager).fieldEdited(FieldType.MONTH);
        verify(mMockScannerManager).fieldEdited(FieldType.YEAR);
    }

    @Test
    @MediumTest
    public void cvcFieldEdited_scannerManagerFieldEditedIsCalledWithUnknown() {
        initFragment(null);
        mCardEditor.setCreditCardScannerManagerForTesting(mMockScannerManager);

        mCvc.setText("101");

        verify(mMockScannerManager).fieldEdited(FieldType.UNKNOWN);
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.ANDROID_SETTINGS_CONTAINMENT})
    public void saveCard_withBillingAddress_SettingsContainmentDisabled() {
        CreditCard card = getSampleLocalCard();
        List<AutofillProfile> profiles = setupBillingAddressProfiles();
        initFragment(card);

        mCardEditor.mBillingAddressSpinner.setSelection(
                2); // 0 is "Select", 1 is address1, 2 is address2
        mDoneButton.performClick();

        verify(mMockPersonalDataManager)
                .setCreditCard(
                        argThat(
                                c -> {
                                    assertThat(c.getBillingAddressId())
                                            .isEqualTo(profiles.get(1).getGUID());
                                    return true;
                                }));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_SETTINGS_CONTAINMENT})
    public void saveCard_withBillingAddress_SettingsContainmentEnabled() {
        CreditCard card = getSampleLocalCard();
        List<AutofillProfile> profiles = setupBillingAddressProfiles();
        initFragment(card);

        // Simulate that the user has selected the second billing address.
        // We set the text for visual confirmation and directly set the selected profile
        // to bypass the complexities of simulating a dropdown item click in Robolectric.
        mCardEditor.mBillingAddressDropdown.setText(profiles.get(1).getLabel(), false);
        mCardEditor.mSelectedBillingProfile = profiles.get(1);
        mDoneButton.performClick();

        verify(mMockPersonalDataManager)
                .setCreditCard(
                        argThat(
                                c -> {
                                    assertThat(c.getBillingAddressId())
                                            .isEqualTo(profiles.get(1).getGUID());
                                    return true;
                                }));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_SETTINGS_CONTAINMENT})
    public void saveCard_noBillingAddressSelected_SettingsContainmentEnabled() {
        CreditCard card = getSampleLocalCard();
        setupBillingAddressProfiles();
        initFragment(card);

        // Do not simulate a selection. The default (no selection) should result in an empty GUID.
        mDoneButton.performClick();

        verify(mMockPersonalDataManager)
                .setCreditCard(
                        argThat(
                                c -> {
                                    assertThat(c.getBillingAddressId()).isEmpty();
                                    return true;
                                }));
    }

    private List<AutofillProfile> setupBillingAddressProfiles() {
        AutofillProfile billingAddress1 =
                AutofillProfile.builder().setGUID("guid-1").setStreetAddress("1 Main St").build();
        AutofillProfile billingAddress2 =
                AutofillProfile.builder().setGUID("guid-2").setStreetAddress("2 Main St").build();
        List<AutofillProfile> profiles = List.of(billingAddress1, billingAddress2);
        when(mMockPersonalDataManager.getProfilesForSettings()).thenReturn(profiles);
        return profiles;
    }
}
