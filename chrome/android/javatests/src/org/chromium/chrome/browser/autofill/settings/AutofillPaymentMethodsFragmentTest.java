// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.device_reauth.BiometricStatus;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.IbanRecordType;
import org.chromium.components.autofill.MandatoryReauthAuthenticationFlowEvent;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.autofill.payments.PaymentInstrument;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Instrumentation tests for AutofillPaymentMethodsFragment. */
@RunWith(ChromeJUnit4ClassRunner.class)
// TODO(crbug.com/344661357): Failing when batched, batch this again.
public class AutofillPaymentMethodsFragmentTest {
    @Rule public final AutofillTestRule rule = new AutofillTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Rule
    public final SettingsActivityTestRule<AutofillPaymentMethodsFragment>
            mSettingsActivityTestRule =
                    new SettingsActivityTestRule<>(AutofillPaymentMethodsFragment.class);

    @Rule public JniMocker mMocker = new JniMocker();

    @Mock private ReauthenticatorBridge mReauthenticatorMock;
    @Mock private AutofillPaymentMethodsDelegate.Natives mNativeMock;
    @Mock private Callback<String> mServerIbanManageLinkOpenerCallback;

    // Card Issuer values that map to the browser CreditCard.Issuer enum.
    private static final int CARD_ISSUER_UNKNOWN = 0;
    private static final long NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE = 100L;
    private static final List<String> CARD_ISSUER_NETWORKS = Arrays.asList("visa", "mastercard");

