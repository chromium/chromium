// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.preference.Preference;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.CollectionUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelperJni;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.SafeBrowsingState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.UpdatesState;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Collections;
import java.util.Set;

/** Tests {@link SafetyCheckSettingsFragment} together with {@link SafetyCheckViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(
        reason =
                "The activity should be restarted for each test to not share saved user preferences"
                        + " between tests.")
public class SafetyCheckSettingsFragmentTest {
    private static final String TEST_EMAIL_ADDRESS = "test@example.com";
    private static final String PASSWORDS_LOCAL = "passwords_local";
    private static final String PASSWORDS_ACCOUNT = "passwords_account";
    private static final String SAFE_BROWSING = "safe_browsing";
    private static final String UPDATES = "updates";
    private static final long S_TO_MS = 1000;
    private static final long MIN_TO_MS = 60 * S_TO_MS;
    private static final long H_TO_MS = 60 * MIN_TO_MS;
    private static final long DAY_TO_MS = 24 * H_TO_MS;

    @Rule
    public SettingsActivityTestRule<SafetyCheckSettingsFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(SafetyCheckSettingsFragment.class);

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private PasswordCheck mPasswordCheck;
    @Mock private SyncService mSyncService;
    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeNativeMock;
    @Mock private PasswordManagerHelper.Natives mPasswordManagerHelperNativeMock;

    private PropertyModel mSafetyCheckModel;
    private PropertyModel mPasswordCheckPreferenceLocalModel;
    private SafetyCheckSettingsFragment mFragment;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        PasswordCheckFactory.setPasswordCheckForTesting(mPasswordCheck);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        mJniMocker.mock(
                PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeNativeMock);
        mJniMocker.mock(PasswordManagerHelperJni.TEST_HOOKS, mPasswordManagerHelperNativeMock);
    }

    @Test
    @SmallTest
    public void testLastRunTimestampStrings() {
        long t0 = 12345;
        Context context = ApplicationProvider.getApplicationContext();
        // Start time not set - returns an empty string.
        assertEquals("", SafetyCheckViewBinder.getLastRunTimestampText(context, 0, 123));
        assertEquals(
                "Checked just now",
                SafetyCheckViewBinder.getLastRunTimestampText(context, t0, t0 + 10 * S_TO_MS));
        assertEquals(
                "Checked 1 minute ago",
                SafetyCheckViewBinder.getLastRunTimestampText(context, t0, t0 + MIN_TO_MS));
        assertEquals(
                "Checked 17 minutes ago",
                SafetyCheckViewBinder.getLastRunTimestampText(context, t0, t0 + 17 * MIN_TO_MS));
        assertEquals(
                "Checked 1 hour ago",
                SafetyCheckViewBinder.getLastRunTimestampText(context, t0, t0 + H_TO_MS));
        assertEquals(
                "Checked 13 hours ago",
                SafetyCheckViewBinder.getLastRunTimestampText(context, t0, t0 + 13 * H_TO_MS));
        assertEquals(
                "Checked yesterday",
                SafetyCheckViewBinder.getLastRunTimestampText(context, t0, t0 + DAY_TO_MS));
        assertEquals(
                "Checked yesterday",
                SafetyCheckViewBinder.getLastRunTimestampText(context, t0, t0 + 2 * DAY_TO_MS - 1));
        assertEquals(
                "Checked 2 days ago",
                SafetyCheckViewBinder.getLastRunTimestampText(context, t0, t0 + 2 * DAY_TO_MS));
        assertEquals(
                "Checked 315 days ago",
                SafetyCheckViewBinder.getLastRunTimestampText(context, t0, t0 + 315 * DAY_TO_MS));
    }

    private void createFragmentAndModel() {
        mSettingsActivityTestRule.startSettingsActivity();
        mFragment = (SafetyCheckSettingsFragment) mSettingsActivityTestRule.getFragment();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSafetyCheckModel =
                            SafetyCheckCoordinator.createSafetyCheckModelAndBind(mFragment);
                    mPasswordCheckPreferenceLocalModel =
                            SafetyCheckCoordinator.createPasswordCheckPreferenceModelAndBind(
                                    mFragment,
                                    mSafetyCheckModel,
                                    SafetyCheckViewBinder.PASSWORDS_KEY_LOCAL,
                                    "Passwords");
                });
    }

    private void createFragmentAndModelByBundle(boolean safetyCheckImmediateRun) {
        mSettingsActivityTestRule.startSettingsActivity(
                SafetyCheckSettingsFragment.createBundle(safetyCheckImmediateRun));
        mFragment = (SafetyCheckSettingsFragment) mSettingsActivityTestRule.getFragment();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSafetyCheckModel =
                            SafetyCheckCoordinator.createSafetyCheckModelAndBind(mFragment);
                    mPasswordCheckPreferenceLocalModel =
                            SafetyCheckCoordinator.createPasswordCheckPreferenceModelAndBind(
                                    mFragment,
                                    mSafetyCheckModel,
                                    SafetyCheckViewBinder.PASSWORDS_KEY_LOCAL,
                                    "Passwords");
                });
    }

    private void configureMockSyncService(boolean isPasswordSyncEnabled) {
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(true);
        Set<Integer> selectedTypes =
                isPasswordSyncEnabled
                        ? CollectionUtil.newHashSet(UserSelectableType.PASSWORDS)
                        : Collections.EMPTY_SET;
        when(mSyncService.getSelectedTypes()).thenReturn(selectedTypes);
        when(mSyncService.getAccountInfo())
                .thenReturn(CoreAccountInfo.createFromEmailAndGaiaId(TEST_EMAIL_ADDRESS, "0"));
        when(mPasswordManagerHelperNativeMock.hasChosenToSyncPasswords(mSyncService))
                .thenReturn(isPasswordSyncEnabled);
    }

    private void configurePasswordManagerUtilBridge(boolean usesSplitStores) {
        when(mPasswordManagerUtilBridgeNativeMock.usesSplitStoresAndUPMForLocal(any()))
                .thenReturn(usesSplitStores);
    }

    private void verifyNullStateDisplayedCorrectly(
            boolean isPasswordSyncEnabled, boolean usesSplitStores) {
        configureMockSyncService(isPasswordSyncEnabled);
        configurePasswordManagerUtilBridge(usesSplitStores);
        createFragmentAndModel();
        // Binds the account model.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SafetyCheckCoordinator.createPasswordCheckPreferenceModelAndBind(
                            mFragment,
                            mSafetyCheckModel,
                            SafetyCheckViewBinder.PASSWORDS_KEY_ACCOUNT,
                            "Passwords for test account");
                });

        Preference passwordsLocal = mFragment.findPreference(PASSWORDS_LOCAL);
        Preference passwordsAccount = mFragment.findPreference(PASSWORDS_ACCOUNT);
        Preference safeBrowsing = mFragment.findPreference(SAFE_BROWSING);
        Preference updates = mFragment.findPreference(UPDATES);

        assertEquals(!isPasswordSyncEnabled || usesSplitStores, passwordsLocal.isVisible());
        assertEquals(isPasswordSyncEnabled, passwordsAccount.isVisible());
        assertEquals("", passwordsLocal.getSummary());
        assertEquals("", passwordsAccount.getSummary());
        assertEquals("", safeBrowsing.getSummary());
        assertEquals("", updates.getSummary());
    }

    @Test
    @MediumTest
    public void testNullStateDisplayedCorrectlySyncOffNoUsingSplitStores() {
        verifyNullStateDisplayedCorrectly(
                /* isPasswordSyncEnabled= */ false, /* usesSplitStores= */ false);
    }

    @Test
    @MediumTest
    public void testNullStateDisplayedCorrectlySyncOffUsingSplitStores() {
        verifyNullStateDisplayedCorrectly(
                /* isPasswordSyncEnabled= */ false, /* usesSplitStores= */ true);
    }

    @Test
    @MediumTest
    public void testNullStateDisplayedCorrectlySyncOnNoUsingSplitStores() {
        verifyNullStateDisplayedCorrectly(
                /* isPasswordSyncEnabled= */ true, /* usesSplitStores= */ false);
    }

    @Test
    @MediumTest
    public void testNullStateDisplayedCorrectlySyncOnUsingSplitStores() {
        verifyNullStateDisplayedCorrectly(true, true);
    }

    @Test
    @MediumTest
    public void testPasswordsCheckTitlesAreCorrect() {
        configureMockSyncService(true);
        configurePasswordManagerUtilBridge(true);
        mSettingsActivityTestRule.startSettingsActivity();
        mFragment = (SafetyCheckSettingsFragment) mSettingsActivityTestRule.getFragment();

        Preference passwordsLocal = mFragment.findPreference(PASSWORDS_LOCAL);
        Preference passwordsAccount = mFragment.findPreference(PASSWORDS_ACCOUNT);

        assertEquals(
                passwordsLocal.getTitle(),
                mFragment.getString(R.string.safety_check_passwords_local_title));
        assertEquals(
                passwordsAccount.getTitle(),
                mFragment.getString(
                        R.string.safety_check_passwords_account_title, TEST_EMAIL_ADDRESS));
    }

    @Test
    @MediumTest
    public void testStateChangeDisplayedCorrectly() {
        createFragmentAndModel();

        Preference passwordsLocal = mFragment.findPreference(PASSWORDS_LOCAL);
        Preference safeBrowsing = mFragment.findPreference(SAFE_BROWSING);
        Preference updates = mFragment.findPreference(UPDATES);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Passwords state remains unchanged.
                    // Safe browsing is in "checking".
                    mSafetyCheckModel.set(
                            SafetyCheckProperties.SAFE_BROWSING_STATE, SafeBrowsingState.CHECKING);
                    // Updates goes through "checking" and ends up in "outdated".
                    mSafetyCheckModel.set(
                            SafetyCheckProperties.UPDATES_STATE, UpdatesState.CHECKING);
                    mSafetyCheckModel.set(
                            SafetyCheckProperties.UPDATES_STATE, UpdatesState.OUTDATED);
                });

        assertEquals(true, passwordsLocal.isVisible());
        assertEquals("", passwordsLocal.getSummary());
        assertEquals("", safeBrowsing.getSummary());
        assertEquals(
                ApplicationProvider.getApplicationContext()
                        .getString(R.string.safety_check_updates_outdated),
                updates.getSummary());
    }

    @Test
    @MediumTest
    public void testSafetyCheckElementsOnClick() {
        createFragmentAndModel();
        CallbackHelper passwordsLocalClicked = new CallbackHelper();
        CallbackHelper safeBrowsingClicked = new CallbackHelper();
        CallbackHelper updatesClicked = new CallbackHelper();
        // Set the listeners
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPasswordCheckPreferenceLocalModel.set(
                            PasswordsCheckPreferenceProperties.PASSWORDS_CLICK_LISTENER,
                            (Preference.OnPreferenceClickListener)
                                    (p) -> {
                                        passwordsLocalClicked.notifyCalled();
                                        return true;
                                    });
                    mSafetyCheckModel.set(
                            SafetyCheckProperties.SAFE_BROWSING_CLICK_LISTENER,
                            (Preference.OnPreferenceClickListener)
                                    (p) -> {
                                        safeBrowsingClicked.notifyCalled();
                                        return true;
                                    });
                    mSafetyCheckModel.set(
                            SafetyCheckProperties.UPDATES_CLICK_LISTENER,
                            (Preference.OnPreferenceClickListener)
                                    (p) -> {
                                        updatesClicked.notifyCalled();
                                        return true;
                                    });
                });
        Preference passwordsLocal = mFragment.findPreference(PASSWORDS_LOCAL);
        Preference safeBrowsing = mFragment.findPreference(SAFE_BROWSING);
        Preference updates = mFragment.findPreference(UPDATES);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // If local password storage is available, should be clickable.
                    passwordsLocal.performClick();
                    // Safe browsing is in "checking", the element deactivates, not clickable.
                    mSafetyCheckModel.set(
                            SafetyCheckProperties.SAFE_BROWSING_STATE, SafeBrowsingState.CHECKING);
                    safeBrowsing.performClick();
                    // Updates goes through "checking" and ends up in "outdated".
                    // Checking: the element deactivates, clicks are not handled.
                    mSafetyCheckModel.set(
                            SafetyCheckProperties.UPDATES_STATE, UpdatesState.CHECKING);
                    // Final state: the element is reactivated and should handle clicks.
                    mSafetyCheckModel.set(
                            SafetyCheckProperties.UPDATES_STATE, UpdatesState.OUTDATED);
                    updates.performClick();
                });
        assertEquals(1, passwordsLocalClicked.getCallCount());
        assertEquals(0, safeBrowsingClicked.getCallCount());
        assertEquals(1, updatesClicked.getCallCount());
    }

    @Test
    @MediumTest
    public void testSafetyCheckDoNotImmediatelyRunByDefault() {
        createFragmentAndModelByBundle(/* safetyCheckImmediateRun= */ false);
        assertEquals(false, mFragment.shouldRunSafetyCheckImmediately());
        assertEquals(
                0,
                ChromeSharedPreferences.getInstance()
                        .readLong(
                                ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP, 0));
    }

    @Test
    @MediumTest
    public void testSafetyCheckImmediatelyRunByBundle() {
        createFragmentAndModelByBundle(/* safetyCheckImmediateRun= */ true);

        // Make sure the safety check was ran.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ChromeSharedPreferences.getInstance()
                                    .readLong(
                                            ChromePreferenceKeys
                                                    .SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP,
                                            0),
                            Matchers.not(0));
                });
    }
}
