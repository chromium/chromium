// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.contrib.RecyclerViewActions.scrollToHolder;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.password_manager.settings.PasswordSettingsTestHelper.hasTextInViewHolder;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;

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
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.DummySettingsForTest;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.PassphraseType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for the "Passwords" settings screen. These tests are not batchable (without significant
 * effort), so consider splitting large new suites into separate classes.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordSettingsTest {
    @Rule
    public SettingsActivityTestRule<DummySettingsForTest> mDummySettingsActivityTestRule =
            new SettingsActivityTestRule<>(DummySettingsForTest.class);

    @Rule
    public SettingsActivityTestRule<PasswordSettings> mPasswordSettingsActivityTestRule =
            new SettingsActivityTestRule<>(PasswordSettings.class);

    @Mock
    private PasswordCheck mPasswordCheck;

    @Mock
    private SyncService mMockSyncService;

    private final PasswordSettingsTestHelper mTestHelper = new PasswordSettingsTestHelper();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        PasswordCheckFactory.setPasswordCheckForTesting(mPasswordCheck);

        // By default sync is off. Tests can override this later.
        setSyncServiceState(false, false);
        TestThreadUtils.runOnUiThreadBlocking(() -> SyncService.overrideForTests(mMockSyncService));

        // This initializes the browser, so some tests can do setup before PasswordSettings is
        // launched. ChromeTabbedActivityTestRule.startMainActivityOnBlankPage() is more commonly
        // used for this end, but using another settings activity instead makes these tests more
        // isolated, i.e. avoids exercising unnecessary logic. DummyUiActivityTestCase also won't
        // fit here, it doesn't initialize enough of the browser.
        mDummySettingsActivityTestRule.startSettingsActivity(null);
    }

    @After
    public void tearDown() {
        mTestHelper.tearDown();
    }

    /**
     * Ensure that resetting of empty passwords list works.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetListEmpty() {
        // Load the preferences, they should show the empty list.
        mTestHelper.startPasswordSettingsFromMainSettings(mPasswordSettingsActivityTestRule);
        onViewWaiting(withText(R.string.password_settings_title));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { getPrefService().setBoolean(Pref.CREDENTIALS_ENABLE_SERVICE, true); });

        final SettingsActivity settingsActivity = mTestHelper.startPasswordSettingsFromMainSettings(
                mPasswordSettingsActivityTestRule);
        onViewWaiting(withText(R.string.password_settings_title));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PasswordSettings savedPasswordPrefs = mPasswordSettingsActivityTestRule.getFragment();
            ChromeSwitchPreference onOffSwitch =
                    (ChromeSwitchPreference) savedPasswordPrefs.findPreference(
                            PasswordSettings.PREF_SAVE_PASSWORDS_SWITCH);
            Assert.assertTrue(onOffSwitch.isChecked());

            onOffSwitch.performClick();
            Assert.assertFalse(getPrefService().getBoolean(Pref.CREDENTIALS_ENABLE_SERVICE));
            onOffSwitch.performClick();
            Assert.assertTrue(getPrefService().getBoolean(Pref.CREDENTIALS_ENABLE_SERVICE));

            settingsActivity.finish();

            getPrefService().setBoolean(Pref.CREDENTIALS_ENABLE_SERVICE, false);
        });

        mTestHelper.startPasswordSettingsFromMainSettings(mPasswordSettingsActivityTestRule);
        onViewWaiting(withText(R.string.password_settings_title));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PasswordSettings savedPasswordPrefs = mPasswordSettingsActivityTestRule.getFragment();
            ChromeSwitchPreference onOffSwitch =
                    (ChromeSwitchPreference) savedPasswordPrefs.findPreference(
                            PasswordSettings.PREF_SAVE_PASSWORDS_SWITCH);
            Assert.assertFalse(onOffSwitch.isChecked());
        });
    }

    /**
     *  Tests that the link pointing to managing passwords in the user's account is not displayed
     *  for non signed in users.
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
     *  Tests that the link pointing to managing passwords in the user's account is not displayed
     *  for signed in users, not syncing passwords.
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
     *  Tests that the link pointing to managing passwords in the user's account is displayed for
     *  users syncing passwords.
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
     *  Tests that the link pointing to managing passwords in the user's account is not displayed
     *  for users syncing passwords with custom passphrase.
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { getPrefService().setBoolean(Pref.CREDENTIALS_ENABLE_AUTOSIGNIN, true); });

        final SettingsActivity settingsActivity = mTestHelper.startPasswordSettingsFromMainSettings(
                mPasswordSettingsActivityTestRule);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PasswordSettings passwordPrefs = mPasswordSettingsActivityTestRule.getFragment();
            ChromeSwitchPreference onOffSwitch =
                    (ChromeSwitchPreference) passwordPrefs.findPreference(
                            PasswordSettings.PREF_AUTOSIGNIN_SWITCH);
            Assert.assertTrue(onOffSwitch.isChecked());

            onOffSwitch.performClick();
            Assert.assertFalse(getPrefService().getBoolean(Pref.CREDENTIALS_ENABLE_AUTOSIGNIN));
            onOffSwitch.performClick();
            Assert.assertTrue(getPrefService().getBoolean(Pref.CREDENTIALS_ENABLE_AUTOSIGNIN));

            settingsActivity.finish();

            getPrefService().setBoolean(Pref.CREDENTIALS_ENABLE_AUTOSIGNIN, false);
        });

        mTestHelper.startPasswordSettingsFromMainSettings(mPasswordSettingsActivityTestRule);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PasswordSettings passwordPrefs = mPasswordSettingsActivityTestRule.getFragment();
            ChromeSwitchPreference onOffSwitch =
                    (ChromeSwitchPreference) passwordPrefs.findPreference(
                            PasswordSettings.PREF_AUTOSIGNIN_SWITCH);
            Assert.assertFalse(onOffSwitch.isChecked());
        });
    }

    /**
     * Check that the check passwords preference is shown.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testCheckPasswordsEnabled() {
        mTestHelper.startPasswordSettingsFromMainSettings(mPasswordSettingsActivityTestRule);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PasswordSettings passwordPrefs = mPasswordSettingsActivityTestRule.getFragment();
            Assert.assertNotNull(
                    passwordPrefs.findPreference(PasswordSettings.PREF_CHECK_PASSWORDS));
        });
    }

    /**
     * Check whether the user is asked to set up a screen lock if attempting to view passwords.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures({ChromeFeatureList.EDIT_PASSWORDS_IN_SETTINGS})
    public void testViewPasswordNoLock() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.UNAVAILABLE);

        final SettingsActivity settingsActivity = mTestHelper.startPasswordSettingsFromMainSettings(
                mPasswordSettingsActivityTestRule);

        View mainDecorView = settingsActivity.getWindow().getDecorView();
        onView(withId(R.id.recycler_view))
                .perform(scrollToHolder(hasTextInViewHolder("test user")));
        onView(withText(containsString("test user"))).perform(click());
        onView(withContentDescription(R.string.password_entry_viewer_copy_stored_password))
                .perform(click());
        onView(withText(R.string.password_entry_viewer_set_lock_screen))
                .inRoot(withDecorView(not(is(mainDecorView))))
                .check(matches(isDisplayed()));
    }

    /**
     * Check whether the user can view a saved password.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures({ChromeFeatureList.EDIT_PASSWORDS_IN_SETTINGS})
    public void testViewPassword() {
        mTestHelper.setPasswordSource(
                new SavedPasswordEntry("https://example.com", "test user", "test password"));

        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        mTestHelper.startPasswordSettingsFromMainSettings(mPasswordSettingsActivityTestRule);
        onView(withId(R.id.recycler_view))
                .perform(scrollToHolder(hasTextInViewHolder("test user")));
        onView(withText(containsString("test user"))).perform(click());

        // Before tapping the view button, pretend that the last successful reauthentication just
        // happened. This will allow showing the password.
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.ReauthScope.ONE_AT_A_TIME);
        onView(withContentDescription(R.string.password_entry_viewer_view_stored_password))
                .perform(click());
        onView(withText("test password")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testDestroysPasswordCheckIfFirstInSettingsStack() {
        setSyncServiceState(true, true);
        SettingsActivity activity =
                mTestHelper.startPasswordSettingsDirectly(mPasswordSettingsActivityTestRule);
        activity.finish();
        CriteriaHelper.pollUiThread(() -> activity.isDestroyed());
        Assert.assertNull(PasswordCheckFactory.getPasswordCheckInstance());
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testDoesNotDestroyPasswordCheckIfNotFirstInSettingsStack() {
        setSyncServiceState(true, true);
        SettingsActivity activity = mTestHelper.startPasswordSettingsFromMainSettings(
                mPasswordSettingsActivityTestRule);
        activity.finish();
        CriteriaHelper.pollUiThread(() -> activity.isDestroyed());
        Assert.assertNotNull(PasswordCheckFactory.getPasswordCheckInstance());
        // Clean up the password check component.
        PasswordCheckFactory.destroy();
    }

    private static PrefService getPrefService() {
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }

    private void setSyncServiceState(
            final boolean usingCustomPassphrase, final boolean syncingPasswords) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            when(mMockSyncService.hasSyncConsent()).thenReturn(syncingPasswords);
            when(mMockSyncService.isEngineInitialized()).thenReturn(true);
            when(mMockSyncService.isUsingExplicitPassphrase()).thenReturn(usingCustomPassphrase);
            when(mMockSyncService.getPassphraseType())
                    .thenReturn(usingCustomPassphrase ? PassphraseType.CUSTOM_PASSPHRASE
                                                      : PassphraseType.KEYSTORE_PASSPHRASE);
            when(mMockSyncService.getActiveDataTypes())
                    .thenReturn(CollectionUtil.newHashSet(
                            syncingPasswords ? ModelType.PASSWORDS : ModelType.AUTOFILL));
        });
    }
}
