// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.settings.GoogleServicesSettings;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for GoogleServicesSettings.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class GoogleServicesSettingsTest {
    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    public final SettingsActivityTestRule<GoogleServicesSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(GoogleServicesSettings.class);

    // SettingsActivity has to be finished before the outer CTA can be finished or trying to finish
    // CTA won't work.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mActivityTestRule).around(mSettingsActivityTestRule);

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertTrue("SIGNIN_ALLOWED pref should be set by default",
                                UserPrefs.get(Profile.getLastUsedRegularProfile())
                                        .getBoolean(Pref.SIGNIN_ALLOWED)));
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> UserPrefs.get(Profile.getLastUsedRegularProfile())
                                   .setBoolean(Pref.SIGNIN_ALLOWED, true));
    }

    @Test
    @LargeTest
    @Features.EnableFeatures(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
    public void signOutUserWithoutShowingSignOutDialog() {
        mAccountManagerTestRule.addTestAccountThenSignin();
        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();
        ChromeSwitchPreference allowChromeSignin =
                (ChromeSwitchPreference) googleServicesSettings.findPreference(
                        GoogleServicesSettings.PREF_ALLOW_SIGNIN);
        Assert.assertTrue("Chrome Signin should be allowed", allowChromeSignin.isChecked());

        onView(withText(R.string.allow_chrome_signin_title)).perform(click());
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertNull("Account should be signed out!",
                                IdentityServicesProvider.get()
                                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN)));
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertFalse("SIGNIN_ALLOWED pref should be unset",
                                UserPrefs.get(Profile.getLastUsedRegularProfile())
                                        .getBoolean(Pref.SIGNIN_ALLOWED)));
        Assert.assertFalse("Chrome Signin should not be allowed", allowChromeSignin.isChecked());
    }

    @Test
    @LargeTest
    @Features.EnableFeatures(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
    public void showSignOutDialogBeforeSigningUserOut() {
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();
        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();
        ChromeSwitchPreference allowChromeSignin =
                (ChromeSwitchPreference) googleServicesSettings.findPreference(
                        GoogleServicesSettings.PREF_ALLOW_SIGNIN);
        Assert.assertTrue("Chrome Signin should be allowed", allowChromeSignin.isChecked());

        onView(withText(R.string.allow_chrome_signin_title)).perform(click());
        // Accept the sign out Dialog
        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertFalse(
                                "Accepting the sign-out dialog should set SIGNIN_ALLOWED to false",
                                UserPrefs.get(Profile.getLastUsedRegularProfile())
                                        .getBoolean(Pref.SIGNIN_ALLOWED)));
        Assert.assertFalse("Chrome Signin should not be allowed", allowChromeSignin.isChecked());
    }

    /**
     * Test: if the onboarding was never shown, the AA chrome preference should not exist.
     *
     * Note:
     * - Presence of the {@link GoogleServicesSettings.PREF_AUTOFILL_ASSISTANT}
     *   shared preference indicates whether onboarding was shown or not.
     * - There's a separate settings screen added if either AUTOFILL_ASSISTANT_PROACTIVE_HELP or
     *   OMNIBOX_ASSISTANT_VOICE_SEARCH is enabled.
     */
    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT)
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP,
            ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH})
    public void
    testAutofillAssistantNoPreferenceIfOnboardingNeverShown() {
        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull(googleServicesSettings.findPreference(
                    GoogleServicesSettings.PREF_AUTOFILL_ASSISTANT));
        });
    }

    /**
     * Test: if the onboarding was shown at least once, the AA chrome preference should also exist.
     *
     * Note:
     * - Presence of the {@link GoogleServicesSettings.PREF_AUTOFILL_ASSISTANT}
     *   shared preference indicates whether onboarding was shown or not.
     * - There's a separate settings screen added if either AUTOFILL_ASSISTANT_PROACTIVE_HELP or
     *   OMNIBOX_ASSISTANT_VOICE_SEARCH is enabled.
     */
    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT)
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP,
            ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH})
    public void
    testAutofillAssistantPreferenceShownIfOnboardingShown() {
        setAutofillAssistantSwitchValue(true);
        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNotNull(googleServicesSettings.findPreference(
                    GoogleServicesSettings.PREF_AUTOFILL_ASSISTANT));
        });
    }

    /**
     * Ensure that the "Autofill Assistant" setting is not shown when the feature is disabled.
     */
    @Test
    @LargeTest
    @Feature({"Sync"})
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT,
            ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP,
            ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH})
    public void
    testAutofillAssistantNoPreferenceIfFeatureDisabled() {
        setAutofillAssistantSwitchValue(true);
        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull(googleServicesSettings.findPreference(
                    GoogleServicesSettings.PREF_AUTOFILL_ASSISTANT));
        });
    }

    /**
     * Ensure that the "Autofill Assistant" on/off switch works.
     */
    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT)
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP,
            ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH})
    public void
    testAutofillAssistantSwitchOn() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { setAutofillAssistantSwitchValue(true); });
        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeSwitchPreference autofillAssistantSwitch =
                    (ChromeSwitchPreference) googleServicesSettings.findPreference(
                            GoogleServicesSettings.PREF_AUTOFILL_ASSISTANT);
            Assert.assertTrue(autofillAssistantSwitch.isChecked());

            autofillAssistantSwitch.performClick();
            Assert.assertFalse(googleServicesSettings.isAutofillAssistantSwitchOn());
            autofillAssistantSwitch.performClick();
            Assert.assertTrue(googleServicesSettings.isAutofillAssistantSwitchOn());
        });
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT)
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP,
            ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH})
    public void
    testAutofillAssistantSwitchOff() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { setAutofillAssistantSwitchValue(false); });
        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeSwitchPreference autofillAssistantSwitch =
                    (ChromeSwitchPreference) googleServicesSettings.findPreference(
                            GoogleServicesSettings.PREF_AUTOFILL_ASSISTANT);
            Assert.assertFalse(autofillAssistantSwitch.isChecked());
        });
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)
    @DisableFeatures(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)
    public void testAutofillAssistantProactiveHelp() {
        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull(googleServicesSettings.findPreference(
                    GoogleServicesSettings.PREF_AUTOFILL_ASSISTANT));

            Assert.assertTrue(
                    googleServicesSettings
                            .findPreference(
                                    GoogleServicesSettings.PREF_AUTOFILL_ASSISTANT_SUBSECTION)
                            .isVisible());
        });
    }

    @Test
    @LargeTest
    @Feature({"AssistantVoiceSearch"})
    @EnableFeatures(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)
    @DisableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)
    public void testAutofillAssistantSubsection_AssistantVoiceSeach() {
        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull(googleServicesSettings.findPreference(
                    GoogleServicesSettings.PREF_AUTOFILL_ASSISTANT));

            Assert.assertTrue(
                    googleServicesSettings
                            .findPreference(
                                    GoogleServicesSettings.PREF_AUTOFILL_ASSISTANT_SUBSECTION)
                            .isVisible());
        });
    }

    @Test
    @LargeTest
    @Feature({"Preference"})
    @DisableFeatures(ChromeFeatureList.METRICS_SETTINGS_ANDROID)
    public void testMetricsSettingsHiddenFlagOff() {
        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull("Metrics settings should be null when the flag is off.",
                    googleServicesSettings.findPreference(
                            GoogleServicesSettings.PREF_METRICS_SETTINGS));
        });
    }

    @Test
    @LargeTest
    @Feature({"Preference"})
    @EnableFeatures(ChromeFeatureList.METRICS_SETTINGS_ANDROID)
    public void testMetricsSettingsShownFlagOn() {
        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNotNull("Metrics settings should exist when the flag is on.",
                    googleServicesSettings.findPreference(
                            GoogleServicesSettings.PREF_METRICS_SETTINGS));
        });
    }

    private void setAutofillAssistantSwitchValue(boolean newValue) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED, newValue);
    }

    private GoogleServicesSettings startGoogleServicesSettings() {
        mSettingsActivityTestRule.startSettingsActivity();
        return mSettingsActivityTestRule.getFragment();
    }
}
