// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.autofill.PersonalDataManagerJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.VirtualCardEnrollmentState;

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
                /* number= */ "4444333322221111",
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
    }

    // This is an amex card with a CVC code.
    private static CreditCard getSampleAmexCardWithCvc() {
        return new CreditCard(
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
    }

    private static final String AMEX_CARD_NUMBER = "378282246310005";
    private static final String AMEX_CARD_NUMBER_PREFIX = "37";
    private static final String NON_AMEX_CARD_NUMBER = "4111111111111111";
    private static final String NON_AMEX_CARD_NUMBER_PREFIX = "41";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private PersonalDataManager mMockPersonalDataManager;
    @Mock private PersonalDataManager.Natives mPersonalDataManagerJni;

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

        PersonalDataManagerFactory.setInstanceForTesting(mMockPersonalDataManager);
    }

    @After
    public void tearDown() {
        if (mScenario != null) {
            mScenario.close();
        }
    }

    private void initFragment(CreditCard card) {
        String guid = card != null ? card.getGUID() : "";
        when(mMockPersonalDataManager.getCreditCard(guid)).thenReturn(card);
        mScenario =
                FragmentScenario.launchInContainer(
                        AutofillLocalCardEditor.class,
                        Bundle.EMPTY,
                        R.style.Theme_MaterialComponents,
                        new FragmentFactory() {
                            @NonNull
                            @Override
                            public Fragment instantiate(
                                    @NonNull ClassLoader classLoader, @NonNull String className) {
                                Fragment fragment = super.instantiate(classLoader, className);
                                if (fragment instanceof AutofillLocalCardEditor) {
                                    ((AutofillLocalCardEditor) fragment).setProfile(mProfile);
                                    Bundle args = new Bundle();
                                    args.putString("guid", guid);
                                    fragment.setArguments(args);
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

        assertThat(expectedDrawable.getBitmap().sameAs(actualDrawable.getBitmap())).isTrue();
    }

    @Test
    @MediumTest
    public void nicknameFieldEmpty_cardDoesNotHaveNickname() {
        initFragment(getSampleLocalCard());

        assertThat(mNicknameText.getText().toString()).isEmpty();
        assertThat(mDoneButton.isEnabled()).isFalse();
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
        assertThat(mDoneButton.isEnabled()).isFalse();
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
        assertThat(mDoneButton.isEnabled()).isFalse();
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
        assertThat(mDoneButton.isEnabled()).isTrue();
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
        assertThat(mDoneButton.isEnabled()).isTrue();
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

        assertThat(mExpirationMonth.isShown()).isTrue();
        assertThat(mExpirationYear.isShown()).isTrue();

        // The expiration date and the cvc fields should not be shown to the user.
        assertThat(mExpirationDate.isShown()).isFalse();
        assertThat(mCvc.isShown()).isFalse();
    }

    @Test
    @MediumTest
    public void testExpirationDateAndSecurityCodeFieldsAreShown() {
        initFragment(getSampleLocalCardWithCvc());

        // When the flag is on, expiration date and cvc fields should be visible.
        assertThat(mExpirationDate.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(mCvc.getVisibility()).isEqualTo(View.VISIBLE);

        assertThat(mExpirationDate.isShown()).isTrue();
        assertThat(mCvc.isShown()).isTrue();

        // When the flag is on, month and year fields shouldn't be visible.
        assertThat(mExpirationMonth.isShown()).isFalse();
        assertThat(mExpirationYear.isShown()).isFalse();
    }

    @Test
    @MediumTest
    public void securityCodeFieldSet_cardHasCvc() {
        CreditCard card = getSampleLocalCardWithCvc();
        String cvc = "234";
        card.setCvc(cvc);
        initFragment(card);

        assertThat(mCvc.getText().toString()).isEqualTo(cvc);
        assertThat(mDoneButton.isEnabled()).isFalse();
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
}
