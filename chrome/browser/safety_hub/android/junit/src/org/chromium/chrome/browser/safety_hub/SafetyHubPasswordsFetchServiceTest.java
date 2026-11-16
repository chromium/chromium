// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.experimental.runners.Enclosed;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.FakePasswordCheckupClientHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.safety_hub.SafetyHubPasswordsFetchService.FetchPasswordsCallback;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.test.util.TestAccounts;

import java.util.Arrays;
import java.util.Collection;

/** Unit tests for SafetyHubPasswordsFetchService. */
@RunWith(Enclosed.class)
public class SafetyHubPasswordsFetchServiceTest {

    /**
     * TODO(crbug.com/493130564): Revert to only parameterizing hasAccount after
     * MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS_PART2 launch.
     */
    @RunWith(ParameterizedRobolectricTestRunner.class)
    @Features.EnableFeatures({ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS})
    public static class SafetyHubPasswordsFetchServiceParamTests {
        @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
        private static final String TEST_EMAIL_ADDRESS = "test@email.com";

        /** Returns all possible combinations for test parameterization. */
        @Parameters(name = "{index}_hasAccount={0}_isIdentityMgr={1}")
        public static Collection<Object[]> data() {
            return Arrays.asList(
                    new Object[][] {
                        {true, false},
                        {true, true},
                        {false, false},
                        {false, true}
                    });
        }

        @Rule(order = -2)
        public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

        @Rule public SafetyHubTestRule mSafetyHubTestRule = new SafetyHubTestRule();

        @Rule
        public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

        @Mock private FetchPasswordsCallback mTaskFinishedCallback;

        @Parameter(0)
        public boolean hasAccount;

        @Parameter(1)
        public boolean isIdentityManagerSourceOfAccounts;

        private PrefService mPrefService;
        private FakePasswordCheckupClientHelper mPasswordCheckupClientHelper;
        private PasswordManagerHelper mPasswordManagerHelper;

        @Before
        public void setUp() {
            FeatureOverrides.overrideFlag(
                    SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS_PART2,
                    isIdentityManagerSourceOfAccounts);

            mPrefService = mSafetyHubTestRule.getPrefService();
            mPasswordCheckupClientHelper = mSafetyHubTestRule.getPasswordCheckupClientHelper();
            mPasswordManagerHelper =
                    PasswordManagerHelper.getForProfile(mSafetyHubTestRule.getProfile());
            mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        }

        private SafetyHubPasswordsFetchService createFetchService() {
            return new SafetyHubPasswordsFetchService(
                    mPasswordManagerHelper,
                    mPrefService,
                    getAccount(),
                    mAccountManagerTestRule.getIdentityManager());
        }

        private String getAccount() {
            return hasAccount ? TEST_EMAIL_ADDRESS : null;
        }

        private String getBreachedPreference() {
            return hasAccount
                    ? Pref.BREACHED_CREDENTIALS_COUNT
                    : Pref.LOCAL_BREACHED_CREDENTIALS_COUNT;
        }

        private String getWeakPreference() {
            return hasAccount ? Pref.WEAK_CREDENTIALS_COUNT : Pref.LOCAL_WEAK_CREDENTIALS_COUNT;
        }

        private String getReusedPreference() {
            return hasAccount ? Pref.REUSED_CREDENTIALS_COUNT : Pref.LOCAL_REUSED_CREDENTIALS_COUNT;
        }

        private String getLastCheckTimePreference() {
            return hasAccount
                    ? Pref.LAST_TIME_IN_MS_ACCOUNT_PASSWORD_CHECK_COMPLETED
                    : Pref.LAST_TIME_IN_MS_LOCAL_PASSWORD_CHECK_COMPLETED;
        }

        private void mockLastCheckTime(long timeInMs) {
            doReturn(timeInMs).when(mPrefService).getLong(getLastCheckTimePreference());
        }

