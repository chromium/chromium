// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;

import org.chromium.base.Callback;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.FakePasswordCheckupClientHelper;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.prefs.PrefService;

import java.util.Arrays;
import java.util.Collection;
import java.util.concurrent.TimeUnit;

/** Unit tests for SafetyHubFetchService. */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class SafetyHubFetchServiceTest {
    private static final int ONE_DAY_IN_MILLISECONDS = (int) TimeUnit.DAYS.toMillis(1);

    @Parameters
    public static Collection testCases() {
        return Arrays.asList(
                /* isLoginDbDeprecationEnabled= */ true, /* isLoginDbDeprecationEnabled= */ false);
    }

    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public SafetyHubTestRule mSafetyHubTestRule = new SafetyHubTestRule();

    @Parameter public boolean mIsLoginDbDeprecationEnabled;

    @Mock private BackgroundTaskScheduler mTaskScheduler;
    @Mock private Callback<Boolean> mTaskFinishedCallback;
    @Captor private ArgumentCaptor<TaskInfo> mTaskInfoCaptor;

    private Profile mProfile;
    private PrefService mPrefService;
    private FakePasswordCheckupClientHelper mPasswordCheckupClientHelper;

    @Before
    public void setUp() {
        if (mIsLoginDbDeprecationEnabled) {
            FeatureOverrides.enable(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID);
        } else {
            FeatureOverrides.disable(ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID);
        }
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mTaskScheduler);
        mProfile = mSafetyHubTestRule.getProfile();
        mPrefService = mSafetyHubTestRule.getPrefService();
        mPasswordCheckupClientHelper = mSafetyHubTestRule.getPasswordCheckupClientHelper();
        mSafetyHubTestRule.setSignedInState(true);
    }

    // Check next job is scheduled after the specified period.
    private void checkNextJobIsScheduled() {
        verify(mTaskScheduler, times(1)).schedule(any(), mTaskInfoCaptor.capture());
        TaskInfo taskInfo = mTaskInfoCaptor.getValue();
        assertEquals(TaskIds.SAFETY_HUB_JOB_ID, taskInfo.getTaskId());
        assertTrue(taskInfo.isPersisted());
        assertFalse(taskInfo.shouldUpdateCurrent());
        assertEquals(
                ONE_DAY_IN_MILLISECONDS,
                ((TaskInfo.OneOffInfo) taskInfo.getTimingInfo()).getWindowStartTimeMs());
        assertEquals(
                ONE_DAY_IN_MILLISECONDS,
                ((TaskInfo.OneOffInfo) taskInfo.getTimingInfo()).getWindowEndTimeMs());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void testAccountPasswordsFetchJobScheduledImmediately_WhenConditionsMet() {
        mSafetyHubTestRule.setPasswordManagerAvailable(true, mIsLoginDbDeprecationEnabled);

        new SafetyHubFetchService(mProfile).onForegroundSessionStart();

        verify(mTaskScheduler, times(1)).schedule(any(), mTaskInfoCaptor.capture());
        TaskInfo taskInfo = mTaskInfoCaptor.getValue();
        assertEquals(TaskIds.SAFETY_HUB_JOB_ID, taskInfo.getTaskId());
        assertTrue(taskInfo.isPersisted());
        assertFalse(taskInfo.shouldUpdateCurrent());
        assertEquals(0, ((TaskInfo.OneOffInfo) taskInfo.getTimingInfo()).getWindowStartTimeMs());
        assertEquals(0, ((TaskInfo.OneOffInfo) taskInfo.getTimingInfo()).getWindowEndTimeMs());
    }

    @Test
    @Features.DisableFeatures({
        ChromeFeatureList.SAFETY_HUB,
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS
    })
    public void testAccountPasswordsFetchJobCancelled_WhenFlagDisabled() {
        mSafetyHubTestRule.setPasswordManagerAvailable(true, mIsLoginDbDeprecationEnabled);

        new SafetyHubFetchService(mProfile).onForegroundSessionStart();

        // Verify prefs are cleaned up when task is cancelled.
        verify(mPrefService, times(1)).clearPref(Pref.BREACHED_CREDENTIALS_COUNT);
        verify(mPrefService, times(1)).clearPref(Pref.WEAK_CREDENTIALS_COUNT);
        verify(mPrefService, times(1)).clearPref(Pref.REUSED_CREDENTIALS_COUNT);
        verify(mTaskScheduler, times(1)).cancel(any(), eq(TaskIds.SAFETY_HUB_JOB_ID));
        verify(mTaskScheduler, never()).schedule(any(), mTaskInfoCaptor.capture());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void testAccountPasswordsFetchJobCancelled_WhenSigninStatusChanged_SignOut() {
        mSafetyHubTestRule.setPasswordManagerAvailable(true, mIsLoginDbDeprecationEnabled);

        SafetyHubFetchService fetchService = new SafetyHubFetchService(mProfile);
        mSafetyHubTestRule.setSignedInState(false);
        fetchService.onSignedOut();

        // Verify prefs are cleaned up when task is cancelled.
        verify(mPrefService, times(1)).clearPref(Pref.BREACHED_CREDENTIALS_COUNT);
        verify(mPrefService, times(1)).clearPref(Pref.WEAK_CREDENTIALS_COUNT);
        verify(mPrefService, times(1)).clearPref(Pref.REUSED_CREDENTIALS_COUNT);
        verify(mTaskScheduler, times(1)).cancel(any(), eq(TaskIds.SAFETY_HUB_JOB_ID));
        verify(mTaskScheduler, never()).schedule(any(), any());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void testAccountPasswordsFetchJobScheduled_WhenSigninStatusChanged_SignIn() {
        mSafetyHubTestRule.setPasswordManagerAvailable(true, mIsLoginDbDeprecationEnabled);

        mSafetyHubTestRule.setSignedInState(true);

        new SafetyHubFetchService(mProfile).onSignedIn();

        verify(mTaskScheduler, times(1)).schedule(any(), mTaskInfoCaptor.capture());
        TaskInfo taskInfo = mTaskInfoCaptor.getValue();
        assertEquals(TaskIds.SAFETY_HUB_JOB_ID, taskInfo.getTaskId());
        assertEquals(0, ((TaskInfo.OneOffInfo) taskInfo.getTimingInfo()).getWindowStartTimeMs());
        assertEquals(0, ((TaskInfo.OneOffInfo) taskInfo.getTimingInfo()).getWindowEndTimeMs());
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB,
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
    })
    public void testAccountPasswordsFetchJobCancelled_WhenPasswordManagerNotAvailable() {
        mSafetyHubTestRule.setSignedInState(true);
        mSafetyHubTestRule.setPasswordManagerAvailable(false, mIsLoginDbDeprecationEnabled);

        new SafetyHubFetchService(mProfile).onForegroundSessionStart();

        // Verify prefs are cleaned up when task is cancelled.
        verify(mPrefService, times(1)).clearPref(Pref.BREACHED_CREDENTIALS_COUNT);
        verify(mPrefService, times(1)).clearPref(Pref.WEAK_CREDENTIALS_COUNT);
        verify(mPrefService, times(1)).clearPref(Pref.REUSED_CREDENTIALS_COUNT);
        verify(mTaskScheduler, times(1)).cancel(any(), eq(TaskIds.SAFETY_HUB_JOB_ID));
        verify(mTaskScheduler, never()).schedule(any(), mTaskInfoCaptor.capture());
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB,
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS
    })
    public void testAccountPasswordsFetchJobRescheduled_whenFetchFails() {
        mSafetyHubTestRule.setPasswordManagerAvailable(true, mIsLoginDbDeprecationEnabled);

        mPasswordCheckupClientHelper.setError(new Exception());

        new SafetyHubFetchService(mProfile).fetchAccountCredentialsCount(mTaskFinishedCallback);

        verify(mPrefService, never()).setInteger(eq(Pref.BREACHED_CREDENTIALS_COUNT), anyInt());
        verify(mPrefService, never()).setInteger(eq(Pref.WEAK_CREDENTIALS_COUNT), anyInt());
        verify(mPrefService, never()).setInteger(eq(Pref.REUSED_CREDENTIALS_COUNT), anyInt());
        verify(mTaskFinishedCallback, times(1)).onResult(eq(/* needsReschedule= */ true));
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB,
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS
    })
    public void testAccountPasswordsFetchJobRescheduled_whenFetchFailsForOneCredentialType() {
        mSafetyHubTestRule.setPasswordManagerAvailable(true, mIsLoginDbDeprecationEnabled);

        mPasswordCheckupClientHelper.setWeakCredentialsError(new Exception());
        int breachedCredentialsCount = 5;
        int reusedCredentialsCount = 3;
        mPasswordCheckupClientHelper.setBreachedCredentialsCount(breachedCredentialsCount);
        mPasswordCheckupClientHelper.setReusedCredentialsCount(reusedCredentialsCount);

        new SafetyHubFetchService(mProfile).fetchAccountCredentialsCount(mTaskFinishedCallback);

        verify(mPrefService, never()).setInteger(eq(Pref.WEAK_CREDENTIALS_COUNT), anyInt());
        verify(mPrefService, times(1))
                .setInteger(Pref.BREACHED_CREDENTIALS_COUNT, breachedCredentialsCount);
        verify(mPrefService, times(1))
                .setInteger(Pref.REUSED_CREDENTIALS_COUNT, reusedCredentialsCount);
        verify(mTaskFinishedCallback, times(1)).onResult(eq(/* needsReschedule= */ true));
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB,
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS
    })
    public void testAccountPasswordsNextTaskScheduled_whenFetchSucceeds() {
        mSafetyHubTestRule.setPasswordManagerAvailable(true, mIsLoginDbDeprecationEnabled);

        int breachedCredentialsCount = 5;
        int weakCredentialsCount = 4;
        int reusedCredentialsCount = 3;
        mPasswordCheckupClientHelper.setBreachedCredentialsCount(breachedCredentialsCount);
        mPasswordCheckupClientHelper.setWeakCredentialsCount(weakCredentialsCount);
        mPasswordCheckupClientHelper.setReusedCredentialsCount(reusedCredentialsCount);

        new SafetyHubFetchService(mProfile).fetchAccountCredentialsCount(mTaskFinishedCallback);

        verify(mPrefService, times(1))
                .setInteger(Pref.BREACHED_CREDENTIALS_COUNT, breachedCredentialsCount);
        verify(mPrefService, times(1))
                .setInteger(Pref.WEAK_CREDENTIALS_COUNT, weakCredentialsCount);
        verify(mPrefService, times(1))
                .setInteger(Pref.REUSED_CREDENTIALS_COUNT, reusedCredentialsCount);
        verify(mTaskFinishedCallback, times(1)).onResult(eq(/* needsReschedule= */ false));

        // Check previous job is cleaned up.
        verify(mTaskScheduler, times(1)).cancel(any(), eq(TaskIds.SAFETY_HUB_JOB_ID));

        // Check next job is scheduled after the specified period.
        checkNextJobIsScheduled();
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB,
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE
    })
    public void testLocalPasswordsFetch_whenFetchFails() {
        mSafetyHubTestRule.setPasswordManagerAvailable(true, mIsLoginDbDeprecationEnabled);

        mPasswordCheckupClientHelper.setError(new Exception());

        new SafetyHubFetchService(mProfile).fetchLocalCredentialsCount();

        verify(mPrefService, never())
                .setInteger(eq(Pref.LOCAL_BREACHED_CREDENTIALS_COUNT), anyInt());
        verify(mPrefService, never()).setInteger(eq(Pref.LOCAL_WEAK_CREDENTIALS_COUNT), anyInt());
        verify(mPrefService, never()).setInteger(eq(Pref.LOCAL_REUSED_CREDENTIALS_COUNT), anyInt());
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB,
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE
    })
    public void testLocalPasswordsFetch_whenFetchFailsForOneCredentialType() {
        mSafetyHubTestRule.setPasswordManagerAvailable(true, mIsLoginDbDeprecationEnabled);

        mPasswordCheckupClientHelper.setWeakCredentialsError(new Exception());
        int breachedCredentialsCount = 5;
        int reusedCredentialsCount = 3;
        mPasswordCheckupClientHelper.setBreachedCredentialsCount(breachedCredentialsCount);
        mPasswordCheckupClientHelper.setReusedCredentialsCount(reusedCredentialsCount);

        new SafetyHubFetchService(mProfile).fetchLocalCredentialsCount();

        verify(mPrefService, never()).setInteger(eq(Pref.LOCAL_WEAK_CREDENTIALS_COUNT), anyInt());
        verify(mPrefService, times(1))
                .setInteger(Pref.LOCAL_BREACHED_CREDENTIALS_COUNT, breachedCredentialsCount);
        verify(mPrefService, times(1))
                .setInteger(Pref.LOCAL_REUSED_CREDENTIALS_COUNT, reusedCredentialsCount);
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB,
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE
    })
    public void testLocalPasswordsFetch_whenFetchSucceeds() {
        mSafetyHubTestRule.setPasswordManagerAvailable(true, mIsLoginDbDeprecationEnabled);

        int breachedCredentialsCount = 5;
        int weakCredentialsCount = 4;
        int reusedCredentialsCount = 3;
        mPasswordCheckupClientHelper.setBreachedCredentialsCount(breachedCredentialsCount);
        mPasswordCheckupClientHelper.setWeakCredentialsCount(weakCredentialsCount);
        mPasswordCheckupClientHelper.setReusedCredentialsCount(reusedCredentialsCount);

        new SafetyHubFetchService(mProfile).fetchLocalCredentialsCount();

        verify(mPrefService, times(1))
                .setInteger(Pref.LOCAL_BREACHED_CREDENTIALS_COUNT, breachedCredentialsCount);
        verify(mPrefService, times(1))
                .setInteger(Pref.LOCAL_WEAK_CREDENTIALS_COUNT, weakCredentialsCount);
        verify(mPrefService, times(1))
                .setInteger(Pref.LOCAL_REUSED_CREDENTIALS_COUNT, reusedCredentialsCount);
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB,
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE
    })
    public void testLocalPasswordsCheckup_whenFetchFails() {
        mSafetyHubTestRule.setPasswordManagerAvailable(true, mIsLoginDbDeprecationEnabled);

        mPasswordCheckupClientHelper.setError(new Exception());

        new SafetyHubFetchService(mProfile).runLocalPasswordCheckup();

        verify(mPrefService, never()).setInteger(eq(Pref.BREACHED_CREDENTIALS_COUNT), anyInt());
        verify(mPrefService, never()).setInteger(eq(Pref.WEAK_CREDENTIALS_COUNT), anyInt());
        verify(mPrefService, never()).setInteger(eq(Pref.REUSED_CREDENTIALS_COUNT), anyInt());
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB,
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE
    })
    public void testLocalPasswordsCheckup_whenFetchFailsForOneCredentialType() {
        mSafetyHubTestRule.setPasswordManagerAvailable(true, mIsLoginDbDeprecationEnabled);
        mSafetyHubTestRule.setSignedInState(false);

        mPasswordCheckupClientHelper.setWeakCredentialsError(new Exception());
        int breachedCredentialsCount = 5;
        int reusedCredentialsCount = 3;
        mPasswordCheckupClientHelper.setBreachedCredentialsCount(breachedCredentialsCount);
        mPasswordCheckupClientHelper.setReusedCredentialsCount(reusedCredentialsCount);

        new SafetyHubFetchService(mProfile).runLocalPasswordCheckup();

        verify(mPrefService, never()).setInteger(eq(Pref.LOCAL_WEAK_CREDENTIALS_COUNT), anyInt());
        verify(mPrefService, times(1))
                .setInteger(Pref.LOCAL_BREACHED_CREDENTIALS_COUNT, breachedCredentialsCount);
        verify(mPrefService, times(1))
                .setInteger(Pref.LOCAL_REUSED_CREDENTIALS_COUNT, reusedCredentialsCount);
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB,
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE
    })
    public void testLocalPasswordsCheckup_whenCheckupFails() {
        mSafetyHubTestRule.setPasswordManagerAvailable(true, mIsLoginDbDeprecationEnabled);

        mPasswordCheckupClientHelper.setError(new Exception());

        new SafetyHubFetchService(mProfile).runLocalPasswordCheckup();

        verify(mPrefService, never())
                .setInteger(eq(Pref.LOCAL_BREACHED_CREDENTIALS_COUNT), anyInt());
        verify(mPrefService, never()).setInteger(eq(Pref.LOCAL_WEAK_CREDENTIALS_COUNT), anyInt());
        verify(mPrefService, never()).setInteger(eq(Pref.LOCAL_REUSED_CREDENTIALS_COUNT), anyInt());
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB,
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
        ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE
    })
    public void testLocalPasswordsCheckup_whenCheckupSucceeds() {
        mSafetyHubTestRule.setPasswordManagerAvailable(true, mIsLoginDbDeprecationEnabled);
        mSafetyHubTestRule.setSignedInState(false);

        int breachedCredentialsCount = 5;
        int weakCredentialsCount = 4;
        int reusedCredentialsCount = 3;
        mPasswordCheckupClientHelper.setBreachedCredentialsCount(breachedCredentialsCount);
        mPasswordCheckupClientHelper.setWeakCredentialsCount(weakCredentialsCount);
        mPasswordCheckupClientHelper.setReusedCredentialsCount(reusedCredentialsCount);

        new SafetyHubFetchService(mProfile).runLocalPasswordCheckup();

        verify(mPrefService, times(1))
                .setInteger(Pref.LOCAL_BREACHED_CREDENTIALS_COUNT, breachedCredentialsCount);
        verify(mPrefService, times(1))
                .setInteger(Pref.LOCAL_WEAK_CREDENTIALS_COUNT, weakCredentialsCount);
        verify(mPrefService, times(1))
                .setInteger(Pref.LOCAL_REUSED_CREDENTIALS_COUNT, reusedCredentialsCount);
    }
}
