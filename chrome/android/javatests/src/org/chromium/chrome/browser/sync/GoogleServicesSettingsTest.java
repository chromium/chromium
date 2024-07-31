// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.os.Build;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.password_manager.account_storage_toggle.AccountStorageToggleFragmentArgs;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.settings.GoogleServicesSettings;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.test.util.AccountCapabilitiesBuilder;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;

/** Tests for GoogleServicesSettings. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "A subset of tests requires adding a new account that could fail if batched.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class GoogleServicesSettingsTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public final JniMocker mJniMocker = new JniMocker();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    public final SettingsActivityTestRule<GoogleServicesSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(GoogleServicesSettings.class);

    // SettingsActivity has to be finished before the outer CTA can be finished or trying to finish
    // CTA won't work.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mActivityTestRule).around(mSettingsActivityTestRule);

    @Mock private PasswordManagerUtilBridge.Natives mMockPasswordManagerUtilBridgeJni;

    @Before
    public void setUp() {
        mJniMocker.mock(PasswordManagerUtilBridgeJni.TEST_HOOKS, mMockPasswordManagerUtilBridgeJni);
        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertTrue(
                                "SIGNIN_ALLOWED pref should be set by default",
                                UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                        .getBoolean(Pref.SIGNIN_ALLOWED)));
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefService prefService =
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                    prefService.clearPref(Pref.SIGNIN_ALLOWED);
                });
    }

    @Test
    @LargeTest
    public void allowSigninOptionHiddenFromChildUser() {
        mSigninTestRule.addChildTestAccountThenWaitForSignin();
        final Profile profile =
                ThreadUtils.runOnUiThreadBlocking(ProfileManager::getLastUsedRegularProfile);
        CriteriaHelper.pollUiThread(profile::isChild);

        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();
        ChromeSwitchPreference allowChromeSignin =
                (ChromeSwitchPreference)
                        googleServicesSettings.findPreference(
                                GoogleServicesSettings.PREF_ALLOW_SIGNIN);
        Assert.assertFalse(
                "Chrome Signin option should not be visible", allowChromeSignin.isVisible());
    }

    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void signOutUserWithoutShowingSignOutDialog() {
        mSigninTestRule.addTestAccountThenSignin();
        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();
        ChromeSwitchPreference allowChromeSignin =
                (ChromeSwitchPreference)
                        googleServicesSettings.findPreference(
                                GoogleServicesSettings.PREF_ALLOW_SIGNIN);
        Assert.assertTrue("Chrome Signin should be allowed", allowChromeSignin.isChecked());

        onView(withText(R.string.allow_chrome_signin_title)).perform(click());
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertFalse(
                                "Account should be signed out!",
                                IdentityServicesProvider.get()
                                        .getIdentityManager(
                                                ProfileManager.getLastUsedRegularProfile())
                                        .hasPrimaryAccount(ConsentLevel.SIGNIN)));
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertFalse(
                                "SIGNIN_ALLOWED pref should be unset",
                                UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                        .getBoolean(Pref.SIGNIN_ALLOWED)));
        Assert.assertFalse("Chrome Signin should not be allowed", allowChromeSignin.isChecked());
    }

    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void showSignOutDialogBeforeSigningUserOutLegacy() {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();
        ChromeSwitchPreference allowChromeSignin =
                (ChromeSwitchPreference)
                        googleServicesSettings.findPreference(
                                GoogleServicesSettings.PREF_ALLOW_SIGNIN);
        Assert.assertTrue("Chrome Signin should be allowed", allowChromeSignin.isChecked());

        onView(withText(R.string.allow_chrome_signin_title)).perform(click());
        // Accept the sign out Dialog
        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertFalse(
                                "Accepting the sign-out dialog should set SIGNIN_ALLOWED to false",
                                UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                        .getBoolean(Pref.SIGNIN_ALLOWED)));
        Assert.assertFalse("Chrome Signin should not be allowed", allowChromeSignin.isChecked());
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void showSignOutDialogBeforeSigningUserOut() {
        mSigninTestRule.addAccountThenSignin(SigninTestRule.TEST_ACCOUNT_1);
        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();
        ChromeSwitchPreference allowChromeSignin =
                (ChromeSwitchPreference)
                        googleServicesSettings.findPreference(
                                GoogleServicesSettings.PREF_ALLOW_SIGNIN);
        Assert.assertTrue("Chrome Signin should be allowed", allowChromeSignin.isChecked());

        onView(withText(R.string.allow_chrome_signin_title)).perform(click());
        onView(withText(R.string.sign_out_title)).inRoot(isDialog()).check(matches(isDisplayed()));
        // Accept the sign out Dialog
        onView(withText(R.string.sign_out)).inRoot(isDialog()).perform(click());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SnackbarManager snackbarManager =
                            mSettingsActivityTestRule.getActivity().getSnackbarManager();
                    Assert.assertTrue(snackbarManager.isShowing());
                    Snackbar currentSnackbar = snackbarManager.getCurrentSnackbarForTesting();
                    Assert.assertEquals(
                            currentSnackbar.getIdentifierForTesting(), Snackbar.UMA_SIGN_OUT);
                    Assert.assertEquals(
                            currentSnackbar.getTextForTesting(),
                            mActivityTestRule
                                    .getActivity()
                                    .getString(R.string.sign_out_snackbar_message));
                    Assert.assertFalse(
                            "Accepting the sign-out dialog should set SIGNIN_ALLOWED to false",
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                    .getBoolean(Pref.SIGNIN_ALLOWED));
                });
        Assert.assertFalse("Chrome Signin should not be allowed", allowChromeSignin.isChecked());
    }

    @Test
    @LargeTest
    @Feature({"Preference"})
    @EnableFeatures({ChromeFeatureList.COMMERCE_PRICE_TRACKING + "<Study"})
    @CommandLineFlags.Add({
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:allow_disable_price_annotations/true"
    })
    public void testPriceTrackingAnnotations() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PriceTrackingFeatures.setPriceTrackingEnabledForTesting(true);
                    PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
                });

        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeSwitchPreference priceAnnotationsSwitch =
                            (ChromeSwitchPreference)
                                    googleServicesSettings.findPreference(
                                            GoogleServicesSettings.PREF_PRICE_TRACKING_ANNOTATIONS);
                    Assert.assertTrue(priceAnnotationsSwitch.isVisible());
                    Assert.assertTrue(priceAnnotationsSwitch.isChecked());

                    priceAnnotationsSwitch.performClick();
                    Assert.assertFalse(
                            PriceTrackingUtilities.isTrackPricesOnTabsEnabled(
                                    ProfileManager.getLastUsedRegularProfile()));
                    priceAnnotationsSwitch.performClick();
                    Assert.assertTrue(
                            PriceTrackingUtilities.isTrackPricesOnTabsEnabled(
                                    ProfileManager.getLastUsedRegularProfile()));
                });
    }

    @Test
    @LargeTest
    @Feature({"Preference"})
    @EnableFeatures({ChromeFeatureList.COMMERCE_PRICE_TRACKING + "<Study"})
    @CommandLineFlags.Add({
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:allow_disable_price_annotations/false"
    })
    public void testPriceTrackingAnnotations_FeatureDisabled() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PriceTrackingFeatures.setPriceTrackingEnabledForTesting(true);
                    PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
                });

        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertNull(
                            googleServicesSettings.findPreference(
                                    GoogleServicesSettings.PREF_PRICE_TRACKING_ANNOTATIONS));
                });
    }

    @Test
    @LargeTest
    @Feature({"Preference"})
    @EnableFeatures({ChromeFeatureList.COMMERCE_PRICE_TRACKING + "<Study"})
    @CommandLineFlags.Add({
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:allow_disable_price_annotations/true"
    })
    public void testPriceTrackingAnnotations_NotSignedIn() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PriceTrackingFeatures.setPriceTrackingEnabledForTesting(true);
                    PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(false);
                });

        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertNull(
                            googleServicesSettings.findPreference(
                                    GoogleServicesSettings.PREF_PRICE_TRACKING_ANNOTATIONS));
                });
    }

    @Test
    @LargeTest
    @MinAndroidSdkLevel(
            value = Build.VERSION_CODES.Q,
            reason = "Digital Wellbeing is only available from Q.")
    public void testUsageStatsReportingShown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefService prefService =
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                    prefService.setBoolean(Pref.USAGE_STATS_ENABLED, true);
                });

        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertNotNull(
                            "Usage stats should exist when the flag and pref are set.",
                            googleServicesSettings.findPreference(
                                    GoogleServicesSettings.PREF_USAGE_STATS_REPORTING));
                });
    }

    @Test
    @LargeTest
    @MinAndroidSdkLevel(
            value = Build.VERSION_CODES.Q,
            reason = "Digital Wellbeing is only available from Q.")
    public void testUsageStatsReportingNotShown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefService prefService =
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                    prefService.setBoolean(Pref.USAGE_STATS_ENABLED, false);
                });

        final GoogleServicesSettings googleServicesSettings = startGoogleServicesSettings();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertNull(
                            "Usage stats should not exist when the pref is not set.",
                            googleServicesSettings.findPreference(
                                    GoogleServicesSettings.PREF_USAGE_STATS_REPORTING));
                });
    }

    @Test
    @LargeTest
    public void hidePasswordsAccountStorageToggleIfSyncing() {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();

        startGoogleServicesSettings();

        onView(withText(R.string.passwords_account_storage_toggle_title)).check(doesNotExist());
    }

    @Test
    @LargeTest
    public void hidePasswordsAccountStorageToggleIfSignedOut() {
        Assert.assertEquals(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN), null);

        startGoogleServicesSettings();

        onView(withText(R.string.passwords_account_storage_toggle_title)).check(doesNotExist());
    }

    @Test
    @LargeTest
    @DisableFeatures({ChromeFeatureList.ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS})
    public void hidePasswordsAccountStorageToggleIfSignedInAndFlagDisabled() {
        mSigninTestRule.addTestAccountThenSignin();

        startGoogleServicesSettings();

        onView(withText(R.string.passwords_account_storage_toggle_title)).check(doesNotExist());
    }

    @Test
    @LargeTest
    @EnableFeatures({
        ChromeFeatureList.ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS,
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS
    })
    public void hidePasswordsAccountStorageToggleIfSignedInAndSyncToSigninEnabled() {
        mSigninTestRule.addTestAccountThenSignin();

        startGoogleServicesSettings();

        onView(withText(R.string.passwords_account_storage_toggle_title)).check(doesNotExist());
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS})
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void hidePasswordsAccountStorageToggleIfGmsCoreOutdated() {
        when(mMockPasswordManagerUtilBridgeJni.isGmsCoreUpdateRequired(any(), any()))
                .thenReturn(true);
        mSigninTestRule.addTestAccountThenSignin();

        startGoogleServicesSettings();

        onView(withText(R.string.passwords_account_storage_toggle_title)).check(doesNotExist());
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS})
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void showPasswordsAccountStorageToggleIfSignedInAndFlagEnabled() {
        CoreAccountInfo account = mSigninTestRule.addTestAccountThenSignin();

        GoogleServicesSettings settings = startGoogleServicesSettings();

        onView(withText(R.string.passwords_account_storage_toggle_title))
                .check(matches(isDisplayed()));
        String expectedSummary =
                mSettingsActivityTestRule
                        .getActivity()
                        .getString(
                                R.string.passwords_account_storage_toggle_summary,
                                account.getEmail());
        onView(withText(expectedSummary)).check(matches(isDisplayed()));
        // Note: Using isChecked() with espresso is tricky, since the title view is a sibling of the
        // toggle view. Just resort to findPreference().
        ChromeSwitchPreference toggle =
                (ChromeSwitchPreference)
                        settings.findPreference(
                                GoogleServicesSettings.PREF_PASSWORDS_ACCOUNT_STORAGE);
        Assert.assertTrue(toggle.isChecked());
        Assert.assertNull(toggle.getBackgroundColor());
        Assert.assertTrue(isPasswordSyncEnabled());

        onView(withText(R.string.passwords_account_storage_toggle_title)).perform(click());

        onView(withText(R.string.passwords_account_storage_toggle_title))
                .check(matches(isDisplayed()));
        Assert.assertFalse(toggle.isChecked());
        Assert.assertFalse(isPasswordSyncEnabled());

        onView(withText(R.string.passwords_account_storage_toggle_title)).perform(click());

        onView(withText(R.string.passwords_account_storage_toggle_title))
                .check(matches(isDisplayed()));
        Assert.assertTrue(toggle.isChecked());
        Assert.assertTrue(isPasswordSyncEnabled());
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS})
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void showPasswordsAccountStorageToggleForNonDisplayableEmail() {
        // TODO(b/343378391) Update accountInfo to use
        // AccountManagerTestRule.TEST_ACCOUNT_NON_DISPLAYABLE_EMAIL.
        mSigninTestRule.addAccountThenSignin(
                new AccountInfo.Builder(
                                "auto-generated-email@gmail.com",
                                FakeAccountManagerFacade.toGaiaId("auto-generated-email@gmail.com"))
                        .fullName("John Doe")
                        .givenName("John")
                        .accountCapabilities(
                                new AccountCapabilitiesBuilder()
                                        .setIsSubjectToParentalControls(true)
                                        .setCanHaveEmailAddressDisplayed(false)
                                        .build())
                        .build());

        startGoogleServicesSettings();

        onView(withText(R.string.passwords_account_storage_toggle_summary_no_email))
                .check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS})
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void showPasswordsAccountStorageToggleWithHighlight() {
        mSigninTestRule.addTestAccountThenSignin();

        Bundle args = new Bundle();
        args.putBoolean(AccountStorageToggleFragmentArgs.HIGHLIGHT, true);
        mSettingsActivityTestRule.startSettingsActivity(args);

        ChromeSwitchPreference toggle =
                (ChromeSwitchPreference)
                        mSettingsActivityTestRule
                                .getFragment()
                                .findPreference(
                                        GoogleServicesSettings.PREF_PASSWORDS_ACCOUNT_STORAGE);
        @Nullable Integer backgroundColor = toggle.getBackgroundColor();
        Assert.assertNotNull(backgroundColor);
    }

    private boolean isPasswordSyncEnabled() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        SyncServiceFactory.getForProfile(ProfileManager.getLastUsedRegularProfile())
                                .getSelectedTypes()
                                .contains(UserSelectableType.PASSWORDS));
    }

    private GoogleServicesSettings startGoogleServicesSettings() {
        mSettingsActivityTestRule.startSettingsActivity();
        return mSettingsActivityTestRule.getFragment();
    }
}