        @Test
        public void noPreferencesUpdated_whenUPMDisabled() {
            mSafetyHubTestRule.setPasswordManagerAvailable(false);

            createFetchService().fetchPasswordsCount(mTaskFinishedCallback);

            verify(mPrefService, never()).setInteger(eq(getBreachedPreference()), anyInt());
            verify(mPrefService, never()).setInteger(eq(getWeakPreference()), anyInt());
            verify(mPrefService, never()).setInteger(eq(getReusedPreference()), anyInt());
            verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred= */ true));
        }

        @Test
        public void noPreferencesUpdated_whenFetchFails() {
            mSafetyHubTestRule.setPasswordManagerAvailable(true);
            mPasswordCheckupClientHelper.setError(new Exception());

            createFetchService().fetchPasswordsCount(mTaskFinishedCallback);

            verify(mPrefService, never()).setInteger(eq(getBreachedPreference()), anyInt());
            verify(mPrefService, never()).setInteger(eq(getWeakPreference()), anyInt());
            verify(mPrefService, never()).setInteger(eq(getReusedPreference()), anyInt());
            verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred= */ true));
        }

        @Test
        public void somePreferencesUpdated_fetchFailsForOneCredentialType() {
            mSafetyHubTestRule.setPasswordManagerAvailable(true);
            mPasswordCheckupClientHelper.setWeakCredentialsError(new Exception());
            int breachedCredentialsCount = 5;
            int reusedCredentialsCount = 3;
            mPasswordCheckupClientHelper.setBreachedCredentialsCount(breachedCredentialsCount);
            mPasswordCheckupClientHelper.setReusedCredentialsCount(reusedCredentialsCount);

            createFetchService().fetchPasswordsCount(mTaskFinishedCallback);

            verify(mPrefService, never()).setInteger(eq(getWeakPreference()), anyInt());
            verify(mPrefService, times(1))
                    .setInteger(getBreachedPreference(), breachedCredentialsCount);
            verify(mPrefService, times(1))
                    .setInteger(getReusedPreference(), reusedCredentialsCount);
            verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred= */ true));
        }

        @Test
        public void preferencesUpdated_whenFetchSucceeds() {
            mSafetyHubTestRule.setPasswordManagerAvailable(true);
            int breachedCredentialsCount = 5;
            int weakCredentialsCount = 4;
            int reusedCredentialsCount = 3;
            mPasswordCheckupClientHelper.setBreachedCredentialsCount(breachedCredentialsCount);
            mPasswordCheckupClientHelper.setWeakCredentialsCount(weakCredentialsCount);
            mPasswordCheckupClientHelper.setReusedCredentialsCount(reusedCredentialsCount);

            createFetchService().fetchPasswordsCount(mTaskFinishedCallback);

            verify(mPrefService, times(1))
                    .setInteger(getBreachedPreference(), breachedCredentialsCount);
            verify(mPrefService, times(1)).setInteger(getWeakPreference(), weakCredentialsCount);
            verify(mPrefService, times(1))
                    .setInteger(getReusedPreference(), reusedCredentialsCount);
            verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred= */ false));
        }

        @Test
        public void noPreferencesUpdated_whenCheckupFails_lastCheckRecently() {
            mSafetyHubTestRule.setPasswordManagerAvailable(true);

            long twoHoursInMs = 120 * TimeUtils.MILLISECONDS_PER_MINUTE;
            mockLastCheckTime(TimeUtils.currentTimeMillis() - twoHoursInMs);
            mPasswordCheckupClientHelper.setError(new Exception());

            createFetchService().runPasswordCheckup(mTaskFinishedCallback);

            verify(mPrefService, never()).clearPref(eq(getBreachedPreference()));
            verify(mPrefService, never()).clearPref(eq(getWeakPreference()));
            verify(mPrefService, never()).clearPref(eq(getReusedPreference()));

            verify(mPrefService, never()).setInteger(eq(getBreachedPreference()), anyInt());
            verify(mPrefService, never()).setInteger(eq(getWeakPreference()), anyInt());
            verify(mPrefService, never()).setInteger(eq(getReusedPreference()), anyInt());

            verify(mPrefService, never()).setLong(eq(getLastCheckTimePreference()), anyLong());
            verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred= */ true));
        }

        @Test
        public void noPreferencesUpdated_whenCheckupFails_lastCheckLongAgo() {
            mSafetyHubTestRule.setPasswordManagerAvailable(true);

            long twoDaysInMs = 2 * TimeUtils.MILLISECONDS_PER_DAY;
            mockLastCheckTime(TimeUtils.currentTimeMillis() - twoDaysInMs);
            mPasswordCheckupClientHelper.setError(new Exception());

            createFetchService().runPasswordCheckup(mTaskFinishedCallback);

            verify(mPrefService, times(1)).clearPref(getBreachedPreference());
            verify(mPrefService, times(1)).clearPref(getWeakPreference());
            verify(mPrefService, times(1)).clearPref(getReusedPreference());

            verify(mPrefService, never()).setInteger(eq(getBreachedPreference()), anyInt());
            verify(mPrefService, never()).setInteger(eq(getWeakPreference()), anyInt());
            verify(mPrefService, never()).setInteger(eq(getReusedPreference()), anyInt());

            verify(mPrefService, never()).setLong(eq(getLastCheckTimePreference()), anyLong());
            verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred= */ true));
        }

        @Test
        public void preferencesUpdated_whenCheckupSucceeds() {
            mSafetyHubTestRule.setPasswordManagerAvailable(true);
            int breachedCredentialsCount = 5;
            int weakCredentialsCount = 4;
            int reusedCredentialsCount = 3;
            mPasswordCheckupClientHelper.setBreachedCredentialsCount(breachedCredentialsCount);
            mPasswordCheckupClientHelper.setWeakCredentialsCount(weakCredentialsCount);
            mPasswordCheckupClientHelper.setReusedCredentialsCount(reusedCredentialsCount);

            long twoDaysInMs = 2 * TimeUtils.MILLISECONDS_PER_DAY;
            mockLastCheckTime(TimeUtils.currentTimeMillis() - twoDaysInMs);

            assertTrue(createFetchService().runPasswordCheckup(mTaskFinishedCallback));

            verify(mPrefService, times(1))
                    .setInteger(getBreachedPreference(), breachedCredentialsCount);
            verify(mPrefService, times(1)).setInteger(getWeakPreference(), weakCredentialsCount);
            verify(mPrefService, times(1))
                    .setInteger(getReusedPreference(), reusedCredentialsCount);
            verify(mPrefService, times(1)).setLong(eq(getLastCheckTimePreference()), anyLong());
            verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred= */ false));
        }

        @Test
        public void noPreferencesUpdated_whenWithinCoolDownPeriod() {
            mSafetyHubTestRule.setPasswordManagerAvailable(true);

            mockLastCheckTime(TimeUtils.currentTimeMillis());

            assertFalse(createFetchService().runPasswordCheckup(mTaskFinishedCallback));

            verify(mPrefService, never()).setInteger(eq(getBreachedPreference()), anyInt());
            verify(mPrefService, never()).setInteger(eq(getWeakPreference()), anyInt());
            verify(mPrefService, never()).setInteger(eq(getReusedPreference()), anyInt());
            verify(mPrefService, never()).setLong(eq(getLastCheckTimePreference()), anyLong());
            verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred= */ false));
        }
    }

    /**
     * TODO(crbug.com/493130564): Revert to regular runner after
     * MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS_PART2 launch.
     */
    @RunWith(ParameterizedRobolectricTestRunner.class)
    @Features.EnableFeatures({ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS})
    public static class SafetyHubPasswordsFetchServiceSingleTests {
        @Parameters(name = "{index}_isIdentityMgr={0}")
        public static Collection<Object> data() {
            return Arrays.asList(false, true);
        }

        @Rule(order = -2)
        public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

        @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
        @Rule public SafetyHubTestRule mSafetyHubTestRule = new SafetyHubTestRule();

        @Rule
        public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

        @Mock private FetchPasswordsCallback mTaskFinishedCallback;

        @Parameter(0)
        public boolean isIdentityManagerSourceOfAccounts;

        private PrefService mPrefService;
        private FakePasswordCheckupClientHelper mPasswordCheckupClientHelper;
        private PasswordManagerHelper mPasswordManagerHelper;

        @Before
        public void setUp() {
            FeatureOverrides.overrideFlag(
                    SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS_PART2,
                    isIdentityManagerSourceOfAccounts);

            mPrefService = mSafetyHubTestRule.getPrefService();
            mPasswordCheckupClientHelper = mSafetyHubTestRule.getPasswordCheckupClientHelper();
            mPasswordManagerHelper =
                    PasswordManagerHelper.getForProfile(mSafetyHubTestRule.getProfile());
            mockLastCheckTime(0);
        }

        private SafetyHubPasswordsFetchService createLocalFetchService() {
            return new SafetyHubPasswordsFetchService(
                    mPasswordManagerHelper,
                    mPrefService,
                    /* account= */ null,
                    mAccountManagerTestRule.getIdentityManager());
        }

        private void mockLastCheckTime(long timeInMs) {
            doReturn(timeInMs)
                    .when(mPrefService)
                    .getLong(Pref.LAST_TIME_IN_MS_LOCAL_PASSWORD_CHECK_COMPLETED);
        }

        @Test
        public void noPreferencesUpdated_whenNoAccountsOnDevice_lastCheckRecently() {
            mSafetyHubTestRule.setPasswordManagerAvailable(true);
            long twoHoursInMs = 120 * TimeUtils.MILLISECONDS_PER_MINUTE;
            mockLastCheckTime(TimeUtils.currentTimeMillis() - twoHoursInMs);

            assertFalse(createLocalFetchService().runPasswordCheckup(mTaskFinishedCallback));

            verify(mPrefService, never()).clearPref(eq(Pref.LOCAL_BREACHED_CREDENTIALS_COUNT));
            verify(mPrefService, never()).clearPref(eq(Pref.LOCAL_WEAK_CREDENTIALS_COUNT));
            verify(mPrefService, never()).clearPref(eq(Pref.LOCAL_REUSED_CREDENTIALS_COUNT));

            verify(mPrefService, never())
                    .setInteger(eq(Pref.LOCAL_BREACHED_CREDENTIALS_COUNT), anyInt());
            verify(mPrefService, never())
                    .setInteger(eq(Pref.LOCAL_WEAK_CREDENTIALS_COUNT), anyInt());
            verify(mPrefService, never())
                    .setInteger(eq(Pref.LOCAL_REUSED_CREDENTIALS_COUNT), anyInt());
            verify(mPrefService, never())
                    .setLong(eq(Pref.LAST_TIME_IN_MS_LOCAL_PASSWORD_CHECK_COMPLETED), anyLong());

            verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred= */ true));
        }

        @Test
        public void noPreferencesUpdated_whenNoAccountsOnDevice_lastCheckLongAgo() {
            mSafetyHubTestRule.setPasswordManagerAvailable(true);

            long twoDaysInMs = 2 * TimeUtils.MILLISECONDS_PER_DAY;
            mockLastCheckTime(TimeUtils.currentTimeMillis() - twoDaysInMs);

            assertFalse(createLocalFetchService().runPasswordCheckup(mTaskFinishedCallback));

            verify(mPrefService, times(1)).clearPref(Pref.LOCAL_BREACHED_CREDENTIALS_COUNT);
            verify(mPrefService, times(1)).clearPref(Pref.LOCAL_WEAK_CREDENTIALS_COUNT);
            verify(mPrefService, times(1)).clearPref(Pref.LOCAL_REUSED_CREDENTIALS_COUNT);

            verify(mPrefService, never())
                    .setInteger(eq(Pref.LOCAL_BREACHED_CREDENTIALS_COUNT), anyInt());
            verify(mPrefService, never())
                    .setInteger(eq(Pref.LOCAL_WEAK_CREDENTIALS_COUNT), anyInt());
            verify(mPrefService, never())
                    .setInteger(eq(Pref.LOCAL_REUSED_CREDENTIALS_COUNT), anyInt());

            verify(mPrefService, never())
                    .setLong(eq(Pref.LAST_TIME_IN_MS_LOCAL_PASSWORD_CHECK_COMPLETED), anyLong());

            verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred= */ true));
        }

        @Test
        public void preferencesUpdated_whenAccountsLoadingInProgress() {
            mSafetyHubTestRule.setPasswordManagerAvailable(true);
            int breachedCredentialsCount = 5;
            int weakCredentialsCount = 4;
            int reusedCredentialsCount = 3;
            mPasswordCheckupClientHelper.setBreachedCredentialsCount(breachedCredentialsCount);
            mPasswordCheckupClientHelper.setWeakCredentialsCount(weakCredentialsCount);
            mPasswordCheckupClientHelper.setReusedCredentialsCount(reusedCredentialsCount);

            long twoDaysInMs = 2 * TimeUtils.MILLISECONDS_PER_DAY;
            mockLastCheckTime(TimeUtils.currentTimeMillis() - twoDaysInMs);

            try (var ignored = mAccountManagerTestRule.blockGetAccountsUpdate()) {
                assertTrue(createLocalFetchService().runPasswordCheckup(mTaskFinishedCallback));
            }

            verify(mPrefService, times(1))
                    .setInteger(Pref.LOCAL_BREACHED_CREDENTIALS_COUNT, breachedCredentialsCount);
            verify(mPrefService, times(1))
                    .setInteger(Pref.LOCAL_WEAK_CREDENTIALS_COUNT, weakCredentialsCount);
            verify(mPrefService, times(1))
                    .setInteger(Pref.LOCAL_REUSED_CREDENTIALS_COUNT, reusedCredentialsCount);
            verify(mPrefService, times(1))
                    .setLong(eq(Pref.LAST_TIME_IN_MS_LOCAL_PASSWORD_CHECK_COMPLETED), anyLong());
            verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred= */ false));
        }

        @Test
        public void preferencesUpdated_whenAccountsFetchFailed() {
            mSafetyHubTestRule.setPasswordManagerAvailable(true);
            int breachedCredentialsCount = 5;
            int weakCredentialsCount = 4;
            int reusedCredentialsCount = 3;
            mPasswordCheckupClientHelper.setBreachedCredentialsCount(breachedCredentialsCount);
            mPasswordCheckupClientHelper.setWeakCredentialsCount(weakCredentialsCount);
            mPasswordCheckupClientHelper.setReusedCredentialsCount(reusedCredentialsCount);

            long twoDaysInMs = 2 * TimeUtils.MILLISECONDS_PER_DAY;
            mockLastCheckTime(TimeUtils.currentTimeMillis() - twoDaysInMs);
            mAccountManagerTestRule.setAccountFetchFailed();

            assertTrue(createLocalFetchService().runPasswordCheckup(mTaskFinishedCallback));

            verify(mPrefService, times(1))
                    .setInteger(Pref.LOCAL_BREACHED_CREDENTIALS_COUNT, breachedCredentialsCount);
            verify(mPrefService, times(1))
                    .setInteger(Pref.LOCAL_WEAK_CREDENTIALS_COUNT, weakCredentialsCount);
            verify(mPrefService, times(1))
                    .setInteger(Pref.LOCAL_REUSED_CREDENTIALS_COUNT, reusedCredentialsCount);
            verify(mPrefService, times(1))
                    .setLong(eq(Pref.LAST_TIME_IN_MS_LOCAL_PASSWORD_CHECK_COMPLETED), anyLong());
            verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred= */ false));
        }

        @Test
        public void preferencesUpdated_whenAccountOnDevice() {
            mSafetyHubTestRule.setPasswordManagerAvailable(true);
            int breachedCredentialsCount = 5;
            int weakCredentialsCount = 4;
            int reusedCredentialsCount = 3;
            mPasswordCheckupClientHelper.setBreachedCredentialsCount(breachedCredentialsCount);
            mPasswordCheckupClientHelper.setWeakCredentialsCount(weakCredentialsCount);
            mPasswordCheckupClientHelper.setReusedCredentialsCount(reusedCredentialsCount);

            long twoDaysInMs = 2 * TimeUtils.MILLISECONDS_PER_DAY;
            mockLastCheckTime(TimeUtils.currentTimeMillis() - twoDaysInMs);
            mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

            assertTrue(createLocalFetchService().runPasswordCheckup(mTaskFinishedCallback));

            verify(mPrefService, times(1))
                    .setInteger(Pref.LOCAL_BREACHED_CREDENTIALS_COUNT, breachedCredentialsCount);
            verify(mPrefService, times(1))
                    .setInteger(Pref.LOCAL_WEAK_CREDENTIALS_COUNT, weakCredentialsCount);
            verify(mPrefService, times(1))
                    .setInteger(Pref.LOCAL_REUSED_CREDENTIALS_COUNT, reusedCredentialsCount);
            verify(mPrefService, times(1))
                    .setLong(eq(Pref.LAST_TIME_IN_MS_LOCAL_PASSWORD_CHECK_COMPLETED), anyLong());
            verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred= */ false));
        }
    }
}
