// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.notNull;
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
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.autofill.MandatoryReauthAuthenticationFlowEvent;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.concurrent.TimeoutException;

/** Instrumentation tests for AutofillPaymentMethodsFragment. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_PAYMENTS_MANDATORY_REAUTH})
public class AutofillPaymentMethodsFragmentTest {
    @Rule public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule public final AutofillTestRule rule = new AutofillTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Rule
    public final SettingsActivityTestRule<AutofillPaymentMethodsFragment>
            mSettingsActivityTestRule =
                    new SettingsActivityTestRule<>(AutofillPaymentMethodsFragment.class);

    @Mock private ReauthenticatorBridge mReauthenticatorMock;

    // Card Issuer values that map to the browser CreditCard.Issuer enum.
    private static final int CARD_ISSUER_UNKNOWN = 0;

    private static final CreditCard SAMPLE_CARD_VISA =
            new CreditCard(
                    /* guid= */ "",
                    /* origin= */ "",
                    /* isLocal= */ false,
                    /* isCached= */ false,
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
                    /* isCached= */ false,
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
                    /* isCached= */ false,
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
                    /* isCached= */ false,
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
                    /* cvc= */ "");
    private static final CreditCard SAMPLE_VIRTUAL_CARD_ENROLLED =
            new CreditCard(
                    /* guid= */ "",
                    /* origin= */ "",
                    /* isLocal= */ false,
                    /* isCached= */ false,
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
                    /* cvc= */ "");
    private static final CreditCard SAMPLE_CARD_WITH_CVC =
            new CreditCard(
                    /* guid= */ "",
                    /* origin= */ "",
                    /* isLocal= */ false,
                    /* isCached= */ false,
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
                    /* cvc= */ "123");

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

        Preference cardPreference = getPreferenceScreen(activity).getPreference(4);
        String summary = cardPreference.getSummary().toString();
        assertThat(summary)
                .contains(
                        String.format(
                                activity.getString(
                                        R.string.autofill_settings_page_summary_separated_by_pipe),
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

        Preference cardPreference = getPreferenceScreen(activity).getPreference(4);
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
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_PAYMENTS_MANDATORY_REAUTH})
    public void testMandatoryReauthToggle_notShownWhenFeatureDisabled() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preferences on the initial screen map are Save and Fill toggle + Add Card
        // button + Payment Apps.
        Assert.assertEquals(3, getPreferenceScreen(activity).getPreferenceCount());
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
        when(mReauthenticatorMock.canUseAuthenticationWithBiometricOrScreenLock())
                .thenReturn(false);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Assert.assertFalse(getMandatoryReauthPreference(activity).isEnabled());
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testMandatoryReauthToggle_disabledWithCorrespondingPrefValue() throws Exception {
        // Simulate the pref was enabled previously, to ensure the toggle value is set
        // correspondingly.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, true);
                });
        // Simulate the user can't authenticate with neither biometric nor screen lock.
        when(mReauthenticatorMock.canUseAuthenticationWithBiometricOrScreenLock())
                .thenReturn(false);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Assert.assertFalse(getMandatoryReauthPreference(activity).isEnabled());
        // Also verify that the Reauth toggle is disabled with the corresponding pref value (greyed
        // out whe pref = ON).
        Assert.assertTrue(getMandatoryReauthPreference(activity).isChecked());
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testMandatoryReauthToggle_displayToggle() throws Exception {
        // Simulate the pref was enabled previously, to ensure the toggle value is set
        // correspondingly.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, true);
                });
        // Simulate the user can authenticate with biometric or screen lock.
        when(mReauthenticatorMock.canUseAuthenticationWithBiometricOrScreenLock()).thenReturn(true);

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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, false);
                });
        // Simulate the user can authenticate with biometric or screen lock.
        when(mReauthenticatorMock.canUseAuthenticationWithBiometricOrScreenLock()).thenReturn(true);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the Reauth preference is not checked, since Reauth pref is disabled.
        Assert.assertFalse(getMandatoryReauthPreference(activity).isChecked());

        // Simulate the biometric authentication will succeed.
        setUpBiometricAuthenticationResult(/* success= */ true);
        // Simulate click on the Reauth toggle, trying to toggle on. Now Chrome is waiting for OS
        // authentication which should succeed.
        TestThreadUtils.runOnUiThreadBlocking(getMandatoryReauthPreference(activity)::performClick);

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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, true);
                });
        // Simulate the user can authenticate with biometric or screen lock.
        when(mReauthenticatorMock.canUseAuthenticationWithBiometricOrScreenLock()).thenReturn(true);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the Reauth preference is checked.
        Assert.assertTrue(getMandatoryReauthPreference(activity).isChecked());

        // Simulate the biometric authentication will fail.
        setUpBiometricAuthenticationResult(/* success= */ false);
        // Simulate click on the Reauth toggle, trying to toggle off. Now Chrome is waiting for OS
        // authentication which should fail.
        TestThreadUtils.runOnUiThreadBlocking(getMandatoryReauthPreference(activity)::performClick);

        verify(mReauthenticatorMock).reauthenticate(notNull());
        // Verify that the Reauth toggle is still checked since authentication failed.
        Assert.assertTrue(getMandatoryReauthPreference(activity).isChecked());
        optOutHistogram.assertExpected();
    }

    // TODO(crbug/1470259): Tests for various FIDO toggle scenarios needs to be added here.
    @Test
    @MediumTest
    public void testMandatoryReauthToggle_FidoToggleHiddenIfReauthFlagIsEnabled() throws Exception {
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, true);
                });
        // Simulate the user can authenticate with biometric or screen lock.
        when(mReauthenticatorMock.canUseAuthenticationWithBiometricOrScreenLock()).thenReturn(true);
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
        TestThreadUtils.runOnUiThreadBlocking(cardPreference::performClick);
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, true);
                });
        // Simulate the user can authenticate with biometric or screen lock.
        when(mReauthenticatorMock.canUseAuthenticationWithBiometricOrScreenLock()).thenReturn(true);
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
        TestThreadUtils.runOnUiThreadBlocking(cardPreference::performClick);

        verify(mReauthenticatorMock).reauthenticate(notNull());
        // Verify that the local card edit dialog was NOT shown.
        Assert.assertNull(rule.getLastestShownFragment());
        editCardReauthHistogram.assertExpected();
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_PAYMENTS_MANDATORY_REAUTH})
    public void testLocalCardEditWithReauth_noReauthWhenFeatureDisabled() throws Exception {
        mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        // Simulate Reauth pref is enabled when feature is disabled though this unlikely occurs in
        // real world. We won't require reauth for this case (e.g. we rollback the feature rollout).
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, true);
                });

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Get the local card widget.
        Preference cardPreference = getFirstPaymentMethodPreference(activity);
        String title = cardPreference.getTitle().toString();
        assertThat(title).contains("Visa");
        assertThat(title).contains("1111");

        // Simulate click on the local card widget. Now Chrome is waiting for OS authentication.
        TestThreadUtils.runOnUiThreadBlocking(cardPreference::performClick);
        // Since reauth feature is disabled, we will directly open local card dialog. Wait for the
        // new dialog to be rendered.
        rule.waitForFragmentToBeShown();

        verify(mReauthenticatorMock, never()).reauthenticate(notNull());
        // Verify that the local card edit dialog was shown.
        Assert.assertTrue(rule.getLastestShownFragment() instanceof AutofillLocalCardEditor);
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_AUTO)
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_PAYMENTS_MANDATORY_REAUTH})
    public void testLocalCardEditWithReauth_reauthOnClickedOnAutoWhenFeatureDisabled()
            throws Exception {
        mAutofillTestHelper.setCreditCard(SAMPLE_LOCAL_CARD);
        // Simulate Reauth pref is enabled.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, true);
                });
        // Simulate the user can authenticate with biometric or screen lock.
        when(mReauthenticatorMock.canUseAuthenticationWithBiometricOrScreenLock()).thenReturn(true);
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
        TestThreadUtils.runOnUiThreadBlocking(cardPreference::performClick);
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, false);
                });
        // Simulate the user can authenticate with biometric or screen lock.
        when(mReauthenticatorMock.canUseAuthenticationWithBiometricOrScreenLock()).thenReturn(true);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the Reauth preference is not checked.
        Assert.assertFalse(getMandatoryReauthPreference(activity).isChecked());
        Preference cardPreference = getFirstPaymentMethodPreference(activity);
        String title = cardPreference.getTitle().toString();
        assertThat(title).contains("Visa");
        assertThat(title).contains("1111");

        // Simulate click on the local card widget.
        TestThreadUtils.runOnUiThreadBlocking(cardPreference::performClick);
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, false);
                });
        // Simulate the user can authenticate with biometric or screen lock.
        when(mReauthenticatorMock.canUseAuthenticationWithBiometricOrScreenLock()).thenReturn(true);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Simulate the biometric authentication will succeed.
        setUpBiometricAuthenticationResult(/* success= */ true);
        // Simulate click on the Reauth toggle, trying to toggle on. Now Chrome is waiting for OS
        // authentication which should succeed.
        TestThreadUtils.runOnUiThreadBlocking(getMandatoryReauthPreference(activity)::performClick);

        // Get the local card's widget.
        Preference cardPreference = getFirstPaymentMethodPreference(activity);
        String title = cardPreference.getTitle().toString();
        assertThat(title).contains("Visa");
        assertThat(title).contains("1111");

        // Simulate click on the local card widget. Now Chrome is waiting for OS authentication.
        TestThreadUtils.runOnUiThreadBlocking(cardPreference::performClick);
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService()
                            .setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, true);
                });
        // Simulate the user can authenticate with biometric or screen lock.
        when(mReauthenticatorMock.canUseAuthenticationWithBiometricOrScreenLock()).thenReturn(true);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Simulate the biometric authentication will succeed.
        setUpBiometricAuthenticationResult(/* success= */ true);
        // Simulate click on the Reauth toggle, trying to toggle off. Now Chrome is waiting for OS
        // authentication which should succeed.
        TestThreadUtils.runOnUiThreadBlocking(getMandatoryReauthPreference(activity)::performClick);

        // Get the local card's widget.
        Preference cardPreference = getFirstPaymentMethodPreference(activity);
        String title = cardPreference.getTitle().toString();
        assertThat(title).contains("Visa");
        assertThat(title).contains("1111");

        // Simulate click on the local card widget.
        TestThreadUtils.runOnUiThreadBlocking(cardPreference::performClick);
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
    @Features.DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testSaveCvcToggle_notShownWhenFeatureDisabled() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

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
        TestThreadUtils.runOnUiThreadBlocking(
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
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testSaveCvcToggle_shown() throws Exception {
        // Initial state, Save Cvc pref is enabled previously.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.AUTOFILL_PAYMENT_CVC_STORAGE, true);
                });

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that save cvc toggle is shown but greyed out when Autofill toggle is disabled.
        ChromeSwitchPreference saveCvcToggle =
                findPreferenceByKey(activity, AutofillPaymentMethodsFragment.PREF_SAVE_CVC);
        Assert.assertTrue(saveCvcToggle.isEnabled());
        Assert.assertTrue(saveCvcToggle.isChecked());
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testDeleteSavedCvcsButton_notShownWhenFeatureDisabled() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference expectedNullDeleteSavedCvcsToggle =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_DELETE_SAVED_CVCS);
        Assert.assertNull(expectedNullDeleteSavedCvcsToggle);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE})
    public void testDeleteSavedCvcsButton_shown() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference deleteSavedCvcsToggle =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_DELETE_SAVED_CVCS);
        Assert.assertNotNull(deleteSavedCvcsToggle);
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
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }

    private static Preference getFirstPaymentMethodPreference(SettingsActivity activity) {
        boolean mandatoryReauthToggleShown =
                ChromeFeatureList.isEnabled(
                                ChromeFeatureList.AUTOFILL_ENABLE_PAYMENTS_MANDATORY_REAUTH)
                        && !BuildInfo.getInstance().isAutomotive;
        if (mandatoryReauthToggleShown) {
            return getPreferenceScreen(activity).getPreference(2);
        } else {
            return getPreferenceScreen(activity).getPreference(1);
        }
    }
}
