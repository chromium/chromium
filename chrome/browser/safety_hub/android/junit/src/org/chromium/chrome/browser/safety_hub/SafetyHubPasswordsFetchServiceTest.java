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
import org.mockito.MockitoAnnotations;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.FakePasswordCheckupClientHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;

/** Unit tests for SafetyHubPasswordsFetchService. */
@RunWith(Enclosed.class)
public class SafetyHubPasswordsFetchServiceTest {
    @RunWith(ParameterizedRobolectricTestRunner.class)
    @Batch(Batch.UNIT_TESTS)
    @Features.EnableFeatures({ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS})
    public static class SafetyHubPasswordsFetchServiceParamTests {
        private static final String TEST_EMAIL_ADDRESS = "test@email.com";

        /** Returns all possible combinations for test parameterization. */
        @Parameters
        public static Collection<Object> data() {
            return Arrays.asList(new Object[] {true, false});
        }

        @Rule(order = -2)
        public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

        @Rule public SafetyHubTestRule mSafetyHubTestRule = new SafetyHubTestRule();

        @Mock private Callback<Boolean> mTaskFinishedCallback;

        @Parameter(0)
        public boolean hasAccount;

        private PrefService mPrefService;
        private FakePasswordCheckupClientHelper mPasswordCheckupClientHelper;
        private PasswordManagerHelper mPasswordManagerHelper;

