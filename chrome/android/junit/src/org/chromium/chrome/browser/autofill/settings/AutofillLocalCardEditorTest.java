// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.when;

import android.graphics.drawable.BitmapDrawable;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.Spinner;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentFactory;
import androidx.fragment.app.testing.FragmentScenario;
import androidx.test.filters.MediumTest;

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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.autofill.PersonalDataManagerJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.components.browser_ui.settings.SettingsNavigation;

import java.util.List;

/** Unit tests for {@link AutofillLocalCardEditorTest}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE)
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
                /* month= */ "5",
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
    }

    private static final String AMEX_CARD_NUMBER = "378282246310005";
    private static final String AMEX_CARD_NUMBER_PREFIX = "37";
    private static final String NON_AMEX_CARD_NUMBER = "4111111111111111";
    private static final String NON_AMEX_CARD_NUMBER_PREFIX = "41";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private PersonalDataManager mMockPersonalDataManager;
    @Mock private PersonalDataManager.Natives mPersonalDataManagerJni;
    @Mock private SettingsNavigation mSettingsNavigation;

    private UserActionTester mActionTester;

    private FragmentScenario mScenario;

    private Button mDoneButton;
    private EditText mNicknameText;
    private TextInputLayout mNicknameLabel;

    private Spinner mExpirationMonth;
    private Spinner mExpirationYear;
    private EditText mExpirationDate;
    private EditText mCvc;

    private EditText mNumberText;

    private ImageView mCvcHintImage;

    private String mNicknameInvalidError;
    private String mExpirationDateInvalidError;
    private String mExpiredCardError;

    @Before
    public void setUp() {
        PersonalDataManagerJni.setInstanceForTesting(mPersonalDataManagerJni);
        // Mock a card recognition logic
        when(mPersonalDataManagerJni.getBasicCardIssuerNetwork(anyString(), anyBoolean()))
                .thenAnswer(
                        new Answer<String>() {
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

        PersonalDataManagerFactory.setInstanceForTesting(mMockPersonalDataManager);
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
        mActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        if (mScenario != null) {
            mScenario.close();
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

        mScenario =
                FragmentScenario.launchInContainer(
                        AutofillLocalCardEditor.class,
                        arguments,
                        R.style.Theme_MaterialComponents,
                        new FragmentFactory() {
                            @NonNull
                            @Override
                            public Fragment instantiate(
                                    @NonNull ClassLoader classLoader, @NonNull String className) {
                                Fragment fragment = super.instantiate(classLoader, className);
                                if (fragment instanceof AutofillLocalCardEditor) {
                                    ((AutofillLocalCardEditor) fragment).setProfile(mProfile);
                                }
                                return fragment;
                            }
                        });
        mScenario.onFragment(
                fragment -> {
                    mDoneButton = fragment.getView().findViewById(R.id.button_primary);
                    mNicknameText = fragment.getView().findViewById(R.id.credit_card_nickname_edit);
                    mNicknameLabel =
                            fragment.getView().findViewById(R.id.credit_card_nickname_label);
                    mNicknameInvalidError =
                            fragment.getContext()
                                    .getString(
                                            R.string.autofill_credit_card_editor_invalid_nickname);
                    mExpirationMonth =
                            fragment.getView()
                                    .findViewById(R.id.autofill_credit_card_editor_month_spinner);
                    mExpirationYear =
                            fragment.getView()
                                    .findViewById(R.id.autofill_credit_card_editor_year_spinner);
                    mExpirationDate =
                            fragment.getView().findViewById(R.id.expiration_month_and_year);
                    mCvc = fragment.getView().findViewById(R.id.cvc);
                    mCvcHintImage = fragment.getView().findViewById(R.id.cvc_hint_image);
                    mNumberText = fragment.getView().findViewById(R.id.credit_card_number_edit);
                    mExpirationDateInvalidError =
                            fragment.getContext()
                                    .getString(
                                            R.string
                                                    .autofill_credit_card_editor_invalid_expiration_date);
                    mExpiredCardError =
                            fragment.getContext()
                                    .getString(R.string.autofill_credit_card_editor_expired_card);
                });
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
        assertFalse(mDoneButton.isEnabled());
    }

    @Test
    @MediumTest
    public void nicknameFieldSet_cardHasNickname() {
        CreditCard card = getSampleLocalCard();
        String nickname = "test nickname";
        card.setNickname(nickname);
        initFragment(card);

        assertThat(mNicknameText.getText().toString()).isEqualTo(nickname);
        // If the nickname is not modified `mDoneButton` button should be disabled.
        assertFalse(mDoneButton.isEnabled());
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
        // Since the nickname has an error, the done button should be disabled.
        assertFalse(mDoneButton.isEnabled());
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
        assertTrue(mDoneButton.isEnabled());
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
        assertTrue(mDoneButton.isEnabled());
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
        assertFalse(mDoneButton.isEnabled());
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
        assertFalse(mDoneButton.isEnabled());
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
        assertFalse(mDoneButton.isEnabled());
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
        assertFalse(mDoneButton.isEnabled());
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
        assertFalse(mDoneButton.isEnabled());

        mExpirationDate.setText(
                String.format("%s/%s", validExpirationMonth, validExpirationYear.substring(2)));

        assertThat(mExpirationDate.getError()).isNull();
        assertTrue(mDoneButton.isEnabled());
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
        assertTrue(mDoneButton.isEnabled());

        mExpirationDate.setText(
                String.format("%s/%s", validExpirationMonth, /* expiration year */ ""));

        // Button should be disabled, but no error should be visible too.
        assertFalse(mDoneButton.isEnabled());
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
        assertTrue(mDoneButton.isEnabled());

        mExpirationDate.setText(/* date= */ "");

        // Button should be disabled, but no error should be visible too.
        assertFalse(mDoneButton.isEnabled());
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

        assertTrue(mDoneButton.isEnabled());

        mExpirationDate.setText(
                String.format(
                        "%s/%s", validExpirationMonth, invalidPastExpirationYear.substring(2)));
        mNicknameText.setText(invalidNickname);

        // Button should be disabled, but no error should be visible too.
        assertFalse(mDoneButton.isEnabled());
        assertThat(mDoneButton.getError()).isNull();

        mNicknameText.setText(validNickname);

        // Button should be disabled, but no error should be visible too.
        assertFalse(mDoneButton.isEnabled());
        assertThat(mDoneButton.getError()).isNull();
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
        initFragment(getSampleLocalCard());

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
        initFragment(getSampleLocalCard());

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
        initFragment(getSampleLocalCard());

        addCardFlowWithoutExistingCardsHistogram.assertExpected();
    }
}
