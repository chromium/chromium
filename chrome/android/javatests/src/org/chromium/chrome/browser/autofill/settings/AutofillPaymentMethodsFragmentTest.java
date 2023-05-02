// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.doAnswer;
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

import org.chromium.base.Callback;
import org.chromium.base.test.util.Batch;
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
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for AutofillPaymentMethodsFragment.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class AutofillPaymentMethodsFragmentTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public final AutofillTestRule rule = new AutofillTestRule();
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Rule
    public final SettingsActivityTestRule<AutofillPaymentMethodsFragment>
            mSettingsActivityTestRule =
                    new SettingsActivityTestRule<>(AutofillPaymentMethodsFragment.class);

    @Mock
    private ReauthenticatorBridge mReauthenticatorMock;

    // Card Issuer values that map to the browser CreditCard.Issuer enum.
    private static final int CARD_ISSUER_UNKNOWN = 0;
    private static final int CARD_ISSUER_GOOGLE = 1;

    private static final CreditCard SAMPLE_CARD_VISA = new CreditCard(/* guid= */ "",
            /* origin= */ "",
            /* isLocal= */ false, /* isCached= */ false, /* name= */ "John Doe",
            /* number= */ "4444333322221111",
            /* obfuscatedNumber= */ "", /* month= */ "5", AutofillTestHelper.nextYear(),
            /* basicCardIssuerNetwork =*/"visa",
            /* issuerIconDrawableId= */ 0, /* billingAddressId= */ "",
            /* serverId= */ "");
    private static final CreditCard SAMPLE_CARD_MASTERCARD =
            new CreditCard(/* guid= */ "", /* origin= */ "",
                    /* isLocal= */ false, /* isCached= */ false, /* name= */ "John Doe",
                    /* number= */ "5454545454545454",
                    /* obfuscatedNumber= */ "", /* month= */ "12", AutofillTestHelper.nextYear(),
                    /* basicCardIssuerNetwork= */ "mastercard", /* issuerIconDrawableId= */ 0,
                    /* billingAddressId= */ "",
                    /* serverId= */ "");
    private static final CreditCard SAMPLE_VIRTUAL_CARD_UNENROLLED = new CreditCard(/* guid= */ "",
            /* origin= */ "",
            /* isLocal= */ false, /* isCached= */ false, /* isVirtual= */ false,
            /* name= */ "John Doe",
            /* number= */ "4444333322221111",
            /* obfuscatedNumber= */ "", /* month= */ "5", AutofillTestHelper.nextYear(),
            /* basicCardIssuerNetwork =*/"visa",
            /* issuerIconDrawableId= */ 0, /* billingAddressId= */ "",
            /* serverId= */ "", /* instrumentId= */ 0, /* cardLabel= */ "", /* nickname= */ "",
            /* cardArtUrl= */ null,
            /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState.UNENROLLED_AND_ELIGIBLE,
            /* productDescription= */ "", /* cardNameForAutofillDisplay= */ "",
            /* obfuscatedLastFourDigits= */ "");
    private static final CreditCard SAMPLE_VIRTUAL_CARD_ENROLLED = new CreditCard(/* guid= */ "",
            /* origin= */ "",
            /* isLocal= */ false, /* isCached= */ false, /* isVirtual= */ false,
            /* name= */ "John Doe",
            /* number= */ "4444333322221111",
            /* obfuscatedNumber= */ "", /* month= */ "5", AutofillTestHelper.nextYear(),
            /* basicCardIssuerNetwork =*/"visa",
            /* issuerIconDrawableId= */ 0, /* billingAddressId= */ "",
            /* serverId= */ "", /* instrumentId= */ 0, /* cardLabel= */ "", /* nickname= */ "",
            /* cardArtUrl= */ null,
            /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState.ENROLLED,
            /* productDescription= */ "", /* cardNameForAutofillDisplay= */ "",
            /* obfuscatedLastFourDigits= */ "");

    private AutofillTestHelper mAutofillTestHelper;

    @Before
    public void setUp() {
        mAutofillTestHelper = new AutofillTestHelper();
        ReauthenticatorBridge.setInstanceForTesting(mReauthenticatorMock);
    }

    @After
    public void tearDown() throws TimeoutException {
        mAutofillTestHelper.clearAllDataForTesting();
        ReauthenticatorBridge.setInstanceForTesting(null);
    }

    @Test
    @MediumTest
    public void testTwoCreditCards_displaysTwoServerCards() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_VISA);
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_MASTERCARD);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getPreferenceScreen(activity).getPreference(1);
        // Verify that the preferences on the initial screen map to Save and Fill toggle + 2 Cards +
        // Add Card button + Payment Apps.
        Assert.assertEquals(5, getPreferenceScreen(activity).getPreferenceCount());
    }

    @Test
    @MediumTest
    public void testCreditCardWithoutNickname_displayNetworkAndLastFourAsTitle() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_VISA);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getPreferenceScreen(activity).getPreference(1);
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

        Preference cardPreference = getPreferenceScreen(activity).getPreference(1);
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

        Preference cardPreference = getPreferenceScreen(activity).getPreference(1);
        String title = cardPreference.getTitle().toString();
        assertThat(title).contains("This is a long nickname");
        assertThat(title).contains("1111");
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_VIRTUAL_CARD_METADATA})
    public void testCreditCardSummary_displaysVirtualCardEnrolledStatus() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getPreferenceScreen(activity).getPreference(1);
        String summary = cardPreference.getSummary().toString();
        assertThat(summary).isEqualTo(
                activity.getString(R.string.autofill_virtual_card_enrolled_text));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_VIRTUAL_CARD_METADATA})
    public void testCreditCardSummary_displaysExpirationDateForUnenrolledCards() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_UNENROLLED);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getPreferenceScreen(activity).getPreference(1);
        String summary = cardPreference.getSummary().toString();
        // Verify that the summary (line below the card name and number) contains the expiration
        // date.
        assertThat(summary).contains(String.format("05/%s", AutofillTestHelper.twoDigitNextYear()));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_VIRTUAL_CARD_METADATA})
    public void testCreditCardSummary_displaysExpirationDateForNonVirtualCards() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_VISA);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getPreferenceScreen(activity).getPreference(1);
        String summary = cardPreference.getSummary().toString();
        assertThat(summary).contains(String.format("05/%s", AutofillTestHelper.twoDigitNextYear()));
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_VIRTUAL_CARD_METADATA})
    public void testCreditCardSummary_displaysExpirationDateForVirtualCardsWhenMetadataFlagOff()
            throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getPreferenceScreen(activity).getPreference(1);
        String summary = cardPreference.getSummary().toString();
        assertThat(summary).contains(String.format("05/%s", AutofillTestHelper.twoDigitNextYear()));
    }

    @Test
    @SmallTest
    @Policies.Add({ @Policies.Item(key = "AutofillCreditCardEnabled", string = "false") })
    public void testAutofillToggleDisabledByPolicy() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        ChromeSwitchPreference autofillTogglePreference =
                (ChromeSwitchPreference) getPreferenceScreen(activity).getPreference(0);
        Assert.assertFalse(autofillTogglePreference.isEnabled());
    }

    @Test
    @SmallTest
    @Policies.Add({ @Policies.Item(key = "AutofillCreditCardEnabled", string = "true") })
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
    @Features.DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_PAYMENTS_MANDATORY_REAUTH})
    public void testMandatoryReauthToggle_notShownWhenFeatureDisabled() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preferences on the initial screen map are Save and Fill toggle + Add Card
        // button + Payment Apps.
        Assert.assertEquals(3, getPreferenceScreen(activity).getPreferenceCount());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_PAYMENTS_MANDATORY_REAUTH})
    // Use the policy to simulate AutofillCreditCard is disabled.
    @Policies.Add({ @Policies.Item(key = "AutofillCreditCardEnabled", string = "false") })
    public void testMandatoryReauthToggle_notShownWhenAutofillDisabled() throws Exception {
        // Simulate the user can authenticate with biometric.
        when(mReauthenticatorMock.canUseAuthentication()).thenReturn(true);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that Reauth toggle is not shown when Autofill toggle is disabled. The preferences
        // on the initial screen map are Save and Fill toggle + Payment Apps (No add card button
        // when Autofill is disabled).
        Assert.assertEquals(2, getPreferenceScreen(activity).getPreferenceCount());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_PAYMENTS_MANDATORY_REAUTH})
    public void testMandatoryReauthToggle_notShownWhenBiometricIsDisabled() throws Exception {
        // Simulate the user can't authenticate with biometric.
        when(mReauthenticatorMock.canUseAuthentication()).thenReturn(false);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preferences on the initial screen map are Save and Fill toggle + Add Card
        // button + Payment Apps.
        Assert.assertEquals(3, getPreferenceScreen(activity).getPreferenceCount());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_PAYMENTS_MANDATORY_REAUTH})
    public void testMandatoryReauthToggle_displayToggle() throws Exception {
        // Simulate the pref was enabled previously, to ensure the toggle value is set
        // correspondingly.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getPrefService().setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, true);
        });
        // Simulate the user can authenticate with biometric, so that Reauth toggle can be shown.
        when(mReauthenticatorMock.canUseAuthentication()).thenReturn(true);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preference on the initial screen map is only Save and Fill toggle +
        // Mandatory Reauth toggle + Add Card button + Payment Apps.
        Assert.assertEquals(4, getPreferenceScreen(activity).getPreferenceCount());
        ChromeSwitchPreference mandatoryReauthPreference =
                (ChromeSwitchPreference) getPreferenceScreen(activity).getPreference(1);
        Assert.assertEquals(mandatoryReauthPreference.getTitle(),
                activity.getString(
                        R.string.autofill_settings_page_enable_payment_method_mandatory_reauth_label));
        Assert.assertTrue(mandatoryReauthPreference.isChecked());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_PAYMENTS_MANDATORY_REAUTH})
    public void testMandatoryReauthToggle_switchValueOnClicked() throws Exception {
        // Initial state, Reauth pref is disabled by default.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getPrefService().setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, false);
        });
        // Simulate the user can authenticate with biometric, so that Reauth toggle can be shown.
        when(mReauthenticatorMock.canUseAuthentication()).thenReturn(true);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the Reauth preference is not checked, since Reauth pref is disabled.
        Assert.assertFalse(getMandatoryReauthPreference(activity).isChecked());

        // Simulate the biometric authentication will succeed.
        setUpBiometricAuthenticationResult(/*success=*/true);
        // Simulate click on the Reauth toggle, trying to toggle on. Now Chrome is waiting for OS
        // authentication.
        TestThreadUtils.runOnUiThreadBlocking(getMandatoryReauthPreference(activity)::performClick);
        // Now call onResume to simulate bringing the settings page back to foreground, which will
        // rebuild the fragment.
        TestThreadUtils.runOnUiThreadBlocking(activity.getMainFragment()::onResume);

        verify(mReauthenticatorMock).reauthenticate(notNull(), /*useLastValidReauth=*/eq(false));
        // Verify that the refreshed Reauth toggle is now checked.
        Assert.assertTrue(getMandatoryReauthPreference(activity).isChecked());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_PAYMENTS_MANDATORY_REAUTH})
    public void testMandatoryReauthToggle_stayAtOldValueIfBiometricAuthFails() throws Exception {
        // Simulate Reauth pref is enabled previously.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getPrefService().setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, true);
        });
        // Simulate the user can authenticate with biometric, so that Reauth toggle can be shown.
        when(mReauthenticatorMock.canUseAuthentication()).thenReturn(true);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the Reauth preference is checked.
        Assert.assertTrue(getMandatoryReauthPreference(activity).isChecked());

        // Simulate the biometric authentication will fail.
        setUpBiometricAuthenticationResult(/*success=*/false);
        // Simulate click on the Reauth toggle, trying to toggle off. Now Chrome is waiting for OS
        // authentication.
        TestThreadUtils.runOnUiThreadBlocking(getMandatoryReauthPreference(activity)::performClick);
        // Now call onResume to simulate bringing the settings page back to foreground, which will
        // rebuild the fragment.
        TestThreadUtils.runOnUiThreadBlocking(activity.getMainFragment()::onResume);

        verify(mReauthenticatorMock).reauthenticate(notNull(), /*useLastValidReauth=*/eq(false));
        // Verify that the refreshed Reauth toggle is still checked since authentication failed.
        Assert.assertTrue(getMandatoryReauthPreference(activity).isChecked());
    }

    private void setUpBiometricAuthenticationResult(boolean success) {
        // We have to manually invoke the passed-in callback.
        doAnswer(invocation -> {
            Callback<Boolean> callback = invocation.getArgument(0);
            callback.onResult(success);
            return true;
        })
                .when(mReauthenticatorMock)
                .reauthenticate(notNull(), /*useLastValidReauth=*/eq(false));
    }

    private ChromeSwitchPreference getMandatoryReauthPreference(SettingsActivity activity) {
        return findPreferenceByKey(activity, AutofillPaymentMethodsFragment.PREF_MANDATORY_REAUTH);
    }

    /**Find preference by the provided key, fail if no matched preference is found.*/
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
}