    private static final CreditCard SAMPLE_CARD_VISA =
            new CreditCard(
                    /* guid= */ "",
                    /* origin= */ "",
                    /* isLocal= */ false,
                    /* name= */ "John Doe",
                    /* number= */ "4444333322221111",
                    /* obfuscatedNumber= */ "",
                    /* month= */ "5",
                    AutofillTestHelper.nextYear(),
                    /* basicCardIssuerNetwork= */ "visa",
                    /* issuerIconDrawableId= */ 0,
                    /* billingAddressId= */ "",
                    /* serverId= */ "");
    private static final CreditCard SAMPLE_CARD_MASTERCARD =
            new CreditCard(
                    /* guid= */ "",
                    /* origin= */ "",
                    /* isLocal= */ false,
                    /* name= */ "John Doe",
                    /* number= */ "5454545454545454",
                    /* obfuscatedNumber= */ "",
                    /* month= */ "12",
                    AutofillTestHelper.nextYear(),
                    /* basicCardIssuerNetwork= */ "mastercard",
                    /* issuerIconDrawableId= */ 0,
                    /* billingAddressId= */ "",
                    /* serverId= */ "");
    private static final CreditCard SAMPLE_LOCAL_CARD =
            new CreditCard(
                    /* guid= */ "",
                    /* origin= */ "",
                    /* isLocal= */ true,
                    /* name= */ "John Doe",
                    /* number= */ "4111111111111111",
                    /* obfuscatedNumber= */ "",
                    /* month= */ "5",
                    AutofillTestHelper.nextYear(),
                    /* basicCardIssuerNetwork= */ "visa",
                    /* issuerIconDrawableId= */ 0,
                    /* billingAddressId= */ "",
                    /* serverId= */ "");
    private static final CreditCard SAMPLE_VIRTUAL_CARD_UNENROLLED =
            new CreditCard(
                    /* guid= */ "",
                    /* origin= */ "",
                    /* isLocal= */ false,
                    /* isVirtual= */ false,
                    /* name= */ "John Doe",
                    /* number= */ "4444333322221111",
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
                    /* cvc= */ "",
                    /* issuerId= */ "",
                    /* productTermsUrl= */ null);
    private static final CreditCard SAMPLE_VIRTUAL_CARD_ENROLLED =
            new CreditCard(
                    /* guid= */ "",
                    /* origin= */ "",
                    /* isLocal= */ false,
                    /* isVirtual= */ false,
                    /* name= */ "John Doe",
                    /* number= */ "4444333322221111",
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
                    /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState.ENROLLED,
                    /* productDescription= */ "",
                    /* cardNameForAutofillDisplay= */ "",
                    /* obfuscatedLastFourDigits= */ "",
                    /* cvc= */ "",
                    /* issuerId= */ "",
                    /* productTermsUrl= */ null);
    private static final CreditCard SAMPLE_CARD_WITH_CVC =
            new CreditCard(
                    /* guid= */ "",
                    /* origin= */ "",
                    /* isLocal= */ false,
                    /* isVirtual= */ false,
                    /* name= */ "John Doe",
                    /* number= */ "4444333322221111",
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
    private static final BankAccount PIX_BANK_ACCOUNT =
            new BankAccount.Builder()
                    .setPaymentInstrument(
                            new PaymentInstrument.Builder()
                                    .setInstrumentId(100L)
                                    .setNickname("nickname")
                                    .setSupportedPaymentRails(new int[] {1})
                                    .build())
                    .setBankName("bank_name")
                    .setAccountNumberSuffix("account_number_suffix")
                    .build();

    private static final Iban VALID_BELGIUM_LOCAL_IBAN =
            new Iban.Builder()
                    .setLabel("")
                    .setNickname("My IBAN")
                    .setRecordType(IbanRecordType.UNKNOWN)
                    .setValue("BE71096123456769")
                    .build();
    private static final Iban VALID_RUSSIA_LOCAL_IBAN =
            new Iban.Builder()
                    .setLabel("")
                    .setNickname("")
                    .setRecordType(IbanRecordType.UNKNOWN)
                    .setValue("RU0204452560040702810412345678901")
                    .build();
    private static final Iban VALID_SERVER_IBAN =
            Iban.createServer(
                    /* instrumentId= */ 100L,
                    /* label= */ "FR •••0189",
                    /* nickname= */ "My IBAN",
                    /* value= */ "");

    private AutofillTestHelper mAutofillTestHelper;

    @Before
    public void setUp() {
        mAutofillTestHelper = new AutofillTestHelper();
        ReauthenticatorBridge.setInstanceForTesting(mReauthenticatorMock);
    }

    @After
    public void tearDown() throws TimeoutException {
        mAutofillTestHelper.clearAllDataForTesting();
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    @DisableFeatures({
        ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE,
        ChromeFeatureList.AUTOFILL_ENABLE_LOCAL_IBAN
    })
    public void testTwoCreditCards_displaysTwoServerCards() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_VISA);
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_MASTERCARD);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preferences on the initial screen map to Save and Fill toggle + Mandatory
        // Reauth toggle + 2 Cards + Add Card button + Payment Apps.
        Assert.assertEquals(6, getPreferenceScreen(activity).getPreferenceCount());
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_AUTO)
    @DisableFeatures({
        ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE,
        ChromeFeatureList.AUTOFILL_ENABLE_LOCAL_IBAN
    })
    public void testTwoCreditCards_displaysTwoServerCards_mandatoryReauthNotShownOnAutomotive()
            throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_VISA);
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_MASTERCARD);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preferences on the initial screen map to Save and Fill toggle + 2 Cards
        // + Add Card button + Payment Apps.
        Assert.assertEquals(5, getPreferenceScreen(activity).getPreferenceCount());
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_AUTO)
    @DisableFeatures(ChromeFeatureList.AUTOFILL_ENABLE_LOCAL_IBAN)
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void
            testTwoCreditCards_displaysTwoServerCards_mandatoryReauthNotShownOnAutomotive_ButCvcStorageEnabled()
                    throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_VISA);
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_MASTERCARD);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preferences on the initial screen map to Save and Fill toggle + CVC
        // storage toggle + 2 Cards + Add Card button + Payment Apps.
        Assert.assertEquals(6, getPreferenceScreen(activity).getPreferenceCount());
    }

    @Test
    @MediumTest
    public void testCreditCardWithoutNickname_displayNetworkAndLastFourAsTitle() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_VISA);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getFirstPaymentMethodPreference(activity);
        String title = cardPreference.getTitle().toString();
        assertThat(title).contains("Visa");
        assertThat(title).contains("1111");
    }

    @Test
    @MediumTest
    public void testCreditCardWithNickname_displaysNicknameAndLastFourAsTitle() throws Exception {
        mAutofillTestHelper.addServerCreditCard(
                SAMPLE_CARD_VISA, "Test nickname", CARD_ISSUER_UNKNOWN);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getFirstPaymentMethodPreference(activity);
        String title = cardPreference.getTitle().toString();
        assertThat(title).contains("Test nickname");
        assertThat(title).contains("1111");
    }

    @Test
    @MediumTest
    public void testCreditCardWithLongNickname_displaysCompleteNicknameAndLastFourAsTitle()
            throws Exception {
        mAutofillTestHelper.addServerCreditCard(
                SAMPLE_CARD_VISA, "This is a long nickname", CARD_ISSUER_UNKNOWN);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getFirstPaymentMethodPreference(activity);
        String title = cardPreference.getTitle().toString();
        assertThat(title).contains("This is a long nickname");
        assertThat(title).contains("1111");
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_VIRTUAL_CARD_METADATA})
    public void testCreditCardSummary_displaysVirtualCardEnrolledStatus() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getFirstPaymentMethodPreference(activity);
        String summary = cardPreference.getSummary().toString();
        assertThat(summary)
                .isEqualTo(activity.getString(R.string.autofill_virtual_card_enrolled_text));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_VIRTUAL_CARD_METADATA})
    public void testCreditCardSummary_displaysExpirationDateForUnenrolledCards() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_UNENROLLED);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getFirstPaymentMethodPreference(activity);
        String summary = cardPreference.getSummary().toString();
        // Verify that the summary (line below the card name and number) contains the expiration
        // date.
        assertThat(summary).contains(String.format("05/%s", AutofillTestHelper.twoDigitNextYear()));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_VIRTUAL_CARD_METADATA})
    public void testCreditCardSummary_displaysExpirationDateForNonVirtualCards() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_VISA);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getFirstPaymentMethodPreference(activity);
        String summary = cardPreference.getSummary().toString();
        assertThat(summary).contains(String.format("05/%s", AutofillTestHelper.twoDigitNextYear()));
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_VIRTUAL_CARD_METADATA})
    public void testCreditCardSummary_displaysExpirationDateForVirtualCardsWhenMetadataFlagOff()
            throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getFirstPaymentMethodPreference(activity);
        String summary = cardPreference.getSummary().toString();
        assertThat(summary).contains(String.format("05/%s", AutofillTestHelper.twoDigitNextYear()));
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testCreditCardSummary_whenCvcExists_doesNotDisplayCvcSavedMessageWhenCvcFlagOff()
            throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_WITH_CVC);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getFirstPaymentMethodPreference(activity);
        String summary = cardPreference.getSummary().toString();
        assertThat(summary).contains(String.format("05/%s", AutofillTestHelper.twoDigitNextYear()));
        assertThat(summary)
                .doesNotContain(
                        activity.getString(R.string.autofill_settings_page_cvc_saved_label));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testCreditCardSummary_whenCvcExists_displayCvcSavedMessage() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_WITH_CVC);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getCardPreference(activity);
        String summary = cardPreference.getSummary().toString();
        assertThat(summary)
                .contains(
                        activity.getString(
                                R.string.autofill_settings_page_summary_separated_by_pipe,
                                String.format("05/%s", AutofillTestHelper.twoDigitNextYear()),
                                activity.getString(
                                        R.string.autofill_settings_page_cvc_saved_label)));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testCreditCardSummary_whenCvcDoesNotExist_doesNotDisplayCvcSavedMessage()
            throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_VISA);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getCardPreference(activity);

        String summary = cardPreference.getSummary().toString();
        assertThat(summary).contains(String.format("05/%s", AutofillTestHelper.twoDigitNextYear()));
        assertThat(summary)
                .doesNotContain(
                        activity.getString(R.string.autofill_settings_page_cvc_saved_label));
    }

    @Test
    @SmallTest
    @Policies.Add({@Policies.Item(key = "AutofillCreditCardEnabled", string = "false")})
    public void testAutofillToggleDisabledByPolicy() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        ChromeSwitchPreference autofillTogglePreference =
                (ChromeSwitchPreference) getPreferenceScreen(activity).getPreference(0);
        Assert.assertFalse(autofillTogglePreference.isEnabled());
    }

    @Test
    @SmallTest
    @Policies.Add({@Policies.Item(key = "AutofillCreditCardEnabled", string = "true")})
    public void testAutofillToggleEnabledByPolicy() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        ChromeSwitchPreference autofillTogglePreference =
                (ChromeSwitchPreference) getPreferenceScreen(activity).getPreference(0);
        Assert.assertTrue(autofillTogglePreference.isEnabled());
    }

    @Test
    @SmallTest
    public void testAutofillToggleEnabledByDefault() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        ChromeSwitchPreference autofillTogglePreference =
                (ChromeSwitchPreference) getPreferenceScreen(activity).getPreference(0);
        Assert.assertTrue(autofillTogglePreference.isEnabled());
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    @DisableFeatures({
        ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE,
        ChromeFeatureList.AUTOFILL_ENABLE_LOCAL_IBAN
    })
    public void testMandatoryReauthToggle_displayToggle() throws Exception {
        // Simulate the pref was enabled previously, to ensure the toggle value is set
        // correspondingly.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, true);
                });
        // Simulate the user can authenticate with biometric or screen lock.
        when(mReauthenticatorMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.BIOMETRICS_AVAILABLE);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preference on the initial screen map is only Save and Fill toggle +
        // Mandatory Reauth toggle + Add Card button + Payment Apps.
        Assert.assertEquals(4, getPreferenceScreen(activity).getPreferenceCount());
        ChromeSwitchPreference mandatoryReauthPreference =
                (ChromeSwitchPreference) getPreferenceScreen(activity).getPreference(1);
        Assert.assertEquals(
                mandatoryReauthPreference.getTitle(),
                activity.getString(
                        R.string
                                .autofill_settings_page_enable_payment_method_mandatory_reauth_label));
        Assert.assertTrue(mandatoryReauthPreference.isChecked());
        Assert.assertTrue(mandatoryReauthPreference.isEnabled());
    }

    @Test
    @MediumTest
    // Use the policy to simulate AutofillCreditCard is disabled.
    @Policies.Add({@Policies.Item(key = "AutofillCreditCardEnabled", string = "false")})
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testMandatoryReauthToggle_disabledWhenAutofillDisabled() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that Reauth toggle is shown but greyed out when Autofill toggle is disabled.
        Assert.assertFalse(getMandatoryReauthPreference(activity).isEnabled());
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testMandatoryReauthToggle_disabledWhenBothBiometricAndScreenLockAreDisabled()
            throws Exception {
        // Simulate the user can't authenticate with neither biometric nor screen lock.
        when(mReauthenticatorMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.UNAVAILABLE);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Assert.assertFalse(getMandatoryReauthPreference(activity).isEnabled());
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testMandatoryReauthToggle_disabledWithCorrespondingPrefValue() throws Exception {
        // Simulate the pref was enabled previously, to ensure the toggle value is set
        // correspondingly.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, true);
                });
        // Simulate the user can't authenticate with neither biometric nor screen lock.
        when(mReauthenticatorMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.UNAVAILABLE);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Assert.assertFalse(getMandatoryReauthPreference(activity).isEnabled());
        // Also verify that the Reauth toggle is disabled with the corresponding pref value (greyed
        // out whe pref = ON).
        Assert.assertTrue(getMandatoryReauthPreference(activity).isChecked());
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testMandatoryReauthToggle_switchValueOnClicked() throws Exception {
        var optInHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                AutofillPaymentMethodsFragment.MANDATORY_REAUTH_OPT_IN_HISTOGRAM,
                                MandatoryReauthAuthenticationFlowEvent.FLOW_STARTED,
                                MandatoryReauthAuthenticationFlowEvent.FLOW_SUCCEEDED)
                        .build();
        // Initial state, Reauth pref is disabled by default.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, false);
                });
        // Simulate the user can authenticate with biometric or screen lock.
        when(mReauthenticatorMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.ONLY_LSKF_AVAILABLE);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the Reauth preference is not checked, since Reauth pref is disabled.
        Assert.assertFalse(getMandatoryReauthPreference(activity).isChecked());

        // Simulate the biometric authentication will succeed.
        setUpBiometricAuthenticationResult(/* success= */ true);
        // Simulate click on the Reauth toggle, trying to toggle on. Now Chrome is waiting for OS
        // authentication which should succeed.
        ThreadUtils.runOnUiThreadBlocking(getMandatoryReauthPreference(activity)::performClick);

        verify(mReauthenticatorMock).reauthenticate(notNull());
        // Verify that the Reauth toggle is now checked.
        Assert.assertTrue(getMandatoryReauthPreference(activity).isChecked());
        optInHistogram.assertExpected();
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testMandatoryReauthToggle_stayAtOldValueIfBiometricAuthFails() throws Exception {
        var optOutHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                AutofillPaymentMethodsFragment.MANDATORY_REAUTH_OPT_OUT_HISTOGRAM,
                                MandatoryReauthAuthenticationFlowEvent.FLOW_STARTED,
                                MandatoryReauthAuthenticationFlowEvent.FLOW_FAILED)
                        .build();
        // Simulate Reauth pref is enabled previously.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, true);
                });
        // Simulate the user can authenticate with biometric or screen lock.
        when(mReauthenticatorMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.BIOMETRICS_AVAILABLE);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the Reauth preference is checked.
        Assert.assertTrue(getMandatoryReauthPreference(activity).isChecked());

        // Simulate the biometric authentication will fail.
        setUpBiometricAuthenticationResult(/* success= */ false);
        // Simulate click on the Reauth toggle, trying to toggle off. Now Chrome is waiting for OS
        // authentication which should fail.
        ThreadUtils.runOnUiThreadBlocking(getMandatoryReauthPreference(activity)::performClick);

        verify(mReauthenticatorMock).reauthenticate(notNull());
        // Verify that the Reauth toggle is still checked since authentication failed.
        Assert.assertTrue(getMandatoryReauthPreference(activity).isChecked());
        optOutHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testMandatoryReauthToggle_noFidoToggle() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference expectedNullFidoToggle =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_FIDO);
        Assert.assertNull(expectedNullFidoToggle);
    }

    @Test
    @MediumTest
    public void testLocalCardEditWithReauth_reauthOnClicked() throws Exception {
        mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        // Simulate Reauth pref is enabled.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, true);
                });
        // Simulate the user can authenticate with biometric or screen lock.
        when(mReauthenticatorMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.ONLY_LSKF_AVAILABLE);
        var editCardReauthHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                AutofillPaymentMethodsFragment.MANDATORY_REAUTH_EDIT_CARD_HISTOGRAM,
                                MandatoryReauthAuthenticationFlowEvent.FLOW_STARTED,
                                MandatoryReauthAuthenticationFlowEvent.FLOW_SUCCEEDED)
                        .build();

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the Reauth preference is checked on non-automotive devices.
        if (!BuildInfo.getInstance().isAutomotive) {
            Assert.assertTrue(getMandatoryReauthPreference(activity).isChecked());
        }

        Preference cardPreference = getFirstPaymentMethodPreference(activity);
        String title = cardPreference.getTitle().toString();
        assertThat(title).contains("Visa");
        assertThat(title).contains("1111");

        // Simulate the biometric authentication will success.
        setUpBiometricAuthenticationResult(/* success= */ true);
        // Simulate click on the local card widget. Now Chrome is waiting for OS authentication.
        ThreadUtils.runOnUiThreadBlocking(cardPreference::performClick);
        // Now mReauthenticatorMock simulate success auth, which will open local card dialog
        // afterwards. Wait for the new dialog to be rendered.
        rule.waitForFragmentToBeShown();

        verify(mReauthenticatorMock).reauthenticate(notNull());
        // Verify that the local card edit dialog was shown.
        Assert.assertTrue(rule.getLastestShownFragment() instanceof AutofillLocalCardEditor);
        editCardReauthHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testLocalCardEditWithReauth_reauthOnClickedButFails() throws Exception {
        mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        // Simulate Reauth pref is enabled.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, true);
                });
        // Simulate the user can authenticate with biometric or screen lock.
        when(mReauthenticatorMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.BIOMETRICS_AVAILABLE);
        var editCardReauthHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                AutofillPaymentMethodsFragment.MANDATORY_REAUTH_EDIT_CARD_HISTOGRAM,
                                MandatoryReauthAuthenticationFlowEvent.FLOW_STARTED,
                                MandatoryReauthAuthenticationFlowEvent.FLOW_FAILED)
                        .build();

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the Reauth preference is checked on non-automotive devices.
        if (!BuildInfo.getInstance().isAutomotive) {
            Assert.assertTrue(getMandatoryReauthPreference(activity).isChecked());
        }

        Preference cardPreference = getFirstPaymentMethodPreference(activity);
        String title = cardPreference.getTitle().toString();
        assertThat(title).contains("Visa");
        assertThat(title).contains("1111");

        // Simulate the biometric authentication will fails.
        setUpBiometricAuthenticationResult(/* success= */ false);
        // Simulate click on the local card widget. Now Chrome is waiting for OS authentication
        // which should fail and hence the payment methods page should still be open.
        ThreadUtils.runOnUiThreadBlocking(cardPreference::performClick);

        verify(mReauthenticatorMock).reauthenticate(notNull());
        // Verify that the local card edit dialog was NOT shown.
        Assert.assertNull(rule.getLastestShownFragment());
        editCardReauthHistogram.assertExpected();
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_AUTO)
    public void testLocalCardEditWithReauth_reauthOnClickedOnAuto() throws Exception {
        mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        // Simulate Reauth pref is enabled.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, true);
                });
        // Simulate the user can authenticate with biometric or screen lock.
        when(mReauthenticatorMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.BIOMETRICS_AVAILABLE);
        var editCardReauthHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                AutofillPaymentMethodsFragment.MANDATORY_REAUTH_EDIT_CARD_HISTOGRAM,
                                MandatoryReauthAuthenticationFlowEvent.FLOW_STARTED,
                                MandatoryReauthAuthenticationFlowEvent.FLOW_SUCCEEDED)
                        .build();

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getFirstPaymentMethodPreference(activity);
        String title = cardPreference.getTitle().toString();
        assertThat(title).contains("Visa");
        assertThat(title).contains("1111");

        // Simulate the biometric authentication will success.
        setUpBiometricAuthenticationResult(/* success= */ true);
        // Simulate click on the local card widget. Now Chrome is waiting for OS authentication.
        ThreadUtils.runOnUiThreadBlocking(cardPreference::performClick);
        // Now mReauthenticatorMock simulate success auth, which will open local card dialog
        // afterwards. Wait for the new dialog to be rendered.
        rule.waitForFragmentToBeShown();

        verify(mReauthenticatorMock).reauthenticate(notNull());
        // Verify that the local card edit dialog was shown.
        Assert.assertTrue(rule.getLastestShownFragment() instanceof AutofillLocalCardEditor);
        editCardReauthHistogram.assertExpected();
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testLocalCardEditWithReauth_noReauthWhenReauthIsDisabled() throws Exception {
        mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        // Simulate Reauth pref is disabled.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, false);
                });
        // Simulate the user can authenticate with biometric or screen lock.
        when(mReauthenticatorMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.ONLY_LSKF_AVAILABLE);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the Reauth preference is not checked.
        Assert.assertFalse(getMandatoryReauthPreference(activity).isChecked());
        Preference cardPreference = getFirstPaymentMethodPreference(activity);
        String title = cardPreference.getTitle().toString();
        assertThat(title).contains("Visa");
        assertThat(title).contains("1111");

        // Simulate click on the local card widget.
        ThreadUtils.runOnUiThreadBlocking(cardPreference::performClick);
        // Since reauth pref is disabled, we will directly open local card dialog. Wait for the new
        // dialog to be rendered.
        rule.waitForFragmentToBeShown();

        verify(mReauthenticatorMock, never()).reauthenticate(notNull());
        // Verify that the local card edit dialog was shown.
        Assert.assertTrue(rule.getLastestShownFragment() instanceof AutofillLocalCardEditor);
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testLocalCardEditWithReauth_turnOnReauthAndVerifyReauthOnClick() throws Exception {
        mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);

        // Initial state, Reauth pref is disabled by default.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, false);
                });
        // Simulate the user can authenticate with biometric or screen lock.
        when(mReauthenticatorMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.BIOMETRICS_AVAILABLE);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Simulate the biometric authentication will succeed.
        setUpBiometricAuthenticationResult(/* success= */ true);
        // Simulate click on the Reauth toggle, trying to toggle on. Now Chrome is waiting for OS
        // authentication which should succeed.
        ThreadUtils.runOnUiThreadBlocking(getMandatoryReauthPreference(activity)::performClick);

        // Get the local card's widget.
        Preference cardPreference = getFirstPaymentMethodPreference(activity);
        String title = cardPreference.getTitle().toString();
        assertThat(title).contains("Visa");
        assertThat(title).contains("1111");

        // Simulate click on the local card widget. Now Chrome is waiting for OS authentication.
        ThreadUtils.runOnUiThreadBlocking(cardPreference::performClick);
        // Now mReauthenticatorMock simulate success auth, which will open local card dialog
        // afterwards. Wait for the new dialog to be rendered.
        rule.waitForFragmentToBeShown();

        // Verify there were 2 biometric authentication attempts, once for enabling mandatory
        // reauth, and another time for opening the local card edit page.
        verify(mReauthenticatorMock, times(2)).reauthenticate(notNull());
        // Verify that the local card edit dialog was shown.
        Assert.assertTrue(rule.getLastestShownFragment() instanceof AutofillLocalCardEditor);
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testLocalCardEditWithReauth_turnOffReauthAndVerifyNoReauthOnClick()
            throws Exception {
        mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);

        // Simulate Reauth pref is enabled.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, true);
                });
        // Simulate the user can authenticate with biometric or screen lock.
        when(mReauthenticatorMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.BIOMETRICS_AVAILABLE);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Simulate the biometric authentication will succeed.
        setUpBiometricAuthenticationResult(/* success= */ true);
        // Simulate click on the Reauth toggle, trying to toggle off. Now Chrome is waiting for OS
        // authentication which should succeed.
        ThreadUtils.runOnUiThreadBlocking(getMandatoryReauthPreference(activity)::performClick);

        // Get the local card's widget.
        Preference cardPreference = getFirstPaymentMethodPreference(activity);
        String title = cardPreference.getTitle().toString();
        assertThat(title).contains("Visa");
        assertThat(title).contains("1111");

        // Simulate click on the local card widget.
        ThreadUtils.runOnUiThreadBlocking(cardPreference::performClick);
        // Since reauth pref is disabled, we will directly open local card dialog. Wait for the new
        // dialog to be rendered.
        rule.waitForFragmentToBeShown();

        // Verify there was only 1 biometric authentication attempt, for disabling mandatory reauth.
        // After disabling, biometric authentication challenge should not be presented to open the
        // local card edit page.
        verify(mReauthenticatorMock, times(1)).reauthenticate(notNull());
        // Verify that the local card edit dialog was shown.
        Assert.assertTrue(rule.getLastestShownFragment() instanceof AutofillLocalCardEditor);
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_LOCAL_IBAN})
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testSaveCvcToggle_shown() throws Exception {
        // Initial state, Save Cvc pref is enabled previously.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.AUTOFILL_PAYMENT_CVC_STORAGE, true);
                });

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preference on the initial screen map is only Save and Fill toggle +
        // Reauth toggle + CVC storage toggle + Add Card button + Payment Apps.
        Assert.assertEquals(5, getPreferenceScreen(activity).getPreferenceCount());

        ChromeSwitchPreference saveCvcToggle =
                findPreferenceByKey(activity, AutofillPaymentMethodsFragment.PREF_SAVE_CVC);
        Assert.assertTrue(saveCvcToggle.isEnabled());
        Assert.assertTrue(saveCvcToggle.isChecked());
    }

    @Test
    @MediumTest
    @DisableFeatures({
        ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE,
        ChromeFeatureList.AUTOFILL_ENABLE_LOCAL_IBAN
    })
    public void testSaveCvcToggle_notShownWhenFeatureDisabled() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preference on the initial screen map is only Save and Fill toggle +
        // Reauth toggle + Add Card button + Payment Apps.
        Assert.assertEquals(4, getPreferenceScreen(activity).getPreferenceCount());
        Preference expectedNullCvcStorageToggle =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_SAVE_CVC);
        Assert.assertNull(expectedNullCvcStorageToggle);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    // Use the policy to simulate AutofillCreditCard is disabled.
    @Policies.Add({@Policies.Item(key = "AutofillCreditCardEnabled", string = "false")})
    public void testSaveCvcToggle_disabledAndOffWhenAutofillDisabled() throws Exception {
        // Initial state, Save Cvc pref is enabled previously.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.AUTOFILL_PAYMENT_CVC_STORAGE, true);
                });

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that save cvc toggle is shown but greyed out with OFF (even if it's previously
        // turned on) when Autofill toggle is disabled.
        ChromeSwitchPreference saveCvcToggle =
                findPreferenceByKey(activity, AutofillPaymentMethodsFragment.PREF_SAVE_CVC);
        Assert.assertFalse(saveCvcToggle.isEnabled());
        Assert.assertFalse(saveCvcToggle.isChecked());
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testDeleteSavedCvcsButton_notShownWhenFeatureDisabled() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_WITH_CVC);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference expectedNullDeleteSavedCvcsToggle =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_DELETE_SAVED_CVCS);
        Assert.assertNull(expectedNullDeleteSavedCvcsToggle);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testDeleteSavedCvcsButton_whenCvcExists_shown() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_WITH_CVC);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference deleteSavedCvcsToggle =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_DELETE_SAVED_CVCS);
        Assert.assertNotNull(deleteSavedCvcsToggle);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testDeleteSavedCvcsButton_whenCvcDoesNotExist_notShown() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_VISA);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference deleteSavedCvcsToggle =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_DELETE_SAVED_CVCS);
        Assert.assertNull(deleteSavedCvcsToggle);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testDeleteSavedCvcsButton_whenClicked_confirmationDialogIsShown() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_WITH_CVC);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        Preference deleteSavedCvcsToggle =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_DELETE_SAVED_CVCS);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    deleteSavedCvcsToggle.performClick();
                });

        onView(withText(R.string.autofill_delete_saved_cvcs_confirmation_dialog_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withText(R.string.autofill_delete_saved_cvcs_confirmation_dialog_message))
                .check(matches(isDisplayed()));
        onView(
                        withText(
                                R.string
                                        .autofill_delete_saved_cvcs_confirmation_dialog_delete_button_label))
                .check(matches(isDisplayed()));
        onView(withText(android.R.string.cancel)).check(matches(isDisplayed()));
    }

    // TODO(crbug.com/40287195): Test to verify the visibility of the delete saved CVCs button when
    // the AutofillCreditCardEnabled policy is set to false. Currently, Android-x86-rel targets
    // are unable to store credit card information when the policy is set to false.

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testDeleteSavedCvcsConfirmationDialogDeleteButton_whenClicked_deleteCvcs()
            throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_WITH_CVC);
        mMocker.mock(AutofillPaymentMethodsDelegateJni.TEST_HOOKS, mNativeMock);
        when(mNativeMock.init(any(Profile.class)))
                .thenReturn(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        Preference deleteSavedCvcsPreference =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_DELETE_SAVED_CVCS);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    deleteSavedCvcsPreference.performClick();
                });
        onView(
                        withText(
                                R.string
                                        .autofill_delete_saved_cvcs_confirmation_dialog_delete_button_label))
                .inRoot(isDialog())
                .perform(click());

        verify(mNativeMock).deleteSavedCvcs(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_LOCAL_IBAN})
    @Policies.Add({@Policies.Item(key = "AutofillCreditCardEnabled", string = "true")})
    public void testAddIbanButton_shownWhenAutofillEnabledAndIbanCriteriaMet() throws Exception {
        // Enable `ShouldShowAddIbanButtonOnSettingsPage` through indicating that the user has used
        // IBAN before.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.AUTOFILL_HAS_SEEN_IBAN, true);
                });
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Assert.assertNotNull(
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_ADD_IBAN));
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_LOCAL_IBAN})
    @Policies.Add({@Policies.Item(key = "AutofillCreditCardEnabled", string = "true")})
    public void testAddIbanButton_notShownWhenFeatureDisabled() throws Exception {
        // Enable `ShouldShowAddIbanButtonOnSettingsPage` through indicating that the user has used
        // IBAN before.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.AUTOFILL_HAS_SEEN_IBAN, true);
                });

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Assert.assertNull(
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_ADD_IBAN));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_LOCAL_IBAN})
    @Policies.Add({@Policies.Item(key = "AutofillCreditCardEnabled", string = "true")})
    public void testAddIbanButton_notShownWhenIbanCriteriaNotMet() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.AUTOFILL_HAS_SEEN_IBAN, false);
                });
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Assert.assertNull(
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_ADD_IBAN));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_LOCAL_IBAN})
    @Policies.Add({@Policies.Item(key = "AutofillCreditCardEnabled", string = "false")})
    public void testAddIbanButton_notShownWhenAutofillDisabled() throws Exception {
        // Enable `ShouldShowAddIbanButtonOnSettingsPage` through indicating that the user has used
        // IBAN before.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.AUTOFILL_HAS_SEEN_IBAN, true);
                });

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Assert.assertNull(
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_ADD_IBAN));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_LOCAL_IBAN})
    public void testAddIbanButtonClicked_opensLocalIbanEditor() throws Exception {
        // Enable `ShouldShowAddIbanButtonOnSettingsPage` through indicating that the user has used
        // IBAN before.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.AUTOFILL_HAS_SEEN_IBAN, true);
                });
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference addIbanPreference =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_ADD_IBAN);

        // Simulate click on the Add Iban button.
        ThreadUtils.runOnUiThreadBlocking(addIbanPreference::performClick);
        rule.waitForFragmentToBeShown();

        // Verify that the local IBAN editor was opened.
        Assert.assertTrue(rule.getLastestShownFragment() instanceof AutofillLocalIbanEditor);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_LOCAL_IBAN})
    public void testAddTwoIbans_displaysTwoLocalIbans() throws Exception {
        mAutofillTestHelper.addOrUpdateLocalIban(VALID_BELGIUM_LOCAL_IBAN);
        mAutofillTestHelper.addOrUpdateLocalIban(VALID_RUSSIA_LOCAL_IBAN);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Assert.assertEquals(
                2, getPreferenceCountWithKey(activity, AutofillPaymentMethodsFragment.PREF_IBAN));
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.AUTOFILL_ENABLE_LOCAL_IBAN,
        ChromeFeatureList.AUTOFILL_ENABLE_SERVER_IBAN
    })
    public void testAddTwoIbans_displaysLocalAndServerIbans() throws Exception {
        mAutofillTestHelper.addOrUpdateLocalIban(VALID_BELGIUM_LOCAL_IBAN);
        mAutofillTestHelper.addServerIban(VALID_SERVER_IBAN);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Assert.assertEquals(
                2, getPreferenceCountWithKey(activity, AutofillPaymentMethodsFragment.PREF_IBAN));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_LOCAL_IBAN})
    public void testLocalIbanWithNickname_displaysLabelAndNickname() throws Exception {
        mAutofillTestHelper.addOrUpdateLocalIban(VALID_BELGIUM_LOCAL_IBAN);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        Preference ibanPreference = getFirstPaymentMethodPreference(activity);

        assertThat(ibanPreference.getTitle().toString()).contains("BE");
        assertThat(ibanPreference.getSummary().toString()).isEqualTo("My IBAN");
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_LOCAL_IBAN})
    public void testLocalIbanWithoutNickname_displaysLabelOnly() throws Exception {
        mAutofillTestHelper.addOrUpdateLocalIban(VALID_RUSSIA_LOCAL_IBAN);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        Preference ibanPreference = getFirstPaymentMethodPreference(activity);

        assertThat(ibanPreference.getTitle().toString()).contains("RU");
        assertThat(ibanPreference.getSummary().toString()).contains("");
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_LOCAL_IBAN})
    public void testLocalIban_notShownWhenFeatureDisabled() throws Exception {
        mAutofillTestHelper.addOrUpdateLocalIban(VALID_BELGIUM_LOCAL_IBAN);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Assert.assertNull(
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_IBAN));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SERVER_IBAN})
    public void testServerIbanWithNickname_displaysNickname() throws Exception {
        mAutofillTestHelper.addServerIban(VALID_SERVER_IBAN);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        Preference ibanPreference = getFirstPaymentMethodPreference(activity);

        assertThat(ibanPreference.getSummary().toString()).isEqualTo("My IBAN");
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SERVER_IBAN})
    public void testServerIban_notShownWhenFeatureDisabled() throws Exception {
        mAutofillTestHelper.addServerIban(VALID_SERVER_IBAN);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Assert.assertNull(
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_IBAN));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SERVER_IBAN})
    public void testCustomUrlForServerIbanManagePage() throws Exception {
        mAutofillTestHelper.addServerIban(VALID_SERVER_IBAN);

        SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();
        mSettingsActivityTestRule
                .getFragment()
                .setServerIbanManageLinkOpenerCallbackForTesting(
                        mServerIbanManageLinkOpenerCallback);
        Preference ibanPreference = getFirstPaymentMethodPreference(settingsActivity);

        ThreadUtils.runOnUiThreadBlocking(ibanPreference::performClick);

        verify(mServerIbanManageLinkOpenerCallback)
                .onResult(
                        eq(
                                "https://pay.google.com/pay?p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign=payment_methods&id="
                                        + VALID_SERVER_IBAN.getInstrumentId()));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SERVER_IBAN})
    @CommandLineFlags.Add({ChromeSwitches.USE_SANDBOX_WALLET_ENVIRONMENT})
    public void testCustomUrlForServerIbanManagePage_sandboxEnabled() throws Exception {
        mAutofillTestHelper.addServerIban(VALID_SERVER_IBAN);

        SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();
        mSettingsActivityTestRule
                .getFragment()
                .setServerIbanManageLinkOpenerCallbackForTesting(
                        mServerIbanManageLinkOpenerCallback);
        Preference ibanPreference = getFirstPaymentMethodPreference(settingsActivity);

        ThreadUtils.runOnUiThreadBlocking(ibanPreference::performClick);

        verify(mServerIbanManageLinkOpenerCallback)
                .onResult(
                        eq(
                                "https://pay.sandbox.google.com/pay?p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign=payment_methods&id="
                                        + VALID_SERVER_IBAN.getInstrumentId()));
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    @EnableFeatures({
        ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE,
        ChromeFeatureList.AUTOFILL_ENABLE_LOCAL_IBAN
    })
    public void testAllToggles_mandatoryReauthEnabled_cvcStorageEnabled_localIbanEnabled()
            throws Exception {
        // Enable `ShouldShowAddIbanButtonOnSettingsPage` through indicating that the user has used
        // IBAN before.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.AUTOFILL_HAS_SEEN_IBAN, true);
                });
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preference on the initial screen map is only Save and Fill toggle +
        // Mandatory Reauth toggle + CVC storage toggle + Add Card button + Add IBAN button +
        // Payment Apps.
        Assert.assertEquals(6, getPreferenceScreen(activity).getPreferenceCount());
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SYNCING_OF_PIX_BANK_ACCOUNTS})
    public void pixAccountAvailable_showPayWithPixPreference() throws Exception {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preference for 'Pay with Pix' is displayed.
        Preference otherFinancialAccountsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment.PREF_FINANCIAL_ACCOUNTS_MANAGEMENT);
        assertThat(otherFinancialAccountsPref.getTitle().toString()).contains("Pix");
        // Verify that the second line on the preference has 'Pix' in it.
        assertThat(otherFinancialAccountsPref.getSummary().toString()).contains("Pix");
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SYNCING_OF_PIX_BANK_ACCOUNTS})
    public void pixAccountNotAvailable_doNotShowPayWithPixPreference() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preference for 'Manage other financial accounts' is not displayed.
        Preference otherFinancialAccountsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment.PREF_FINANCIAL_ACCOUNTS_MANAGEMENT);
        assertThat(otherFinancialAccountsPref).isNull();
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SYNCING_OF_PIX_BANK_ACCOUNTS})
    public void pixAccountAvailable_expOff_doNotShowPayWithPixPreference() throws Exception {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preference for 'Manage other financial accounts' is not displayed.
        Preference otherFinancialAccountsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment.PREF_FINANCIAL_ACCOUNTS_MANAGEMENT);
        assertThat(otherFinancialAccountsPref).isNull();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SYNCING_OF_PIX_BANK_ACCOUNTS})
    public void
            testOtherFinancialAccountsPreferenceClicked_opensFinancialAccountsManagementFragment()
                    throws Exception {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        Preference otherFinancialAccountsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment.PREF_FINANCIAL_ACCOUNTS_MANAGEMENT);

        // Simulate click on the preference/.
        ThreadUtils.runOnUiThreadBlocking(otherFinancialAccountsPref::performClick);
        rule.waitForFragmentToBeShown();

        // Verify that the financial accounts management fragment is opened.
        Assert.assertTrue(
                rule.getLastestShownFragment() instanceof FinancialAccountsManagementFragment);
    }

    private void setUpBiometricAuthenticationResult(boolean success) {
        // We have to manually invoke the passed-in callback.
        doAnswer(
                        invocation -> {
                            Callback<Boolean> callback = invocation.getArgument(0);
                            callback.onResult(success);
                            return true;
                        })
                .when(mReauthenticatorMock)
                .reauthenticate(notNull());
    }

    private ChromeSwitchPreference getMandatoryReauthPreference(SettingsActivity activity) {
        return findPreferenceByKey(activity, AutofillPaymentMethodsFragment.PREF_MANDATORY_REAUTH);
    }

    /** Find preference by the provided key, fail if no matched preference is found. */
    private ChromeSwitchPreference findPreferenceByKey(SettingsActivity activity, String key) {
        ChromeSwitchPreference preference =
                (ChromeSwitchPreference) getPreferenceScreen(activity).findPreference(key);
        Assert.assertNotNull(preference);
        return preference;
    }

    private static PreferenceScreen getPreferenceScreen(SettingsActivity activity) {
        return ((AutofillPaymentMethodsFragment) activity.getMainFragment()).getPreferenceScreen();
    }

    private static PrefService getPrefService() {
        return UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
    }

    private static Preference getFirstPaymentMethodPreference(SettingsActivity activity) {
        boolean mandatoryReauthToggleShown = !BuildInfo.getInstance().isAutomotive;
        boolean saveCvcToggleShown =
                ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE);
        // The first payment method will come after the general settings for enabling
        // autofill, enabling mandatory re-auth (if available), and enabling CVC storage (if
        // available).
        int firstPaymentMethodIndex =
                1 + (mandatoryReauthToggleShown ? 1 : 0) + (saveCvcToggleShown ? 1 : 0);
        return getPreferenceScreen(activity).getPreference(firstPaymentMethodIndex);
    }

    private Preference getCardPreference(SettingsActivity activity) {
        for (int i = 0; i < getPreferenceScreen(activity).getPreferenceCount(); i++) {
            Preference preference = getPreferenceScreen(activity).getPreference(i);
            if (preference.getTitle() != null
                    && CARD_ISSUER_NETWORKS.stream()
                            .anyMatch(
                                    issuer ->
                                            preference
                                                    .getTitle()
                                                    .toString()
                                                    .toLowerCase()
                                                    .contains(issuer))) {
                return preference;
            }
        }
        Assert.fail("Failed to find the card preference.");
        return null;
    }

    private int getPreferenceCountWithKey(SettingsActivity activity, String preferenceKey) {
        int matchingPreferenceCount = 0;

        for (int i = 0; i < getPreferenceScreen(activity).getPreferenceCount(); i++) {
            Preference preference = getPreferenceScreen(activity).getPreference(i);
            if (preference.getKey() != null && preference.getKey().equals(preferenceKey)) {
                matchingPreferenceCount++;
            }
        }
        return matchingPreferenceCount;
    }
}
