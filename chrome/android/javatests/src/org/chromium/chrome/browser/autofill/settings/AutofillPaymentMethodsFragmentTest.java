// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasData;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;

import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;
import androidx.test.espresso.intent.Intents;
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
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.autofill.AndroidAutofillAvailabilityStatus;
import org.chromium.chrome.browser.autofill.AutofillClientProviderUtils;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.GoogleWalletLauncher;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.autofill.options.AutofillOptionsFragment;
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
import org.chromium.components.autofill.payments.Ewallet;
import org.chromium.components.autofill.payments.PaymentInstrument;
import org.chromium.components.browser_ui.settings.CardWithButtonPreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.payments.AndroidPaymentAppFactory;
import org.chromium.components.payments.PackageManagerDelegate;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Instrumentation tests for AutofillPaymentMethodsFragment. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({
    ChromeFeatureList.AUTOFILL_ENABLE_LOYALTY_CARDS_FILLING,
})
@DisableFeatures({
    ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_AMERICAN_EXPRESS,
    ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_BMO,
    ChromeFeatureList.AUTOFILL_ENABLE_FLAT_RATE_CARD_BENEFITS_FROM_CURINOS,
})
@Batch(Batch.PER_CLASS)
public class AutofillPaymentMethodsFragmentTest {
    @Rule public final AutofillTestRule mAutofillTestRule = new AutofillTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Rule
    public final SettingsActivityTestRule<AutofillPaymentMethodsFragment>
            mSettingsActivityTestRule =
                    new SettingsActivityTestRule<>(AutofillPaymentMethodsFragment.class);

