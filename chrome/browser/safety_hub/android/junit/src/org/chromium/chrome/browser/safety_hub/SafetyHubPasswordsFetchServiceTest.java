// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.FakePasswordCheckupClientHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.prefs.PrefService;

/** Unit tests for SafetyHubPasswordsFetchService. */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({
    ChromeFeatureList.SAFETY_HUB,
    ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS
})
// TODO(crbug.com/390442009): Update the tests when the logic starts taking the flag into account
// explicitly. For now the flag is checked in PasswordManagerHelper which gets indirectly
// invoked by these tests.
@Features.DisableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
public class SafetyHubPasswordsFetchServiceTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public SafetyHubTestRule mSafetyHubTestRule = new SafetyHubTestRule();

    @Mock private Callback<Boolean> mTaskFinishedCallback;

    private PrefService mPrefService;
    private FakePasswordCheckupClientHelper mPasswordCheckupClientHelper;
    private PasswordManagerHelper mPasswordManagerHelper;

    @Before
    public void setUp() {
        mPrefService = mSafetyHubTestRule.getPrefService();
        mPasswordCheckupClientHelper = mSafetyHubTestRule.getPasswordCheckupClientHelper();
        mPasswordManagerHelper =
                PasswordManagerHelper.getForProfile(mSafetyHubTestRule.getProfile());
    }

    @Test
    public void noPreferencesUpdated_whenUPMDisabled() {
        mSafetyHubTestRule.setUPMStatus(false);

        new SafetyHubPasswordsFetchService(mPasswordManagerHelper, mPrefService, null)
                .fetchPasswordsCount(mTaskFinishedCallback);

        verify(mPrefService, never()).setInteger(eq(Pref.BREACHED_CREDENTIALS_COUNT), anyInt());
        verify(mPrefService, never()).setInteger(eq(Pref.WEAK_CREDENTIALS_COUNT), anyInt());
        verify(mPrefService, never()).setInteger(eq(Pref.REUSED_CREDENTIALS_COUNT), anyInt());
        verify(mTaskFinishedCallback, times(1)).onResult(eq(/* onCheckupFinishedCallback= */ true));
    }

    @Test
    public void noPreferencesUpdated_whenFetchFails() {
        mPasswordCheckupClientHelper.setError(new Exception());

        new SafetyHubPasswordsFetchService(mPasswordManagerHelper, mPrefService, null)
                .fetchPasswordsCount(mTaskFinishedCallback);

        verify(mPrefService, never()).setInteger(eq(Pref.BREACHED_CREDENTIALS_COUNT), anyInt());
        verify(mPrefService, never()).setInteger(eq(Pref.WEAK_CREDENTIALS_COUNT), anyInt());
        verify(mPrefService, never()).setInteger(eq(Pref.REUSED_CREDENTIALS_COUNT), anyInt());
        verify(mTaskFinishedCallback, times(1)).onResult(eq(/* onCheckupFinishedCallback= */ true));
    }

    @Test
    public void somePreferencesUpdated_fetchFailsForOneCredentialType() {
        mPasswordCheckupClientHelper.setWeakCredentialsError(new Exception());
        int breachedCredentialsCount = 5;
        int reusedCredentialsCount = 3;
        mPasswordCheckupClientHelper.setBreachedCredentialsCount(breachedCredentialsCount);
        mPasswordCheckupClientHelper.setReusedCredentialsCount(reusedCredentialsCount);

        new SafetyHubPasswordsFetchService(mPasswordManagerHelper, mPrefService, null)
                .fetchPasswordsCount(mTaskFinishedCallback);

        verify(mPrefService, never()).setInteger(eq(Pref.WEAK_CREDENTIALS_COUNT), anyInt());
        verify(mPrefService, times(1))
                .setInteger(Pref.BREACHED_CREDENTIALS_COUNT, breachedCredentialsCount);
        verify(mPrefService, times(1))
                .setInteger(Pref.REUSED_CREDENTIALS_COUNT, reusedCredentialsCount);
        verify(mTaskFinishedCallback, times(1)).onResult(eq(/* onCheckupFinishedCallback= */ true));
    }

    @Test
    public void preferencesUpdated_whenFetchSucceeds() {
        int breachedCredentialsCount = 5;
        int weakCredentialsCount = 4;
        int reusedCredentialsCount = 3;
        mPasswordCheckupClientHelper.setBreachedCredentialsCount(breachedCredentialsCount);
        mPasswordCheckupClientHelper.setWeakCredentialsCount(weakCredentialsCount);
        mPasswordCheckupClientHelper.setReusedCredentialsCount(reusedCredentialsCount);

        new SafetyHubPasswordsFetchService(mPasswordManagerHelper, mPrefService, null)
                .fetchPasswordsCount(mTaskFinishedCallback);

        verify(mPrefService, times(1))
                .setInteger(Pref.BREACHED_CREDENTIALS_COUNT, breachedCredentialsCount);
        verify(mPrefService, times(1))
                .setInteger(Pref.WEAK_CREDENTIALS_COUNT, weakCredentialsCount);
        verify(mPrefService, times(1))
                .setInteger(Pref.REUSED_CREDENTIALS_COUNT, reusedCredentialsCount);
        verify(mTaskFinishedCallback, times(1))
                .onResult(eq(/* onCheckupFinishedCallback= */ false));
    }
}
