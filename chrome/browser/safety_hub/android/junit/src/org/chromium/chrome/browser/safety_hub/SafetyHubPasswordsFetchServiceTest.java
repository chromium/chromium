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
import org.mockito.MockitoAnnotations;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.FakePasswordCheckupClientHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.prefs.PrefService;

import java.util.Arrays;
import java.util.Collection;

/** Unit tests for SafetyHubPasswordsFetchService. */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Features.EnableFeatures({
    ChromeFeatureList.SAFETY_HUB,
    ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS
})
// TODO(crbug.com/390442009): Update the tests when the logic starts taking the flag into account
// explicitly. For now the flag is checked in PasswordManagerHelper which gets indirectly
// invoked by these tests.
@Features.DisableFeatures(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID)
public class SafetyHubPasswordsFetchServiceTest {
    private static final String TEST_EMAIL_ADDRESS = "test@email.com";

    @Parameters
    public static Collection<Object> data() {
        return Arrays.asList(new Object[] {true, false});
    }

    @Parameter public boolean hasAccount;

    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Rule public SafetyHubTestRule mSafetyHubTestRule = new SafetyHubTestRule();

    @Mock private Callback<Boolean> mTaskFinishedCallback;

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
        return hasAccount ? Pref.BREACHED_CREDENTIALS_COUNT : Pref.LOCAL_BREACHED_CREDENTIALS_COUNT;
    }

    private String getWeakPreference() {
        return hasAccount ? Pref.WEAK_CREDENTIALS_COUNT : Pref.LOCAL_WEAK_CREDENTIALS_COUNT;
    }

    private String getReusedPreference() {
        return hasAccount ? Pref.REUSED_CREDENTIALS_COUNT : Pref.LOCAL_REUSED_CREDENTIALS_COUNT;
    }

    @Test
    public void noPreferencesUpdated_whenUPMDisabled() {
        mSafetyHubTestRule.setUPMStatus(false);

        new SafetyHubPasswordsFetchService(mPasswordManagerHelper, mPrefService, getAccount())
                .fetchPasswordsCount(mTaskFinishedCallback);

        verify(mPrefService, never()).setInteger(eq(getBreachedPreference()), anyInt());
        verify(mPrefService, never()).setInteger(eq(getWeakPreference()), anyInt());
        verify(mPrefService, never()).setInteger(eq(getReusedPreference()), anyInt());
        verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred */ true));
    }

    @Test
    public void noPreferencesUpdated_whenFetchFails() {
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
        verify(mPrefService, times(1)).setInteger(getReusedPreference(), reusedCredentialsCount);
        verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred */ true));
    }

    @Test
    public void preferencesUpdated_whenFetchSucceeds() {
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
        verify(mPrefService, times(1)).setInteger(getReusedPreference(), reusedCredentialsCount);
        verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred */ false));
    }

    @Test
    public void noPreferencesUpdated_whenCheckupFails() {
        mPasswordCheckupClientHelper.setError(new Exception());

        new SafetyHubPasswordsFetchService(mPasswordManagerHelper, mPrefService, getAccount())
                .runPasswordCheckup(mTaskFinishedCallback);

        verify(mPrefService, never()).setInteger(eq(getBreachedPreference()), anyInt());
        verify(mPrefService, never()).setInteger(eq(getWeakPreference()), anyInt());
        verify(mPrefService, never()).setInteger(eq(getReusedPreference()), anyInt());
        verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred= */ true));
    }

    @Test
    public void preferencesUpdated_whenCheckupSucceeds() {
        int breachedCredentialsCount = 5;
        int weakCredentialsCount = 4;
        int reusedCredentialsCount = 3;
        mPasswordCheckupClientHelper.setBreachedCredentialsCount(breachedCredentialsCount);
        mPasswordCheckupClientHelper.setWeakCredentialsCount(weakCredentialsCount);
        mPasswordCheckupClientHelper.setReusedCredentialsCount(reusedCredentialsCount);

        new SafetyHubPasswordsFetchService(mPasswordManagerHelper, mPrefService, getAccount())
                .runPasswordCheckup(mTaskFinishedCallback);

        verify(mPrefService, times(1))
                .setInteger(getBreachedPreference(), breachedCredentialsCount);
        verify(mPrefService, times(1)).setInteger(getWeakPreference(), weakCredentialsCount);
        verify(mPrefService, times(1)).setInteger(getReusedPreference(), reusedCredentialsCount);
        verify(mTaskFinishedCallback, times(1)).onResult(eq(/* errorOccurred= */ false));
    }
}
