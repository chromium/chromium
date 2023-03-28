// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.preference.Preference;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.SafeBrowsingState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.UpdatesState;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests {@link SafetyCheckSettingsFragment} together with {@link SafetyCheckViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class SafetyCheckSettingsFragmentTest {
    private static final String PASSWORDS = "passwords";
    private static final String SAFE_BROWSING = "safe_browsing";
    private static final String UPDATES = "updates";
    private static final long S_TO_MS = 1000;
    private static final long MIN_TO_MS = 60 * S_TO_MS;
    private static final long H_TO_MS = 60 * MIN_TO_MS;
    private static final long DAY_TO_MS = 24 * H_TO_MS;

    @Rule
    public SettingsActivityTestRule<SafetyCheckSettingsFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(SafetyCheckSettingsFragment.class);

    @Rule
    public TestRule mFeaturesProcessor = new Features.JUnitProcessor();

    @Mock
    private SafetyCheckBridge mSafetyCheckBridge;

    @Mock
    private PasswordCheck mPasswordCheck;

    private PropertyModel mModel;
    private SafetyCheckSettingsFragment mFragment;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        PasswordCheckFactory.setPasswordCheckForTesting(mPasswordCheck);
        // Make the passwords initial status return immediately to avoid spinning animation.
        when(mPasswordCheck.getSavedPasswordsCount()).thenReturn(0);
        when(mPasswordCheck.getCompromisedCredentialsCount()).thenReturn(0);
        doAnswer(invocation -> {
            PasswordCheck.Observer observer =
                    (PasswordCheck.Observer) (invocation.getArguments()[0]);
            observer.onCompromisedCredentialsFetchCompleted();
            observer.onSavedPasswordsFetchCompleted();
            return null;
        })
                .when(mPasswordCheck)
                .addObserver(any(SafetyCheckMediator.class), eq(true));
    }

    @Test
    @SmallTest
    public void testLastRunTimestampStrings() {
        long t0 = 12345;
        Context context = InstrumentationRegistry.getTargetContext();
        // Start time not set - returns an empty string.
        assertEquals("", SafetyCheckViewBinder.getLastRunTimestampText(context, 0, 123));
        assertEquals("Checked just now",
                SafetyCheckViewBinder.getLastRunTimestampText(context, t0, t0 + 10 * S_TO_MS));
        assertEquals("Checked 1 minute ago",
                SafetyCheckViewBinder.getLastRunTimestampText(context, t0, t0 + MIN_TO_MS));
        assertEquals("Checked 17 minutes ago",
                SafetyCheckViewBinder.getLastRunTimestampText(context, t0, t0 + 17 * MIN_TO_MS));
        assertEquals("Checked 1 hour ago",
                SafetyCheckViewBinder.getLastRunTimestampText(context, t0, t0 + H_TO_MS));
        assertEquals("Checked 13 hours ago",
                SafetyCheckViewBinder.getLastRunTimestampText(context, t0, t0 + 13 * H_TO_MS));
        assertEquals("Checked yesterday",
                SafetyCheckViewBinder.getLastRunTimestampText(context, t0, t0 + DAY_TO_MS));
        assertEquals("Checked yesterday",
                SafetyCheckViewBinder.getLastRunTimestampText(context, t0, t0 + 2 * DAY_TO_MS - 1));
        assertEquals("Checked 2 days ago",
                SafetyCheckViewBinder.getLastRunTimestampText(context, t0, t0 + 2 * DAY_TO_MS));
        assertEquals("Checked 315 days ago",
                SafetyCheckViewBinder.getLastRunTimestampText(context, t0, t0 + 315 * DAY_TO_MS));
    }

    private void createFragmentAndModel() {
        mSettingsActivityTestRule.startSettingsActivity();
        mFragment = (SafetyCheckSettingsFragment) mSettingsActivityTestRule.getFragment();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mModel = SafetyCheckCoordinator.createModelAndMcp(mFragment); });
    }

    private void createFragmentAndModelByBundle(boolean safetyCheckImmediateRun) {
        mSettingsActivityTestRule.startSettingsActivity(
                SafetyCheckSettingsFragment.createBundle(safetyCheckImmediateRun));
        mFragment = (SafetyCheckSettingsFragment) mSettingsActivityTestRule.getFragment();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mModel = SafetyCheckCoordinator.createModelAndMcp(mFragment); });
    }

    @Test
    @MediumTest
    public void testNullStateDisplayedCorrectly() {
        createFragmentAndModel();
        Preference passwords = mFragment.findPreference(PASSWORDS);
        Preference safeBrowsing = mFragment.findPreference(SAFE_BROWSING);
        Preference updates = mFragment.findPreference(UPDATES);

        assertEquals("", passwords.getSummary());
        assertEquals("", safeBrowsing.getSummary());
        assertEquals("", updates.getSummary());
    }

    @Test
    @MediumTest
    public void testStateChangeDisplayedCorrectly() {
        createFragmentAndModel();
        Preference passwords = mFragment.findPreference(PASSWORDS);
        Preference safeBrowsing = mFragment.findPreference(SAFE_BROWSING);
        Preference updates = mFragment.findPreference(UPDATES);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Passwords state remains unchanged.
            // Safe browsing is in "checking".
            mModel.set(SafetyCheckProperties.SAFE_BROWSING_STATE, SafeBrowsingState.CHECKING);
            // Updates goes through "checking" and ends up in "outdated".
            mModel.set(SafetyCheckProperties.UPDATES_STATE, UpdatesState.CHECKING);
            mModel.set(SafetyCheckProperties.UPDATES_STATE, UpdatesState.OUTDATED);
        });

        assertEquals("", passwords.getSummary());
        assertEquals("", safeBrowsing.getSummary());
        assertEquals(InstrumentationRegistry.getTargetContext().getString(
                             R.string.safety_check_updates_outdated),
                updates.getSummary());
    }

    @Test
    @MediumTest
    public void testSafetyCheckElementsOnClick() {
        createFragmentAndModel();
        CallbackHelper passwordsClicked = new CallbackHelper();
        CallbackHelper safeBrowsingClicked = new CallbackHelper();
        CallbackHelper updatesClicked = new CallbackHelper();
        // Set the listeners
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(SafetyCheckProperties.PASSWORDS_CLICK_LISTENER,
                    (Preference.OnPreferenceClickListener) (p) -> {
                        passwordsClicked.notifyCalled();
                        return true;
                    });
            mModel.set(SafetyCheckProperties.SAFE_BROWSING_CLICK_LISTENER,
                    (Preference.OnPreferenceClickListener) (p) -> {
                        safeBrowsingClicked.notifyCalled();
                        return true;
                    });
            mModel.set(SafetyCheckProperties.UPDATES_CLICK_LISTENER,
                    (Preference.OnPreferenceClickListener) (p) -> {
                        updatesClicked.notifyCalled();
                        return true;
                    });
        });
        Preference passwords = mFragment.findPreference(PASSWORDS);
        Preference safeBrowsing = mFragment.findPreference(SAFE_BROWSING);
        Preference updates = mFragment.findPreference(UPDATES);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Passwords state remains unchanged, should be clickable.
            passwords.performClick();
            // Safe browsing is in "checking", the element deactivates, not clickable.
            mModel.set(SafetyCheckProperties.SAFE_BROWSING_STATE, SafeBrowsingState.CHECKING);
            safeBrowsing.performClick();
            // Updates goes through "checking" and ends up in "outdated".
            // Checking: the element deactivates, clicks are not handled.
            mModel.set(SafetyCheckProperties.UPDATES_STATE, UpdatesState.CHECKING);
            // Final state: the element is reactivated and should handle clicks.
            mModel.set(SafetyCheckProperties.UPDATES_STATE, UpdatesState.OUTDATED);
            updates.performClick();
        });
        // Passwords and updates should get clicked, SB element is inactive.
        assertEquals(1, passwordsClicked.getCallCount());
        assertEquals(0, safeBrowsingClicked.getCallCount());
        assertEquals(1, updatesClicked.getCallCount());
    }

    @Test
    @MediumTest
    public void testSafetyCheckDoNotImmediatelyRunByDefault() {
        createFragmentAndModelByBundle(/*safetyCheckImmediateRun=*/false);
        assertEquals(false, mFragment.shouldRunSafetyCheckImmediately());
        assertEquals(0,
                SharedPreferencesManager.getInstance().readLong(
                        ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP, 0));
    }

    @Test
    @MediumTest
    public void testSafetyCheckImmediatelyRunByBundle() {
        createFragmentAndModelByBundle(/*safetyCheckImmediateRun=*/true);

        // Make sure the safety check was ran.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    SharedPreferencesManager.getInstance().readLong(
                            ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP, 0),
                    Matchers.not(0));
        });
    }
}