    @Mock private ReauthenticatorBridge mReauthenticatorMock;
    @Mock private AutofillPaymentMethodsDelegate.Natives mNativeMock;
    @Mock private Callback<String> mServerIbanManageLinkOpenerCallback;
    @Mock private PackageManagerDelegate mPackageManagerDelegate;

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
                    /* networkAndLastFourDigits= */ "",
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
                    /* networkAndLastFourDigits= */ "",
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
                    /* networkAndLastFourDigits= */ "",
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
                    /* cvc= */ "",
                    /* issuerId= */ "",
                    /* benefitSource= */ "",
                    /* productTermsUrl= */ null);
    private static final CreditCard SAMPLE_VIRTUAL_CARD_ENROLLED =
            new CreditCard(
                    /* guid= */ "",
                    /* origin= */ "",
                    /* isLocal= */ false,
                    /* isVirtual= */ false,
                    /* name= */ "John Doe",
                    /* number= */ "4444333322221111",
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
                    /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState.ENROLLED,
                    /* productDescription= */ "",
                    /* cardNameForAutofillDisplay= */ "",
                    /* obfuscatedLastFourDigits= */ "",
                    /* cvc= */ "",
                    /* issuerId= */ "",
                    /* benefitSource= */ "",
                    /* productTermsUrl= */ null);
    private static final CreditCard SAMPLE_CARD_WITH_CVC =
            new CreditCard(
                    /* guid= */ "",
                    /* origin= */ "",
                    /* isLocal= */ false,
                    /* isVirtual= */ false,
                    /* name= */ "John Doe",
                    /* number= */ "4444333322221111",
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
                    /* benefitSource= */ "",
                    /* productTermsUrl= */ null);

    private static final Ewallet EWALLET_ACCOUNT =
            new Ewallet.Builder()
                    .setPaymentInstrument(
                            new PaymentInstrument.Builder()
                                    .setInstrumentId(100)
                                    .setNickname("nickname")
                                    .setSupportedPaymentRails(new int[] {0})
                                    .setIsFidoEnrolled(true)
                                    .build())
                    .setEwalletName("eWallet name")
                    .setAccountDisplayName("Ewallet account display name")
                    .build();

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
        Intents.init();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.SETTING_TURNED_OFF);
                });
    }

    @After
    public void tearDown() throws TimeoutException {
        Intents.release();
        mAutofillTestHelper.clearAllDataForTesting();
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    @DisableFeatures({
        ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE,
    })
    public void testTwoCreditCards_displaysTwoServerCards() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_VISA);
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_MASTERCARD);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preferences on the initial screen map to Save and Fill toggle + Mandatory
        // Reauth toggle + 2 Cards + Add Card button + Payment Apps + Loyalty cards.
        assertEquals(7, getPreferenceScreen(activity).getPreferenceCount());
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_AUTO)
    @DisableFeatures({
        ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE,
    })
    public void testTwoCreditCards_displaysTwoServerCards_mandatoryReauthNotShownOnAutomotive()
            throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_VISA);
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_MASTERCARD);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preferences on the initial screen map to Save and Fill toggle + 2 Cards
        // + Add Card button + Payment Apps + Loyalty cards.
        assertEquals(6, getPreferenceScreen(activity).getPreferenceCount());
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_AUTO)
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void
            testTwoCreditCards_displaysTwoServerCards_mandatoryReauthNotShownOnAutomotive_ButCvcStorageEnabled()
                    throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_VISA);
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_MASTERCARD);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preferences on the initial screen map to Save and Fill toggle + CVC
        // storage toggle + 2 Cards + Add Card button + Payment Apps + Loyalty cards.
        assertEquals(7, getPreferenceScreen(activity).getPreferenceCount());
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
    public void testCreditCardSummary_displaysExpirationDateForNonVirtualCards() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_VISA);

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
        assertFalse(autofillTogglePreference.isEnabled());
    }

    @Test
    @SmallTest
    @Policies.Add({@Policies.Item(key = "AutofillCreditCardEnabled", string = "true")})
    public void testAutofillToggleEnabledByPolicy() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        ChromeSwitchPreference autofillTogglePreference =
                (ChromeSwitchPreference) getPreferenceScreen(activity).getPreference(0);
        assertTrue(autofillTogglePreference.isEnabled());
    }

    @Test
    @SmallTest
    public void testAutofillToggleEnabledByDefault() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        ChromeSwitchPreference autofillTogglePreference =
                (ChromeSwitchPreference) getPreferenceScreen(activity).getPreference(0);
        assertTrue(autofillTogglePreference.isEnabled());
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    @DisableFeatures({
        ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE,
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
        // Mandatory Reauth toggle + Add Card button + Payment Apps + Loyalty cards.
        assertEquals(5, getPreferenceScreen(activity).getPreferenceCount());
        ChromeSwitchPreference mandatoryReauthPreference =
                (ChromeSwitchPreference) getPreferenceScreen(activity).getPreference(1);
        assertEquals(
                mandatoryReauthPreference.getTitle(),
                activity.getString(
                        R.string
                                .autofill_settings_page_enable_payment_method_mandatory_reauth_label));
        assertTrue(mandatoryReauthPreference.isChecked());
        assertTrue(mandatoryReauthPreference.isEnabled());
    }

    @Test
    @MediumTest
    // Use the policy to simulate AutofillCreditCard is disabled.
    @Policies.Add({@Policies.Item(key = "AutofillCreditCardEnabled", string = "false")})
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testMandatoryReauthToggle_disabledWhenAutofillDisabled() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that Reauth toggle is shown but greyed out when Autofill toggle is disabled.
        assertFalse(getMandatoryReauthPreference(activity).isEnabled());
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

        assertFalse(getMandatoryReauthPreference(activity).isEnabled());
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

        assertFalse(getMandatoryReauthPreference(activity).isEnabled());
        // Also verify that the Reauth toggle is disabled with the corresponding pref value (greyed
        // out whe pref = ON).
        assertTrue(getMandatoryReauthPreference(activity).isChecked());
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
        assertFalse(getMandatoryReauthPreference(activity).isChecked());

        // Simulate the biometric authentication will succeed.
        setUpBiometricAuthenticationResult(/* success= */ true);
        // Simulate click on the Reauth toggle, trying to toggle on. Now Chrome is waiting for OS
        // authentication which should succeed.
        ThreadUtils.runOnUiThreadBlocking(getMandatoryReauthPreference(activity)::performClick);

        verify(mReauthenticatorMock).reauthenticate(notNull());
        // Verify that the Reauth toggle is now checked.
        assertTrue(getMandatoryReauthPreference(activity).isChecked());
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
        assertTrue(getMandatoryReauthPreference(activity).isChecked());

        // Simulate the biometric authentication will fail.
        setUpBiometricAuthenticationResult(/* success= */ false);
        // Simulate click on the Reauth toggle, trying to toggle off. Now Chrome is waiting for OS
        // authentication which should fail.
        ThreadUtils.runOnUiThreadBlocking(getMandatoryReauthPreference(activity)::performClick);

        verify(mReauthenticatorMock).reauthenticate(notNull());
        // Verify that the Reauth toggle is still checked since authentication failed.
        assertTrue(getMandatoryReauthPreference(activity).isChecked());
        optOutHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testMandatoryReauthToggle_noFidoToggle() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference expectedNullFidoToggle =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_FIDO);
        assertNull(expectedNullFidoToggle);
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
        if (!DeviceInfo.isAutomotive()) {
            assertTrue(getMandatoryReauthPreference(activity).isChecked());
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
        mAutofillTestRule.waitForFragmentToBeShown();

        verify(mReauthenticatorMock).reauthenticate(notNull());
        // Verify that the local card edit dialog was shown.
        assertTrue(mAutofillTestRule.getLastestShownFragment() instanceof AutofillLocalCardEditor);
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
        if (!DeviceInfo.isAutomotive()) {
            assertTrue(getMandatoryReauthPreference(activity).isChecked());
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
        assertNull(mAutofillTestRule.getLastestShownFragment());
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
        mAutofillTestRule.waitForFragmentToBeShown();

        verify(mReauthenticatorMock).reauthenticate(notNull());
        // Verify that the local card edit dialog was shown.
        assertTrue(mAutofillTestRule.getLastestShownFragment() instanceof AutofillLocalCardEditor);
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
        assertFalse(getMandatoryReauthPreference(activity).isChecked());
        Preference cardPreference = getFirstPaymentMethodPreference(activity);
        String title = cardPreference.getTitle().toString();
        assertThat(title).contains("Visa");
        assertThat(title).contains("1111");

        // Simulate click on the local card widget.
        ThreadUtils.runOnUiThreadBlocking(cardPreference::performClick);
        // Since reauth pref is disabled, we will directly open local card dialog. Wait for the new
        // dialog to be rendered.
        mAutofillTestRule.waitForFragmentToBeShown();

        verify(mReauthenticatorMock, never()).reauthenticate(notNull());
        // Verify that the local card edit dialog was shown.
        assertTrue(mAutofillTestRule.getLastestShownFragment() instanceof AutofillLocalCardEditor);
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
        mAutofillTestRule.waitForFragmentToBeShown();

        // Verify there were 2 biometric authentication attempts, once for enabling mandatory
        // reauth, and another time for opening the local card edit page.
        verify(mReauthenticatorMock, times(2)).reauthenticate(notNull());
        // Verify that the local card edit dialog was shown.
        assertTrue(mAutofillTestRule.getLastestShownFragment() instanceof AutofillLocalCardEditor);
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
        mAutofillTestRule.waitForFragmentToBeShown();

        // Verify there was only 1 biometric authentication attempt, for disabling mandatory reauth.
        // After disabling, biometric authentication challenge should not be presented to open the
        // local card edit page.
        verify(mReauthenticatorMock, times(1)).reauthenticate(notNull());
        // Verify that the local card edit dialog was shown.
        assertTrue(mAutofillTestRule.getLastestShownFragment() instanceof AutofillLocalCardEditor);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testSaveCvcToggle_shown() throws Exception {
        // Initial state, Save Cvc pref is enabled previously.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.AUTOFILL_PAYMENT_CVC_STORAGE, true);
                });

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preference on the initial screen map is only Save and Fill toggle +
        // Reauth toggle + CVC storage toggle + Add Card button + Payment Apps + Loyalty cards.
        assertEquals(6, getPreferenceScreen(activity).getPreferenceCount());

        ChromeSwitchPreference saveCvcToggle =
                findPreferenceByKey(activity, AutofillPaymentMethodsFragment.PREF_SAVE_CVC);
        assertTrue(saveCvcToggle.isEnabled());
        assertTrue(saveCvcToggle.isChecked());
    }

    @Test
    @MediumTest
    @DisableFeatures({
        ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE,
    })
    public void testSaveCvcToggle_notShownWhenFeatureDisabled() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preference on the initial screen map is only Save and Fill toggle +
        // Reauth toggle + Add Card button + Payment Apps + Loyalty cards.
        assertEquals(5, getPreferenceScreen(activity).getPreferenceCount());
        Preference expectedNullCvcStorageToggle =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_SAVE_CVC);
        assertNull(expectedNullCvcStorageToggle);
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
        assertFalse(saveCvcToggle.isEnabled());
        assertFalse(saveCvcToggle.isChecked());
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
        assertNull(expectedNullDeleteSavedCvcsToggle);
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
        assertNotNull(deleteSavedCvcsToggle);
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
        assertNull(deleteSavedCvcsToggle);
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
        AutofillPaymentMethodsDelegateJni.setInstanceForTesting(mNativeMock);
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
    @Policies.Add({@Policies.Item(key = "AutofillCreditCardEnabled", string = "true")})
    public void testAddIbanButton_shownWhenAutofillEnabledAndIbanCriteriaMet() throws Exception {
        // Enable `ShouldShowAddIbanButtonOnSettingsPage` through indicating that the user has used
        // IBAN before.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.AUTOFILL_HAS_SEEN_IBAN, true);
                });
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        assertNotNull(
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_ADD_IBAN));
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "AutofillCreditCardEnabled", string = "true")})
    public void testAddIbanButton_notShownWhenIbanCriteriaNotMet() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.AUTOFILL_HAS_SEEN_IBAN, false);
                });
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        assertNull(
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_ADD_IBAN));
    }

    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "AutofillCreditCardEnabled", string = "false")})
    public void testAddIbanButton_notShownWhenAutofillDisabled() throws Exception {
        // Enable `ShouldShowAddIbanButtonOnSettingsPage` through indicating that the user has used
        // IBAN before.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.AUTOFILL_HAS_SEEN_IBAN, true);
                });

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        assertNull(
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_ADD_IBAN));
    }

    @Test
    @MediumTest
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
        mAutofillTestRule.waitForFragmentToBeShown();

        // Verify that the local IBAN editor was opened.
        assertTrue(mAutofillTestRule.getLastestShownFragment() instanceof AutofillLocalIbanEditor);
    }

    @Test
    @MediumTest
    public void testAddTwoIbans_displaysTwoLocalIbans() throws Exception {
        mAutofillTestHelper.addOrUpdateLocalIban(VALID_BELGIUM_LOCAL_IBAN);
        mAutofillTestHelper.addOrUpdateLocalIban(VALID_RUSSIA_LOCAL_IBAN);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        assertEquals(
                2, getPreferenceCountWithKey(activity, AutofillPaymentMethodsFragment.PREF_IBAN));
    }

    @Test
    @MediumTest
    public void testAddTwoIbans_displaysLocalAndServerIbans() throws Exception {
        mAutofillTestHelper.addOrUpdateLocalIban(VALID_BELGIUM_LOCAL_IBAN);
        mAutofillTestHelper.addServerIban(VALID_SERVER_IBAN);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        assertEquals(
                2, getPreferenceCountWithKey(activity, AutofillPaymentMethodsFragment.PREF_IBAN));
    }

    @Test
    @MediumTest
    public void testLocalIbanWithNickname_displaysLabelAndNickname() throws Exception {
        mAutofillTestHelper.addOrUpdateLocalIban(VALID_BELGIUM_LOCAL_IBAN);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        Preference ibanPreference = getFirstPaymentMethodPreference(activity);

        assertThat(ibanPreference.getTitle().toString()).contains("BE");
        assertThat(ibanPreference.getSummary().toString()).isEqualTo("My IBAN");
    }

    @Test
    @MediumTest
    public void testLocalIbanWithoutNickname_displaysLabelOnly() throws Exception {
        mAutofillTestHelper.addOrUpdateLocalIban(VALID_RUSSIA_LOCAL_IBAN);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        Preference ibanPreference = getFirstPaymentMethodPreference(activity);

        assertThat(ibanPreference.getTitle().toString()).contains("RU");
        assertThat(ibanPreference.getSummary().toString()).contains("");
    }

    @Test
    @MediumTest
    public void testServerIbanWithNickname_displaysNickname() throws Exception {
        mAutofillTestHelper.addServerIban(VALID_SERVER_IBAN);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        Preference ibanPreference = getFirstPaymentMethodPreference(activity);

        assertThat(ibanPreference.getSummary().toString()).isEqualTo("My IBAN");
    }

    @Test
    @MediumTest
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
        // Payment Apps + Loyalty cards.
        assertEquals(7, getPreferenceScreen(activity).getPreferenceCount());
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS,
    })
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM})
    public void financialAccountAvailable_showPayWithEwalletPreference() throws Exception {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preference for 'Pay with eWallet' is displayed.
        Preference otherFinancialAccountsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment.PREF_FINANCIAL_ACCOUNTS_MANAGEMENT);
        assertThat(otherFinancialAccountsPref.getTitle().toString()).contains("eWallet");
        assertFalse(otherFinancialAccountsPref.getTitle().toString().contains("Pix"));
        // Verify that the second line on the preference has only 'eWallet' in it.
        assertThat(otherFinancialAccountsPref.getSummary().toString()).contains("eWallet");
        assertFalse(otherFinancialAccountsPref.getTitle().toString().contains("Pix"));
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS,
    })
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM})
    public void financialAccountAvailable_showPayWithPixPreference() throws Exception {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preference for 'Pay with Pix' is displayed.
        Preference otherFinancialAccountsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment.PREF_FINANCIAL_ACCOUNTS_MANAGEMENT);
        assertThat(otherFinancialAccountsPref.getTitle().toString()).contains("Pix");
        assertFalse(otherFinancialAccountsPref.getTitle().toString().contains("eWallet"));
        // Verify that the second line on the preference has only 'Pix' in it.
        assertThat(otherFinancialAccountsPref.getSummary().toString()).contains("Pix");
        assertFalse(otherFinancialAccountsPref.getTitle().toString().contains("eWallet"));
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS,
    })
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM})
    public void financialAccountAvailable_showPayWithEwalletAndPixPreference() throws Exception {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preference for 'Pay with eWallet and Pix' is displayed.
        Preference otherFinancialAccountsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment.PREF_FINANCIAL_ACCOUNTS_MANAGEMENT);
        assertThat(otherFinancialAccountsPref.getTitle().toString()).contains("eWallet and Pix");
        // Verify that the second line on the preference has 'eWallet and Pix' in it.
        assertThat(otherFinancialAccountsPref.getSummary().toString()).contains("eWallet and Pix");
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS})
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM})
    public void financialAccountNotAvailable_doNotShowOtherFinancalPreference() throws Exception {
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
    @DisableFeatures({
        ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS,
        ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM
    })
    public void financialAccountAvailable_expOff_doNotShowPayWithEwalletPreference()
            throws Exception {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);

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
    @EnableFeatures({ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS})
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM})
    public void testEwalletAccountsPreferenceClicked_opensFinancialAccountsManagementFragment()
            throws Exception {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        Preference otherFinancialAccountsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment.PREF_FINANCIAL_ACCOUNTS_MANAGEMENT);

        // Simulate click on the preference.
        ThreadUtils.runOnUiThreadBlocking(otherFinancialAccountsPref::performClick);
        mAutofillTestRule.waitForFragmentToBeShown();

        // Verify that the financial accounts management fragment is opened.
        assertTrue(
                mAutofillTestRule.getLastestShownFragment()
                        instanceof FinancialAccountsManagementFragment);
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM})
    public void testPixAccountsPreferenceClicked_opensFinancialAccountsManagementFragment()
            throws Exception {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        Preference otherFinancialAccountsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment.PREF_FINANCIAL_ACCOUNTS_MANAGEMENT);

        // Simulate click on the preference/.
        ThreadUtils.runOnUiThreadBlocking(otherFinancialAccountsPref::performClick);
        mAutofillTestRule.waitForFragmentToBeShown();

        // Verify that the financial accounts management fragment is opened.
        assertTrue(
                mAutofillTestRule.getLastestShownFragment()
                        instanceof FinancialAccountsManagementFragment);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS,
        ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM
    })
    public void financialAccountAvailable_separatePixPreferenceItem_showPayWithPixPreferenceOnly()
            throws Exception {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preference for 'Pay with Pix' is displayed.
        Preference pixFinancialAccountsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment.PREF_FINANCIAL_ACCOUNTS_MANAGEMENT);
        assertThat(pixFinancialAccountsPref).isNotNull();
        Preference nonCardPaymentMethodsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment
                                        .PREF_NON_CARD_PAYMENT_METHODS_MANAGEMENT);
        assertThat(nonCardPaymentMethodsPref).isNull();
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS,
        ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM
    })
    public void
            financialAccountAvailable_separatePixPreferenceItem_showPayWithNonCardPaymentPreferenceOnly()
                    throws Exception {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that only the preference for 'Pay with non-card payment methods' is displayed.
        Preference pixFinancialAccountsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment.PREF_FINANCIAL_ACCOUNTS_MANAGEMENT);
        assertThat(pixFinancialAccountsPref).isNull();
        Preference nonCardPaymentMethodsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment
                                        .PREF_NON_CARD_PAYMENT_METHODS_MANAGEMENT);
        assertThat(nonCardPaymentMethodsPref).isNotNull();
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS,
        ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM
    })
    public void
            financialAccountAvailable_separatePixPreferenceItem_showPayWithPixAndNonCardPaymentPreferences()
                    throws Exception {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that both the preferences for 'Pay with Pix' and 'Pay with non-card payment
        // methods' are displayed.
        Preference pixFinancialAccountsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment.PREF_FINANCIAL_ACCOUNTS_MANAGEMENT);
        assertThat(pixFinancialAccountsPref).isNotNull();
        Preference nonCardPaymentMethodsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment
                                        .PREF_NON_CARD_PAYMENT_METHODS_MANAGEMENT);
        assertThat(nonCardPaymentMethodsPref).isNotNull();
        assertThat(pixFinancialAccountsPref).isNotEqualTo(nonCardPaymentMethodsPref);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS,
        ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM
    })
    public void
            financialAccountAvailable_separatePixPreferenceItem_showNeitherPayWithPixNorNonCardPaymentPreferences()
                    throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that none of the preferences for 'Pay with Pix' and 'Pay with non-card payment
        // methods' are displayed.
        Preference pixFinancialAccountsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment.PREF_FINANCIAL_ACCOUNTS_MANAGEMENT);
        assertThat(pixFinancialAccountsPref).isNull();
        Preference nonCardPaymentMethodsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment
                                        .PREF_NON_CARD_PAYMENT_METHODS_MANAGEMENT);
        assertThat(nonCardPaymentMethodsPref).isNull();
    }

    // This test verifies that when the A2A flow has been shown at least once (as tracked by the
    // FACILITATED_PAYMENTS_A2A_TRIGGERED_ONCE pref), the non-card payment preferences are shown.
    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS,
        ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM,
        ChromeFeatureList.FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT
    })
    public void a2aFlowShownAtLeastOnce_noEwalletAdded_showNonCardPaymentPreferences()
            throws Exception {
        // Note: no eWallet added.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.FACILITATED_PAYMENTS_A2A_TRIGGERED_ONCE, true);
                });

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preference for 'Pay with non-card payment methods' is displayed.
        Preference nonCardPaymentMethodsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment
                                        .PREF_NON_CARD_PAYMENT_METHODS_MANAGEMENT);
        assertThat(nonCardPaymentMethodsPref).isNotNull();
    }

    // This test verifies that when the A2A flow has never been shown (as tracked by the
    // FACILITATED_PAYMENTS_A2A_TRIGGERED_ONCE pref), the non-card payment preferences are not
    // shown.
    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS,
        ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM,
        ChromeFeatureList.FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT
    })
    public void a2aFlowNeverShown_noEwalletAdded_nonCardPaymentPreferencesNotShown()
            throws Exception {
        // Note: no eWallet added.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.FACILITATED_PAYMENTS_A2A_TRIGGERED_ONCE, false);
                });

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preference for 'Pay with non-card payment methods' is not displayed.
        Preference nonCardPaymentMethodsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment
                                        .PREF_NON_CARD_PAYMENT_METHODS_MANAGEMENT);
        assertThat(nonCardPaymentMethodsPref).isNull();
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS,
        ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM
    })
    @DisableFeatures({ChromeFeatureList.FACILITATED_PAYMENTS_ENABLE_A2A_PAYMENT})
    public void
            a2aShownAtLeastOnce_a2aFlagDisabled_noEwalletAdded_nonCardPaymentPreferencesNotShown()
                    throws Exception {
        // Note: no eWallet added.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.FACILITATED_PAYMENTS_A2A_TRIGGERED_ONCE, true);
                });

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preference for 'Pay with non-card payment methods' is not displayed.
        Preference nonCardPaymentMethodsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment
                                        .PREF_NON_CARD_PAYMENT_METHODS_MANAGEMENT);
        assertThat(nonCardPaymentMethodsPref).isNull();
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS,
        ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM
    })
    public void
            separatePixPreferenceItem_testNonCardPaymentMethodsPreferenceClicked_opensNonCardPaymentMethodsManagementFragment()
                    throws Exception {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        Preference nonCardPaymentMethodsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment
                                        .PREF_NON_CARD_PAYMENT_METHODS_MANAGEMENT);

        // Simulate click on the preference.
        ThreadUtils.runOnUiThreadBlocking(nonCardPaymentMethodsPref::performClick);
        mAutofillTestRule.waitForFragmentToBeShown();

        // Verify that the financial accounts management fragment is opened.
        assertTrue(
                mAutofillTestRule.getLastestShownFragment()
                        instanceof NonCardPaymentMethodsManagementFragment);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS,
        ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM
    })
    public void
            separatePixPreferenceItem_testPixAccountsPreferenceClicked_opensFinancialAccountsManagementFragment()
                    throws Exception {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        Preference nonCardPaymentMethodsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment.PREF_FINANCIAL_ACCOUNTS_MANAGEMENT);

        // Simulate click on the preference.
        ThreadUtils.runOnUiThreadBlocking(nonCardPaymentMethodsPref::performClick);
        mAutofillTestRule.waitForFragmentToBeShown();

        // Verify that the financial accounts management fragment is opened.
        assertTrue(
                mAutofillTestRule.getLastestShownFragment()
                        instanceof FinancialAccountsManagementFragment);
    }

    @Test
    @MediumTest
    public void testFirstCardPromo_promoShownAndButtonOpensAddCard() throws Exception {
        var cardsShownWithoutExistingCardsHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                AutofillPaymentMethodsFragment
                                        .VIEWED_CARDS_WITHOUT_EXISTING_CARDS_HISTOGRAM,
                                true)
                        .build();

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        cardsShownWithoutExistingCardsHistogram.assertExpected();

        Preference promoPreference = getFirstPaymentMethodPreference(activity);
        assertTrue(promoPreference instanceof CardWithButtonPreference);
        String title = promoPreference.getTitle().toString();
        assertThat(title)
                .isEqualTo(activity.getString(R.string.autofill_create_first_credit_card_title));
        String summary = promoPreference.getSummary().toString();
        assertThat(summary)
                .isEqualTo(activity.getString(R.string.autofill_create_first_credit_card_summary));

        onView(withId(R.id.card_button)).perform(scrollTo(), click());

        // Verify that the local card editor fragment is opened.
        assertTrue(mAutofillTestRule.getLastestShownFragment() instanceof AutofillLocalCardEditor);
    }

    @Test
    @MediumTest
    public void testFirstCardPromo_promoNotShownWithExistingCards() throws Exception {
        var cardsShownWithoutExistingCardsHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                AutofillPaymentMethodsFragment
                                        .VIEWED_CARDS_WITHOUT_EXISTING_CARDS_HISTOGRAM,
                                false)
                        .build();

        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_VISA);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        cardsShownWithoutExistingCardsHistogram.assertExpected();

        Preference cardPreference = getFirstPaymentMethodPreference(activity);
        assertFalse(cardPreference instanceof CardWithButtonPreference);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.AUTOFILL_ENABLE_LOYALTY_CARDS_FILLING,
    })
    public void testLoyaltyCards_showsGoogleWalletLink() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the link to manage loyalty cards in Google wallet is displayed.
        Preference loyaltyCardsPref =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_LOYALTY_CARDS);
        assertNotNull(loyaltyCardsPref);
        assertThat(loyaltyCardsPref.getTitle().toString()).contains("Loyalty cards");
        assertThat(loyaltyCardsPref.getSummary().toString()).contains("Google Wallet");
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.AUTOFILL_ENABLE_LOYALTY_CARDS_FILLING,
    })
    public void testLoyaltyCards_linkOpensNewActivity() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the link to manage loyalty cards in Google Wallet is displayed.
        Preference loyaltyCardsPref =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_LOYALTY_CARDS);
        // Simulate click on the loyalty card row.
        ThreadUtils.runOnUiThreadBlocking(loyaltyCardsPref::performClick);

        intended(hasData(GoogleWalletLauncher.GOOGLE_WALLET_PASSES_URL));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN})
    public void testSettingsState_SaveAndFillPaymentMethodsDisabledInThirdPartyMode()
            throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Save and fill payment methods toggle is shown and disabled.
        ChromeSwitchPreference saveAndFillPaymentMethodsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment.PREF_SAVE_AND_FILL_PAYMENT_METHODS);
        assertFalse(saveAndFillPaymentMethodsPref.isEnabled());
        assertFalse(saveAndFillPaymentMethodsPref.isChecked());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN
    })
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testSettingsState_MandatoryReauthDisabledInThirdPartyMode() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Mandatory reauth toggle is shown and disabled.
        assertTrue(getMandatoryReauthPreference(activity).isVisible());
        assertFalse(getMandatoryReauthPreference(activity).isEnabled());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN
    })
    public void testSettingsState_SaveCVCDisabledInThirdPartyMode() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Save security codes is shown and disabled.
        ChromeSwitchPreference saveCvcToggle =
                findPreferenceByKey(activity, AutofillPaymentMethodsFragment.PREF_SAVE_CVC);
        assertFalse(saveCvcToggle.isEnabled());
        assertFalse(saveCvcToggle.isChecked());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN
    })
    public void testSettingsState_CardBenefitsHiddenInThirdPartyMode() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Card benefits is hidden.
        Preference cardBenefitsPref =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_CARD_BENEFITS);
        assertNull(cardBenefitsPref);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN
    })
    public void testSettingsState_CardsListShownInThirdPartyMode() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_VISA);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // List of cards is shown.
        Preference cardPreference = getPreferenceScreen(activity).getPreference(4);
        assertEquals(
                1, getPreferenceCountWithKey(activity, AutofillPaymentMethodsFragment.PREF_CARD));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN
    })
    public void testSettingsState_IbanListShownInThirdPartyMode() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        mAutofillTestHelper.addServerIban(VALID_SERVER_IBAN);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // List of ibans is shown.
        Preference ibanPreference = getPreferenceScreen(activity).getPreference(4);
        assertEquals(
                1, getPreferenceCountWithKey(activity, AutofillPaymentMethodsFragment.PREF_IBAN));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN
    })
    public void testSettingsState_AddCardHiddenInThirdPartyMode() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Add card (Check out faster with autofill) is hidden.
        assertNull(
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_ADD_CARD));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN
    })
    public void testSettingsState_AddIbanButtonHiddenInthirdPartyMode() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        mAutofillTestHelper.addServerIban(VALID_SERVER_IBAN);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Add IBAN button is hidden.
        assertNull(
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_ADD_IBAN));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN
    })
    public void testSettingsState_PaymentAppsShownInThirdPartyMode() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });

        List<ResolveInfo> activities = new ArrayList<>();
        ResolveInfo alicePay = new ResolveInfo();
        alicePay.activityInfo = new ActivityInfo();
        alicePay.activityInfo.packageName = "com.alicepay.app";
        alicePay.activityInfo.name = "com.alicepay.app.WebPaymentActivity";
        activities.add(alicePay);
        when(mPackageManagerDelegate.getActivitiesThatCanRespondToIntent(any()))
                .thenReturn(activities);
        AndroidPaymentAppFactory.setPackageManagerDelegateForTest(mPackageManagerDelegate);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Payment apps is shown and enabled.
        Preference paymentAppsPref =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_PAYMENT_APPS);
        assertNotNull(paymentAppsPref);
        assertTrue(paymentAppsPref.isEnabled());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN
    })
    public void testSettingsState_LoyaltyCardsDisabledInThirdPartyMode() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Loyalty cards is shown and disabled.
        Preference loyaltyCards =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_LOYALTY_CARDS);
        assertNotNull(loyaltyCards);
        assertFalse(loyaltyCards.isEnabled());
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN,
        ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS,
        ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM
    })
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM})
    public void testSettingsState_hidePayWithPixAndEwalletPreferenceInThirdPartyMode()
            throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that both the preferences for 'Pay with Pix' and 'Pay with non-card payment
        // methods' are hidden.
        Preference pixFinancialAccountsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment.PREF_FINANCIAL_ACCOUNTS_MANAGEMENT);
        assertNull(pixFinancialAccountsPref);

        Preference nonCardPaymentMethodsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment
                                        .PREF_NON_CARD_PAYMENT_METHODS_MANAGEMENT);
        assertNull(nonCardPaymentMethodsPref);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN,
        ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS,
    })
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM})
    public void testSettingsState_hidePayWithEwalletPreferenceInThirdPartyMode() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preference for 'Pay with eWallet' is displayed.
        Preference otherFinancialAccountsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment.PREF_FINANCIAL_ACCOUNTS_MANAGEMENT);
        assertNull(otherFinancialAccountsPref);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN,
        ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM
    })
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM})
    public void testSettingsState_hidePayWithPixPreferenceInThirdPartyMode() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Financial Accounts Managementis shown and disabled.
        Preference otherFinancialAccountsPref =
                getPreferenceScreen(activity)
                        .findPreference(
                                AutofillPaymentMethodsFragment.PREF_FINANCIAL_ACCOUNTS_MANAGEMENT);
        assertNull(otherFinancialAccountsPref);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN
    })
    public void testDisabledSettingsText_shownInThirdPartyMode() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        assertNotNull(
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.DISABLED_SETTINGS_INFO));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN})
    public void testDisabledSettingsText_linksToAutofillOptionsPage() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillClientProviderUtils.setAutofillAvailabilityToUseForTesting(
                            AndroidAutofillAvailabilityStatus.AVAILABLE);
                });
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        CardWithButtonPreference disabled_settings_info_pref =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.DISABLED_SETTINGS_INFO);
        assertNotNull(disabled_settings_info_pref);
        onView(allOf(withId(R.id.icon), isDescendantOfA(withId(R.id.card_layout))))
                .check(matches(isDisplayed()));
        String title = disabled_settings_info_pref.getTitle().toString();
        assertThat(title)
                .isEqualTo(
                        activity.getString(R.string.autofill_disable_settings_explanation_title));
        String summary = disabled_settings_info_pref.getSummary().toString();
        assertThat(summary)
                .isEqualTo(activity.getString(R.string.autofill_disable_settings_explanation));

        onView(withId(R.id.card_button))
                .check(matches(withText(R.string.autofill_disable_settings_button_label)))
                .perform(scrollTo(), click());

        // Verify that the Autofill options fragment is opened.
        assertTrue(mAutofillTestRule.getLastestShownFragment() instanceof AutofillOptionsFragment);
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
        assertNotNull(preference);
        return preference;
    }

    private static PreferenceScreen getPreferenceScreen(SettingsActivity activity) {
        return ((AutofillPaymentMethodsFragment) activity.getMainFragment()).getPreferenceScreen();
    }

    private static PrefService getPrefService() {
        return UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
    }

    private static Preference getFirstPaymentMethodPreference(SettingsActivity activity) {
        boolean mandatoryReauthToggleShown = !DeviceInfo.isAutomotive();
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
        fail("Failed to find the card preference.");
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
