// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.os.Bundle;

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

import org.chromium.base.CollectionUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.PassphraseType;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Tests for the "Passwords" settings screen. These tests are not batchable (without significant
 * effort), so consider splitting large new suites into separate classes.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordSettingsTest {
    private static final String OFFER_TO_SAVE_PASSWORDS_HISTOGRAM =
            "PasswordManager.Settings.ToggleOfferToSavePasswords";
    private static final String AUTO_SIGNIN_HISTOGRAM = "PasswordManager.Settings.ToggleAutoSignIn";

    @Rule
    public SettingsActivityTestRule<PasswordSettings> mPasswordSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PasswordSettings.class);

    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new AutomotiveContextWrapperTestRule();

    @Mock private PasswordCheck mPasswordCheck;

    @Mock private SyncService mMockSyncService;

    private final PasswordSettingsTestHelper mTestHelper = new PasswordSettingsTestHelper();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        PasswordCheckFactory.setPasswordCheckForTesting(mPasswordCheck);

        // By default sync is off. Tests can override this later.
        setSyncServiceState(false, false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> SyncServiceFactory.setInstanceForTesting(mMockSyncService));

        // This initializes the browser, so some tests can do setup before PasswordSettings is
        // launched. ChromeTabbedActivityTestRule.startMainActivityOnBlankPage() is more commonly
        // used for this end, but using another settings activity instead makes these tests more
        // isolated, i.e. avoids exercising unnecessary logic. BlankUiTestActivityTestCase also
        // won't fit here, it doesn't initialize enough of the browser.
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(
                PasswordManagerHelper.MANAGE_PASSWORDS_REFERRER,
                ManagePasswordsReferrer.CHROME_SETTINGS);
        mPasswordSettingsActivityTestRule.startSettingsActivity(fragmentArgs);
        mPasswordSettingsActivityTestRule.finishActivity();
    }

    @After
    public void tearDown() {
        mTestHelper.tearDown();
    }

    /** Ensure that resetting of empty passwords list works. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetListEmpty() {
        // Load the preferences, they should show the empty list.
        mTestHelper.startPasswordSettingsFromMainSettings(mPasswordSettingsActivityTestRule);
        onViewWaiting(withText(R.string.password_manager_settings_title));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PasswordSettings savePasswordPreferences =
                            mPasswordSettingsActivityTestRule.getFragment();
                    // Emulate an update from PasswordStore. This should not crash.
                    savePasswordPreferences.passwordListAvailable(0);
                });
    }

    /**
     * Ensure that the on/off switch in "Save Passwords" settings actually enables and disables
     * password saving.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSavePasswordsSwitch() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.CREDENTIALS_ENABLE_SERVICE, true);
                });

        mTestHelper.startPasswordSettingsFromMainSettings(mPasswordSettingsActivityTestRule);
        onViewWaiting(withText(R.string.password_manager_settings_title));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PasswordSettings savedPasswordPrefs =
                            mPasswordSettingsActivityTestRule.getFragment();
                    ChromeSwitchPreference onOffSwitch =
                            (ChromeSwitchPreference)
                                    savedPasswordPrefs.findPreference(
                                            PasswordSettings.PREF_SAVE_PASSWORDS_SWITCH);
                    assertTrue(onOffSwitch.isChecked());

                    onOffSwitch.performClick();
                    assertFalse(getPrefService().getBoolean(Pref.CREDENTIALS_ENABLE_SERVICE));
                    Assert.assertEquals(
                            1,
                            RecordHistogram.getHistogramValueCountForTesting(
                                    OFFER_TO_SAVE_PASSWORDS_HISTOGRAM, 0));

                    onOffSwitch.performClick();
                    assertTrue(getPrefService().getBoolean(Pref.CREDENTIALS_ENABLE_SERVICE));
                    Assert.assertEquals(
                            1,
                            RecordHistogram.getHistogramValueCountForTesting(
                                    OFFER_TO_SAVE_PASSWORDS_HISTOGRAM, 1));
                });

        mPasswordSettingsActivityTestRule.finishActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.CREDENTIALS_ENABLE_SERVICE, false);
                });

        mTestHelper.startPasswordSettingsFromMainSettings(mPasswordSettingsActivityTestRule);
        onViewWaiting(withText(R.string.password_manager_settings_title));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PasswordSettings savedPasswordPrefs =
                            mPasswordSettingsActivityTestRule.getFragment();
                    ChromeSwitchPreference onOffSwitch =
                            (ChromeSwitchPreference)
                                    savedPasswordPrefs.findPreference(
                                            PasswordSettings.PREF_SAVE_PASSWORDS_SWITCH);
                    assertFalse(onOffSwitch.isChecked());
                });
    }

    /**
     * Tests that the link pointing to managing passwords in the user's account is not displayed for
     * non signed in users.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testManageAccountLinkNotSignedIn() {
        // Add a password entry, because the link is only displayed if the password list is not
        // empty.
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));
        mTestHelper.startPasswordSettingsFromMainSettings(mPasswordSettingsActivityTestRule);
        PasswordSettings savedPasswordPrefs = mPasswordSettingsActivityTestRule.getFragment();
        Assert.assertNull(
                savedPasswordPrefs.findPreference(PasswordSettings.PREF_KEY_MANAGE_ACCOUNT_LINK));
    }

    /**
     * Tests that the link pointing to managing passwords in the user's account is not displayed for
     * signed in users, not syncing passwords.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testManageAccountLinkSignedInNotSyncing() {
        // Add a password entry, because the link is only displayed if the password list is not
        // empty.
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));
        setSyncServiceState(false, false);

        mTestHelper.startPasswordSettingsFromMainSettings(mPasswordSettingsActivityTestRule);
        PasswordSettings savedPasswordPrefs = mPasswordSettingsActivityTestRule.getFragment();

        Assert.assertNull(
                savedPasswordPrefs.findPreference(PasswordSettings.PREF_KEY_MANAGE_ACCOUNT_LINK));
    }

    /**
     * Tests that the link pointing to managing passwords in the user's account is displayed for
     * users syncing passwords.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testManageAccountLinkSyncing() {
        // Add a password entry, because the link is only displayed if the password list is not
        // empty.
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));
        setSyncServiceState(false, true);

        mTestHelper.startPasswordSettingsFromMainSettings(mPasswordSettingsActivityTestRule);
        PasswordSettings savedPasswordPrefs = mPasswordSettingsActivityTestRule.getFragment();

        Assert.assertNotNull(
                savedPasswordPrefs.findPreference(PasswordSettings.PREF_KEY_MANAGE_ACCOUNT_LINK));
    }

    /**
     * Tests that the link pointing to managing passwords in the user's account is not displayed for
     * users syncing passwords with custom passphrase.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testManageAccountLinkSyncingWithPassphrase() {
        // Add a password entry, because the link is only displayed if the password list is not
        // empty.
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));
        setSyncServiceState(true, true);

        mTestHelper.startPasswordSettingsFromMainSettings(mPasswordSettingsActivityTestRule);
        PasswordSettings savedPasswordPrefs = mPasswordSettingsActivityTestRule.getFragment();

        Assert.assertNull(
                savedPasswordPrefs.findPreference(PasswordSettings.PREF_KEY_MANAGE_ACCOUNT_LINK));
    }

    /**
     * Ensure that the "Auto Sign-in" switch in "Save Passwords" settings actually enables and
     * disables auto sign-in.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAutoSignInCheckbox() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.CREDENTIALS_ENABLE_AUTOSIGNIN, true);
                });

        mTestHelper.startPasswordSettingsFromMainSettings(mPasswordSettingsActivityTestRule);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PasswordSettings passwordPrefs =
                            mPasswordSettingsActivityTestRule.getFragment();
                    ChromeSwitchPreference onOffSwitch =
                            (ChromeSwitchPreference)
                                    passwordPrefs.findPreference(
                                            PasswordSettings.PREF_AUTOSIGNIN_SWITCH);
                    assertTrue(onOffSwitch.isChecked());

                    onOffSwitch.performClick();
                    assertFalse(getPrefService().getBoolean(Pref.CREDENTIALS_ENABLE_AUTOSIGNIN));
                    Assert.assertEquals(
                            1,
                            RecordHistogram.getHistogramValueCountForTesting(
                                    AUTO_SIGNIN_HISTOGRAM, 0));

                    onOffSwitch.performClick();
                    assertTrue(getPrefService().getBoolean(Pref.CREDENTIALS_ENABLE_AUTOSIGNIN));
                    Assert.assertEquals(
                            1,
                            RecordHistogram.getHistogramValueCountForTesting(
                                    AUTO_SIGNIN_HISTOGRAM, 1));
                });

        mPasswordSettingsActivityTestRule.finishActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.CREDENTIALS_ENABLE_AUTOSIGNIN, false);
                });

        mTestHelper.startPasswordSettingsFromMainSettings(mPasswordSettingsActivityTestRule);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PasswordSettings passwordPrefs =
                            mPasswordSettingsActivityTestRule.getFragment();
                    ChromeSwitchPreference onOffSwitch =
                            (ChromeSwitchPreference)
                                    passwordPrefs.findPreference(
                                            PasswordSettings.PREF_AUTOSIGNIN_SWITCH);
                    assertFalse(onOffSwitch.isChecked());
                });
    }

    /**
     * Ensure that the "Auto Sign-in" switch in "Save Passwords" settings is not present on
     * automotive.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAutoSignInCheckboxIsNotPresentOnAutomotive() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);

        mTestHelper.startPasswordSettingsFromMainSettings(mPasswordSettingsActivityTestRule);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PasswordSettings passwordPrefs =
                            mPasswordSettingsActivityTestRule.getFragment();
                    ChromeSwitchPreference onOffSwitch =
                            (ChromeSwitchPreference)
                                    passwordPrefs.findPreference(
                                            PasswordSettings.PREF_AUTOSIGNIN_SWITCH);
                    assertNull("There should be no autosignin switch.", onOffSwitch);
                });
    }

    /** Check that the check passwords preference is shown. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testCheckPasswordsEnabled() {
        mTestHelper.startPasswordSettingsFromMainSettings(mPasswordSettingsActivityTestRule);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PasswordSettings passwordPrefs =
                            mPasswordSettingsActivityTestRule.getFragment();
                    Assert.assertNotNull(
                            passwordPrefs.findPreference(PasswordSettings.PREF_CHECK_PASSWORDS));
                });
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testDestroysPasswordCheckIfFirstInSettingsStack() {
        setSyncServiceState(true, true);
        mTestHelper.startPasswordSettingsDirectly(mPasswordSettingsActivityTestRule);
        mPasswordSettingsActivityTestRule.finishActivity();
        Assert.assertNull(PasswordCheckFactory.getPasswordCheckInstance());
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testDoesNotDestroyPasswordCheckIfNotFirstInSettingsStack() {
        setSyncServiceState(true, true);
        mTestHelper.startPasswordSettingsFromMainSettings(mPasswordSettingsActivityTestRule);
        mPasswordSettingsActivityTestRule.finishActivity();
        Assert.assertNotNull(PasswordCheckFactory.getPasswordCheckInstance());
        // Clean up the password check component.
        PasswordCheckFactory.destroy();
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testLocalPasswordsMigrationSheetTriggeredWhenShouldShow() {
        mTestHelper.setPasswordSourceWithMultipleEntries(PasswordSettingsTestHelper.GREEK_GODS);
        assertFalse(mTestHelper.getHandler().wasShowWarningCalled());
        mTestHelper.startPasswordSettingsFromMainSettings(mPasswordSettingsActivityTestRule);
        assertTrue(mTestHelper.getHandler().wasShowWarningCalled());
    }

    private static PrefService getPrefService() {
        return UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
    }

    private void setSyncServiceState(
            final boolean usingCustomPassphrase, final boolean syncingPasswords) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(mMockSyncService.hasSyncConsent()).thenReturn(syncingPasswords);
                    when(mMockSyncService.isEngineInitialized()).thenReturn(true);
                    when(mMockSyncService.isUsingExplicitPassphrase())
                            .thenReturn(usingCustomPassphrase);
                    when(mMockSyncService.getPassphraseType())
                            .thenReturn(
                                    usingCustomPassphrase
                                            ? PassphraseType.CUSTOM_PASSPHRASE
                                            : PassphraseType.KEYSTORE_PASSPHRASE);
                    when(mMockSyncService.getActiveDataTypes())
                            .thenReturn(
                                    CollectionUtil.newHashSet(
                                            syncingPasswords
                                                    ? DataType.PASSWORDS
                                                    : DataType.AUTOFILL));
                });
    }
}
