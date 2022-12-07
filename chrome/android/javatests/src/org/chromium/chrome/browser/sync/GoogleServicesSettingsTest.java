// Copyright 2020 The Chromium Authors
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
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.settings.GoogleServicesSettings;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for GoogleServicesSettings.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class GoogleServicesSettingsTest {
    private static final String CHILD_ACCOUNT_NAME =
            AccountManagerTestRule.generateChildEmail("account@gmail.com");

    @Rule
    public final SigninTestRule mSigninTestRule = new SigninTestRule();

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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
            prefService.clearPref(Pref.SIGNIN_ALLOWED);
        });
    }

    @Test
    @LargeTest
    public void allowSigninOptionHiddenFromChildUser() {
        mSigninTestRule.addAccountAndWaitForSeeding(CHILD_ACCOUNT_NAME);
        final Profile profile = TestThreadUtils.runOnUiThreadBlockingNoException(
                Profile::getLastUsedRegularProfile);
        CriteriaHelper.pollUiThread(profile::isChild);

        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();
        ChromeSwitchPreference allowChromeSignin =
                (ChromeSwitchPreference) googleServicesSettings.findPreference(
                        GoogleServicesSettings.PREF_ALLOW_SIGNIN);
        Assert.assertFalse(
                "Chrome Signin option should not be visible", allowChromeSignin.isVisible());
    }

    @Test
    @LargeTest
    public void signOutUserWithoutShowingSignOutDialog() {
        mSigninTestRule.addTestAccountThenSignin();
        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();
        ChromeSwitchPreference allowChromeSignin =
                (ChromeSwitchPreference) googleServicesSettings.findPreference(
                        GoogleServicesSettings.PREF_ALLOW_SIGNIN);
        Assert.assertTrue("Chrome Signin should be allowed", allowChromeSignin.isChecked());

        onView(withText(R.string.allow_chrome_signin_title)).perform(click());
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertFalse("Account should be signed out!",
                                IdentityServicesProvider.get()
                                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                                        .hasPrimaryAccount(ConsentLevel.SIGNIN)));
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertFalse("SIGNIN_ALLOWED pref should be unset",
                                UserPrefs.get(Profile.getLastUsedRegularProfile())
                                        .getBoolean(Pref.SIGNIN_ALLOWED)));
        Assert.assertFalse("Chrome Signin should not be allowed", allowChromeSignin.isChecked());
    }

    @Test
    @LargeTest
    public void showSignOutDialogBeforeSigningUserOut() {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
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

    @Test
    @LargeTest
    @Feature({"AssistantVoiceSearch"})
    @EnableFeatures(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)
    public void testAutofillAssistantSubsection_AssistantVoiceSeach() {
        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
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
    @EnableFeatures({ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH,
            ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH})
    public void
    testAutofillAssistantSubsection_NonPersonalizedAssistant() {
        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(
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

    @Test
    @LargeTest
    @Feature({"Preference"})
    @EnableFeatures({ChromeFeatureList.COMMERCE_PRICE_TRACKING + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_price_tracking/true"
                    + "/allow_disable_price_annotations/true"})
    public void
    testPriceTrackingAnnotations() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true));

        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeSwitchPreference priceAnnotationsSwitch =
                    (ChromeSwitchPreference) googleServicesSettings.findPreference(
                            GoogleServicesSettings.PREF_PRICE_TRACKING_ANNOTATIONS);
            Assert.assertTrue(priceAnnotationsSwitch.isVisible());
            Assert.assertTrue(priceAnnotationsSwitch.isChecked());

            priceAnnotationsSwitch.performClick();
            Assert.assertFalse(PriceTrackingUtilities.isTrackPricesOnTabsEnabled());
            priceAnnotationsSwitch.performClick();
            Assert.assertTrue(PriceTrackingUtilities.isTrackPricesOnTabsEnabled());
        });
    }

    @Test
    @LargeTest
    @Feature({"Preference"})
    @EnableFeatures({ChromeFeatureList.COMMERCE_PRICE_TRACKING + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_price_tracking/true"
                    + "/allow_disable_price_annotations/false"})
    public void
    testPriceTrackingAnnotations_FeatureDisabled() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true));

        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull(googleServicesSettings.findPreference(
                    GoogleServicesSettings.PREF_PRICE_TRACKING_ANNOTATIONS));
        });
    }

    @Test
    @LargeTest
    @Feature({"Preference"})
    @EnableFeatures({ChromeFeatureList.COMMERCE_PRICE_TRACKING + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_price_tracking/true"
                    + "/allow_disable_price_annotations/true"})
    public void
    testPriceTrackingAnnotations_NotSignedIn() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(false));

        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNull(googleServicesSettings.findPreference(
                    GoogleServicesSettings.PREF_PRICE_TRACKING_ANNOTATIONS));
        });
    }

    private GoogleServicesSettings startGoogleServicesSettings() {
        mSettingsActivityTestRule.startSettingsActivity();
        return mSettingsActivityTestRule.getFragment();
    }
}
