// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import androidx.annotation.Nullable;
import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.settings.GoogleServicesSettings;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for ManageSyncSettings.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class GoogleServicesSettingsTest {
    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final SettingsActivityTestRule<GoogleServicesSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(GoogleServicesSettings.class, true);

    private @Nullable ChromeSwitchPreference mAllowChromeSignin;

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
        startGoogleServicesSettings();

        onView(withText(R.string.offer_chrome_signin_title)).perform(click());
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertNull("Account should be signed out!",
                                IdentityServicesProvider.get()
                                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                                        .getPrimaryAccountInfo(ConsentLevel.NOT_REQUIRED)));
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertFalse("SIGNIN_ALLOWED pref should be unset",
                                UserPrefs.get(Profile.getLastUsedRegularProfile())
                                        .getBoolean(Pref.SIGNIN_ALLOWED)));
        Assert.assertFalse("Chrome Signin should not be allowed", mAllowChromeSignin.isChecked());
    }

    @Test
    @LargeTest
    @Features.EnableFeatures(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
    public void showSignOutDialogBeforeSigningUserOut() {
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();
        startGoogleServicesSettings();

        onView(withText(R.string.offer_chrome_signin_title)).perform(click());
        // Accept the sign out Dialog
        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertFalse(
                                "Accepting the sign-out dialog should set SIGNIN_ALLOWED to false",
                                UserPrefs.get(Profile.getLastUsedRegularProfile())
                                        .getBoolean(Pref.SIGNIN_ALLOWED)));
        Assert.assertFalse("Chrome Signin should not be allowed", mAllowChromeSignin.isChecked());
    }

    private void startGoogleServicesSettings() {
        mSettingsActivityTestRule.startSettingsActivity();
        GoogleServicesSettings googleServicesSettings = mSettingsActivityTestRule.getFragment();
        mAllowChromeSignin = (ChromeSwitchPreference) googleServicesSettings.findPreference(
                GoogleServicesSettings.PREF_ALLOW_SIGNIN);
        Assert.assertTrue("Chrome Signin should be allowed", mAllowChromeSignin.isChecked());
    }
}