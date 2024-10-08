// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.drawable.BitmapDrawable;
import android.os.Bundle;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.autofill.AutofillEditorBase;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/** Instrumentation tests for AutofillLocalCardEditor. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillLocalCardEditorTest {
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
                    /* obfuscatedNumber= */ "",
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
                    /* obfuscatedNumber= */ "",
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
                    /* obfuscatedNumber= */ "",
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
                    /* obfuscatedNumber= */ "",
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

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mAutofillTestHelper = new AutofillTestHelper();
        mActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        mActionTester.tearDown();
    }

    @Test
    @MediumTest
    public void nicknameFieldEmpty_cardDoesNotHaveNickname() throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mNicknameText.getText().toString()).isEmpty();
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    public void nicknameFieldSet_cardHasNickname() throws Exception {
        String nickname = "test nickname";
        SAMPLE_LOCAL_CARD.setNickname(nickname);
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mNicknameText.getText().toString())
                .isEqualTo(nickname);
        // If the nickname is not modified the Done button should be disabled.
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    public void testNicknameFieldIsShown() throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getVisibility())
                .isEqualTo(View.VISIBLE);
    }

    @Test
    @MediumTest
    public void testInvalidNicknameShowsErrorMessage() throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mNicknameText.setText("Nickname 123");
                    } catch (Exception e) {
                        throw new AssertionError("Failed to set the nickname", e);
                    }
                });

        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getError())
                .isEqualTo(
                        activity.getResources()
                                .getString(R.string.autofill_credit_card_editor_invalid_nickname));
        // Since the nickname has an error, the done button should be disabled.
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    public void testErrorMessageHiddenAfterNicknameIsEditedFromInvalidToValid() throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mNicknameText.setText("Nickname 123");
                    } catch (Exception e) {
                        throw new AssertionError("Failed to set the nickname", e);
                    }
                });
        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getError())
                .isEqualTo(
                        activity.getResources()
                                .getString(R.string.autofill_credit_card_editor_invalid_nickname));

        // Set the nickname to valid one.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mNicknameText.setText("Valid Nickname");
                    } catch (Exception e) {
                        throw new AssertionError("Failed to set the nickname", e);
                    }
                });

        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getError()).isNull();
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isTrue();
    }

    @Test
    @MediumTest
    public void testErrorMessageHiddenAfterNicknameIsEditedFromInvalidToEmpty() throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mNicknameText.setText("Nickname 123");
                    } catch (Exception e) {
                        throw new AssertionError("Failed to set the nickname", e);
                    }
                });
        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getError())
                .isEqualTo(
                        activity.getResources()
                                .getString(R.string.autofill_credit_card_editor_invalid_nickname));

        // Clear the nickname.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mNicknameText.setText(null);
                    } catch (Exception e) {
                        throw new AssertionError("Failed to set the nickname", e);
                    }
                });

        assertThat(autofillLocalCardEditorFragment.mNicknameLabel.getError()).isNull();
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isTrue();
    }

    @Test
    @MediumTest
    public void testNicknameLengthCappedAt25Characters() throws Exception {
        String veryLongNickname = "This is a very very long nickname";
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mNicknameText.setText(veryLongNickname);
                    } catch (Exception e) {
                        throw new AssertionError("Failed to set the nickname", e);
                    }
                });

        assertThat(autofillLocalCardEditorFragment.mNicknameText.getText().toString())
                .isEqualTo(veryLongNickname.substring(0, 25));
    }

    private Bundle fragmentArgs(String guid) {
        Bundle args = new Bundle();
        args.putString(AutofillEditorBase.AUTOFILL_GUID, guid);
        return args;
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testExpirationDateSpinnerAreShownWhenCvcFlagOff() throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mExpirationMonth.getVisibility())
                .isEqualTo(View.VISIBLE);
        assertThat(autofillLocalCardEditorFragment.mExpirationYear.getVisibility())
                .isEqualTo(View.VISIBLE);
        assertThat(autofillLocalCardEditorFragment.mExpirationDate).isNull();
        assertThat(autofillLocalCardEditorFragment.mCvc).isNull();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testExpirationDateAndSecurityCodeFieldsAreShown() throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mExpirationDate.getVisibility())
                .isEqualTo(View.VISIBLE);
        assertThat(autofillLocalCardEditorFragment.mCvc.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(autofillLocalCardEditorFragment.mExpirationMonth).isNull();
        assertThat(autofillLocalCardEditorFragment.mExpirationYear).isNull();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void securityCodeFieldSet_cardHasCvc() throws Exception {
        String cvc = "234";
        SAMPLE_LOCAL_CARD_WITH_CVC.setCvc(cvc);
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mCvc.getText().toString()).isEqualTo(cvc);
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testSecurityCode_whenAmExCardIsSet_usesAmExCvcHintImage() throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_AMEX_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();

        verifyCvcHintImage(
                autofillLocalCardEditorFragment, /* expectedImage= */ R.drawable.cvc_icon_amex);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testSecurityCode_whenCardIsNotSet_usesDefaultCvcHintImage() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();

        verifyCvcHintImage(
                autofillLocalCardEditorFragment, /* expectedImage= */ R.drawable.cvc_icon);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testSecurityCode_whenAmExCardNumberIsEntered_usesAmExCvcHintImage()
            throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setCardNumberOnEditor(autofillLocalCardEditorFragment, AMEX_CARD_NUMBER);

        verifyCvcHintImage(
                autofillLocalCardEditorFragment, /* expectedImage= */ R.drawable.cvc_icon_amex);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testSecurityCode_whenNonAmExCardNumberIsEntered_usesDefaultCvcHintImage()
            throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setCardNumberOnEditor(autofillLocalCardEditorFragment, NON_AMEX_CARD_NUMBER);

        verifyCvcHintImage(
                autofillLocalCardEditorFragment, /* expectedImage= */ R.drawable.cvc_icon);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testSecurityCode_whenNumberIsChangedFromAmExToNonAmEx_usesDefaultCvcHintImage()
            throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_AMEX_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setCardNumberOnEditor(autofillLocalCardEditorFragment, NON_AMEX_CARD_NUMBER);

        verifyCvcHintImage(
                autofillLocalCardEditorFragment, /* expectedImage= */ R.drawable.cvc_icon);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testSecurityCode_whenNumberIsChangedFromNonAmExToAmEx_usesAmExCvcHintImage()
            throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setCardNumberOnEditor(autofillLocalCardEditorFragment, AMEX_CARD_NUMBER);

        verifyCvcHintImage(
                autofillLocalCardEditorFragment, /* expectedImage= */ R.drawable.cvc_icon_amex);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void expirationDateFieldSet_cardHasExpirationDate() throws Exception {
        String validExpirationMonth = "12";
        String validExpirationYear = AutofillTestHelper.nextYear();
        SAMPLE_LOCAL_CARD_WITH_CVC.setMonth(validExpirationMonth);
        SAMPLE_LOCAL_CARD_WITH_CVC.setYear(validExpirationYear);
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mExpirationDate.getText().toString())
                .isEqualTo(
                        String.format(
                                "%s/%s", validExpirationMonth, validExpirationYear.substring(2)));
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testExpirationDate_whenInvalidDate_showsErrorMessage() throws Exception {
        String invalidExpirationMonth = "14";
        String validExpirationYear = AutofillTestHelper.nextYear();
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setExpirationDateOnEditor(
                autofillLocalCardEditorFragment,
                String.format("%s/%s", invalidExpirationMonth, validExpirationYear.substring(2)));

        assertThat(autofillLocalCardEditorFragment.mExpirationDate.getError())
                .isEqualTo(
                        activity.getResources()
                                .getString(
                                        R.string
                                                .autofill_credit_card_editor_invalid_expiration_date));
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testExpirationDate_whenDateInPast_showsErrorMessage() throws Exception {
        String validExpirationMonth = "12";
        String invalidPastExpirationYear = "2020";
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setExpirationDateOnEditor(
                autofillLocalCardEditorFragment,
                String.format(
                        "%s/%s", validExpirationMonth, invalidPastExpirationYear.substring(2)));

        assertThat(autofillLocalCardEditorFragment.mExpirationDate.getError())
                .isEqualTo(
                        activity.getResources()
                                .getString(R.string.autofill_credit_card_editor_expired_card));
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testExpirationDate_whenDateIsCorrected_removesErrorMessage() throws Exception {
        String validExpirationMonth = "12";
        String invalidPastExpirationYear = "2020";
        String validExpirationYear = AutofillTestHelper.nextYear();
        SAMPLE_LOCAL_CARD_WITH_CVC.setMonth(validExpirationMonth);
        SAMPLE_LOCAL_CARD_WITH_CVC.setYear(invalidPastExpirationYear);
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));

        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        assertThat(autofillLocalCardEditorFragment.mExpirationDate.getError())
                .isEqualTo(
                        activity.getResources()
                                .getString(R.string.autofill_credit_card_editor_expired_card));
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();

        setExpirationDateOnEditor(
                autofillLocalCardEditorFragment,
                String.format("%s/%s", validExpirationMonth, validExpirationYear.substring(2)));

        assertThat(autofillLocalCardEditorFragment.mExpirationDate.getError()).isNull();
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isTrue();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testExpirationDate_whenDateIsEditedFromValidToIncomplete_disablesSaveButton()
            throws Exception {
        String validExpirationMonth = "12";
        String validExpirationYear = AutofillTestHelper.nextYear();
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setExpirationDateOnEditor(
                autofillLocalCardEditorFragment,
                String.format("%s/%s", validExpirationMonth, validExpirationYear.substring(2)));

        assertThat(autofillLocalCardEditorFragment.mExpirationDate.getError()).isNull();
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isTrue();

        setExpirationDateOnEditor(
                autofillLocalCardEditorFragment,
                String.format("%s/%s", validExpirationMonth, /* expiration year */ ""));

        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testExpirationDate_whenDateIsEditedFromValidToEmpty_disablesSaveButton()
            throws Exception {
        String validExpirationMonth = "12";
        String validExpirationYear = AutofillTestHelper.nextYear();
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setExpirationDateOnEditor(
                autofillLocalCardEditorFragment,
                String.format("%s/%s", validExpirationMonth, validExpirationYear.substring(2)));

        assertThat(autofillLocalCardEditorFragment.mExpirationDate.getError()).isNull();
        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isTrue();

        setExpirationDateOnEditor(autofillLocalCardEditorFragment, /* date= */ "");

        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void
            testExpirationDate_whenCorrectingOnlyNickname_keepsSaveButtonDisabledDueToInvalidDate()
                    throws Exception {
        String validNickname = "valid";
        String invalidNickname = "Invalid 123";
        String invalidPastExpirationYear = "2020";
        String validExpirationMonth = "12";
        String validExpirationYear = AutofillTestHelper.nextYear();
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setExpirationDateOnEditor(
                autofillLocalCardEditorFragment,
                String.format("%s/%s", validExpirationMonth, validExpirationYear.substring(2)));
        setNicknameOnEditor(autofillLocalCardEditorFragment, validNickname);

        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isTrue();

        setExpirationDateOnEditor(
                autofillLocalCardEditorFragment,
                String.format(
                        "%s/%s", validExpirationMonth, invalidPastExpirationYear.substring(2)));
        setNicknameOnEditor(autofillLocalCardEditorFragment, invalidNickname);

        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();

        setNicknameOnEditor(autofillLocalCardEditorFragment, validNickname);

        assertThat(autofillLocalCardEditorFragment.mDoneButton.isEnabled()).isFalse();
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
        Assert.assertNotNull(fakeModalDialogManager.getShownDialogModel());
        ThreadUtils.runOnUiThreadBlocking(() -> fakeModalDialogManager.clickNegativeButton());

        // Verify the dialog is closed
        Assert.assertNull(fakeModalDialogManager.getShownDialogModel());

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
        Assert.assertNotNull(fakeModalDialogManager.getShownDialogModel());
        ThreadUtils.runOnUiThreadBlocking(() -> fakeModalDialogManager.clickPositiveButton());

        // Verify the dialog is closed
        Assert.assertNull(fakeModalDialogManager.getShownDialogModel());

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
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testRecordUserAction_whenNewCreditCardIsAddedWithCvc() throws Exception {
        String validExpirationYear = AutofillTestHelper.nextYear();
        String validExpirationMonth = "12";

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setCardNumberOnEditor(autofillLocalCardEditorFragment, NON_AMEX_CARD_NUMBER);
        setExpirationDateOnEditor(
                autofillLocalCardEditorFragment,
                String.format("%s/%s", validExpirationMonth, validExpirationYear.substring(2)));
        setSecurityCodeOnEditor(autofillLocalCardEditorFragment, /* code= */ "321");
        performButtonClickOnEditor(autofillLocalCardEditorFragment.mDoneButton);

        Assert.assertTrue(
                "User action should be logged.",
                mActionTester.getActions().contains("AutofillCreditCardsAddedWithCvc"));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testRecordUserAction_whenExistingCreditCardWithoutCvcIsEditedAndCvcIsLeftBlank()
            throws Exception {
        String validExpirationYear = AutofillTestHelper.nextYear();
        String validExpirationMonth = "12";
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setExpirationDateOnEditor(
                autofillLocalCardEditorFragment,
                String.format("%s/%s", validExpirationMonth, validExpirationYear.substring(2)));
        performButtonClickOnEditor(autofillLocalCardEditorFragment.mDoneButton);

        Assert.assertTrue(
                "User action should be logged.",
                mActionTester.getActions().contains("AutofillCreditCardsEditedAndCvcWasLeftBlank"));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testRecordUserAction_whenExistingCreditCardWithoutCvcIsEditedAndCvcIsAdded()
            throws Exception {
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setSecurityCodeOnEditor(autofillLocalCardEditorFragment, /* code= */ "321");
        performButtonClickOnEditor(autofillLocalCardEditorFragment.mDoneButton);

        Assert.assertTrue(
                "User action should be logged.",
                mActionTester.getActions().contains("AutofillCreditCardsEditedAndCvcWasAdded"));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testRecordUserAction_whenExistingCreditCardWithCvcIsEditedAndCvcIsRemoved()
            throws Exception {
        SAMPLE_LOCAL_CARD_WITH_CVC.setNumber(NON_AMEX_CARD_NUMBER);
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setSecurityCodeOnEditor(autofillLocalCardEditorFragment, /* code= */ "");
        performButtonClickOnEditor(autofillLocalCardEditorFragment.mDoneButton);

        Assert.assertTrue(
                "User action should be logged.",
                mActionTester.getActions().contains("AutofillCreditCardsEditedAndCvcWasRemoved"));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testRecordUserAction_whenExistingCreditCardWithCvcIsEditedAndCvcIsUpdated()
            throws Exception {
        SAMPLE_LOCAL_CARD_WITH_CVC.setNumber(NON_AMEX_CARD_NUMBER);
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setSecurityCodeOnEditor(autofillLocalCardEditorFragment, /* code= */ "321");
        performButtonClickOnEditor(autofillLocalCardEditorFragment.mDoneButton);

        Assert.assertTrue(
                "User action should be logged.",
                mActionTester.getActions().contains("AutofillCreditCardsEditedAndCvcWasUpdated"));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testRecordUserAction_whenExistingCreditCardWithCvcIsEditedAndCvcIsUnchanged()
            throws Exception {
        String validExpirationYear = AutofillTestHelper.nextYear();
        String validExpirationMonth = "12";
        SAMPLE_LOCAL_CARD_WITH_CVC.setNumber(NON_AMEX_CARD_NUMBER);
        String guid = mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD_WITH_CVC);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(fragmentArgs(guid));
        AutofillLocalCardEditor autofillLocalCardEditorFragment =
                (AutofillLocalCardEditor) activity.getMainFragment();
        setExpirationDateOnEditor(
                autofillLocalCardEditorFragment,
                String.format("%s/%s", validExpirationMonth, validExpirationYear.substring(2)));
        performButtonClickOnEditor(autofillLocalCardEditorFragment.mDoneButton);

        Assert.assertTrue(
                "User action should be logged.",
                mActionTester.getActions().contains("AutofillCreditCardsEditedAndCvcWasUnchanged"));
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

    private void setNicknameOnEditor(
            AutofillLocalCardEditor autofillLocalCardEditorFragment, String nickname) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalCardEditorFragment.mNicknameText.setText(nickname);
                    } catch (Exception e) {
                        throw new AssertionError("Failed to set the nickname", e);
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

    private void verifyCvcHintImage(
            AutofillLocalCardEditor autofillLocalCardEditorFragment, int expectedImage) {
        ImageView expectedCvcHintImage = new ImageView(ContextUtils.getApplicationContext());
        expectedCvcHintImage.setImageResource(expectedImage);

        BitmapDrawable expectedDrawable = (BitmapDrawable) expectedCvcHintImage.getDrawable();
        BitmapDrawable actualDrawable =
                (BitmapDrawable) autofillLocalCardEditorFragment.mCvcHintImage.getDrawable();
        assertThat(expectedDrawable.getBitmap().sameAs(actualDrawable.getBitmap())).isTrue();
    }
}