        @Before
        public void setUp() {
            // Needed because of BaseRobolectricTestRule.
            MockitoAnnotations.openMocks(this);

            mPrefService = mSafetyHubTestRule.getPrefService();
            mPasswordCheckupClientHelper = mSafetyHubTestRule.getPasswordCheckupClientHelper();
            mPasswordManagerHelper =
                    PasswordManagerHelper.getForProfile(mSafetyHubTestRule.getProfile());
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

            new SafetyHubPasswordsFetchService(mPasswordManagerHelper, mPrefService, getAccount())
                    .fetchPasswordsCount(mTaskFinishedCallback);

            verify(mPrefService, never()).setInteger(eq(getBreachedPreference()), anyInt());
            verify(mPrefService, never()).setInteger(eq(getWeakPreference()), anyInt());
            verify(mPrefService, never()).setInteger(eq(getReusedPreference()), anyInt());
            verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred */ true));
        }

        @Test
        public void noPreferencesUpdated_whenFetchFails() {
            mSafetyHubTestRule.setPasswordManagerAvailable(true);
            mPasswordCheckupClientHelper.setError(new Exception());

            new SafetyHubPasswordsFetchService(mPasswordManagerHelper, mPrefService, getAccount())
                    .fetchPasswordsCount(mTaskFinishedCallback);

            verify(mPrefService, never()).setInteger(eq(getBreachedPreference()), anyInt());
            verify(mPrefService, never()).setInteger(eq(getWeakPreference()), anyInt());
            verify(mPrefService, never()).setInteger(eq(getReusedPreference()), anyInt());
            verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred */ true));
        }

        @Test
        public void somePreferencesUpdated_fetchFailsForOneCredentialType() {
            mSafetyHubTestRule.setPasswordManagerAvailable(true);
            mPasswordCheckupClientHelper.setWeakCredentialsError(new Exception());
            int breachedCredentialsCount = 5;
            int reusedCredentialsCount = 3;
            mPasswordCheckupClientHelper.setBreachedCredentialsCount(breachedCredentialsCount);
            mPasswordCheckupClientHelper.setReusedCredentialsCount(reusedCredentialsCount);

            new SafetyHubPasswordsFetchService(mPasswordManagerHelper, mPrefService, getAccount())
                    .fetchPasswordsCount(mTaskFinishedCallback);

            verify(mPrefService, never()).setInteger(eq(getWeakPreference()), anyInt());
            verify(mPrefService, times(1))
                    .setInteger(getBreachedPreference(), breachedCredentialsCount);
            verify(mPrefService, times(1))
                    .setInteger(getReusedPreference(), reusedCredentialsCount);
            verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred */ true));
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

            new SafetyHubPasswordsFetchService(mPasswordManagerHelper, mPrefService, getAccount())
                    .fetchPasswordsCount(mTaskFinishedCallback);

            verify(mPrefService, times(1))
                    .setInteger(getBreachedPreference(), breachedCredentialsCount);
            verify(mPrefService, times(1)).setInteger(getWeakPreference(), weakCredentialsCount);
            verify(mPrefService, times(1))
                    .setInteger(getReusedPreference(), reusedCredentialsCount);
            verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred */ false));
        }

        @Test
        public void noPreferencesUpdated_whenCheckupFails_lastCheckRecently() {
            mSafetyHubTestRule.setPasswordManagerAvailable(true);

            long twoHoursInMs = 120 * TimeUtils.MILLISECONDS_PER_MINUTE;
            mockLastCheckTime(TimeUtils.currentTimeMillis() - twoHoursInMs);
            mPasswordCheckupClientHelper.setError(new Exception());

            new SafetyHubPasswordsFetchService(mPasswordManagerHelper, mPrefService, getAccount())
                    .runPasswordCheckup(mTaskFinishedCallback);

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

            new SafetyHubPasswordsFetchService(mPasswordManagerHelper, mPrefService, getAccount())
                    .runPasswordCheckup(mTaskFinishedCallback);

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

            assertTrue(
                    new SafetyHubPasswordsFetchService(
                                    mPasswordManagerHelper, mPrefService, getAccount())
                            .runPasswordCheckup(mTaskFinishedCallback));

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

            assertFalse(
                    new SafetyHubPasswordsFetchService(
                                    mPasswordManagerHelper, mPrefService, getAccount())
                            .runPasswordCheckup(mTaskFinishedCallback));

            verify(mPrefService, never()).setInteger(eq(getBreachedPreference()), anyInt());
            verify(mPrefService, never()).setInteger(eq(getWeakPreference()), anyInt());
            verify(mPrefService, never()).setInteger(eq(getReusedPreference()), anyInt());
            verify(mPrefService, never()).setLong(eq(getLastCheckTimePreference()), anyLong());
            verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred= */ false));
        }
    }

    @RunWith(BaseRobolectricTestRunner.class)
    @Batch(Batch.UNIT_TESTS)
    @Features.EnableFeatures({ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS})
    public static class SafetyHubPasswordsFetchServiceSingleTests {
        @Rule public SafetyHubTestRule mSafetyHubTestRule = new SafetyHubTestRule();

        @Mock private Callback<Boolean> mTaskFinishedCallback;
        @Mock private AccountManagerFacade mMockFacade;

        private PrefService mPrefService;
        private FakePasswordCheckupClientHelper mPasswordCheckupClientHelper;
        private PasswordManagerHelper mPasswordManagerHelper;

        @Before
        public void setUp() {
            MockitoAnnotations.openMocks(this);

            mPrefService = mSafetyHubTestRule.getPrefService();
            mPasswordCheckupClientHelper = mSafetyHubTestRule.getPasswordCheckupClientHelper();
            mPasswordManagerHelper =
                    PasswordManagerHelper.getForProfile(mSafetyHubTestRule.getProfile());
            mockLastCheckTime(0);
            doReturn(Promise.fulfilled(new ArrayList<>())).when(mMockFacade).getAccounts();
            doReturn(true).when(mMockFacade).didAccountFetchSucceed();

            AccountManagerFacadeProvider.setInstanceForTests(mMockFacade);
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

            assertFalse(
                    new SafetyHubPasswordsFetchService(mPasswordManagerHelper, mPrefService, null)
                            .runPasswordCheckup(mTaskFinishedCallback));

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

            assertFalse(
                    new SafetyHubPasswordsFetchService(mPasswordManagerHelper, mPrefService, null)
                            .runPasswordCheckup(mTaskFinishedCallback));

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
    }
}
