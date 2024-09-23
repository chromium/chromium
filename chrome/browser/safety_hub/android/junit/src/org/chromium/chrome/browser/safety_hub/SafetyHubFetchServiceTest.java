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

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
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

import java.util.concurrent.TimeUnit;

/** Unit tests for SafetyHubFetchService. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyHubFetchServiceTest {
    private static final int ONE_DAY_IN_MILLISECONDS = (int) TimeUnit.DAYS.toMillis(1);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public SafetyHubTestRule mSafetyHubTestRule = new SafetyHubTestRule();

    @Mock private BackgroundTaskScheduler mTaskScheduler;
    @Mock private Callback<Boolean> mTaskFinishedCallback;
    @Captor private ArgumentCaptor<TaskInfo> mTaskInfoCaptor;

    private Profile mProfile;
    private PrefService mPrefService;
    private FakePasswordCheckupClientHelper mPasswordCheckupClientHelper;

    @Before
    public void setUp() {
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mTaskScheduler);
        mProfile = mSafetyHubTestRule.getProfile();
        mPrefService = mSafetyHubTestRule.getPrefService();
        mPasswordCheckupClientHelper = mSafetyHubTestRule.getPasswordCheckupClientHelper();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void testTaskScheduledImmediately_WhenConditionsMet() {
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
    @Features.DisableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void testTaskCancelled_WhenConditionsNotMet() {
        new SafetyHubFetchService(mProfile).onForegroundSessionStart();

        // Verify prefs are cleaned up when task is cancelled.
        verify(mPrefService, times(1)).clearPref(Pref.BREACHED_CREDENTIALS_COUNT);
        verify(mTaskScheduler, times(1)).cancel(any(), eq(TaskIds.SAFETY_HUB_JOB_ID));
        verify(mTaskScheduler, never()).schedule(any(), mTaskInfoCaptor.capture());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void testTaskCancelled_WhenSigninStatusChanged_SignOut() {
        mSafetyHubTestRule.setSignedInState(false);

        new SafetyHubFetchService(mProfile).onSignedOut();

        // Verify prefs are cleaned up when task is cancelled.
        verify(mPrefService, times(1)).clearPref(Pref.BREACHED_CREDENTIALS_COUNT);
        verify(mTaskScheduler, times(1)).cancel(any(), eq(TaskIds.SAFETY_HUB_JOB_ID));
        verify(mTaskScheduler, never()).schedule(any(), any());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void testTaskScheduled_WhenSigninStatusChanged_SignIn() {
        mSafetyHubTestRule.setSignedInState(true);

        new SafetyHubFetchService(mProfile).onSignedIn();

        verify(mTaskScheduler, times(1)).schedule(any(), mTaskInfoCaptor.capture());
        TaskInfo taskInfo = mTaskInfoCaptor.getValue();
        assertEquals(TaskIds.SAFETY_HUB_JOB_ID, taskInfo.getTaskId());
        assertEquals(0, ((TaskInfo.OneOffInfo) taskInfo.getTimingInfo()).getWindowStartTimeMs());
        assertEquals(0, ((TaskInfo.OneOffInfo) taskInfo.getTimingInfo()).getWindowEndTimeMs());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void testTaskCancelled_WhenUPMDisabled() {
        mSafetyHubTestRule.setUPMStatus(false);
        new SafetyHubFetchService(mProfile).onForegroundSessionStart();

        // Verify prefs are cleaned up when task is cancelled.
        verify(mPrefService, times(1)).clearPref(Pref.BREACHED_CREDENTIALS_COUNT);
        verify(mTaskScheduler, times(1)).cancel(any(), eq(TaskIds.SAFETY_HUB_JOB_ID));
        verify(mTaskScheduler, never()).schedule(any(), mTaskInfoCaptor.capture());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void testTaskRescheduled_whenFetchFails() {
        mPasswordCheckupClientHelper.setError(new Exception());

        new SafetyHubFetchService(mProfile).fetchBreachedCredentialsCount(mTaskFinishedCallback);

        verify(mPrefService, never()).setInteger(eq(Pref.BREACHED_CREDENTIALS_COUNT), anyInt());
        verify(mTaskFinishedCallback, times(1)).onResult(eq(/* needsReschedule= */ true));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void testNextTaskScheduled_WhenFetchSucceeds() {
        int breachedCredentialsCount = 5;
        mPasswordCheckupClientHelper.setBreachedCredentialsCount(breachedCredentialsCount);

        new SafetyHubFetchService(mProfile).fetchBreachedCredentialsCount(mTaskFinishedCallback);

        verify(mPrefService, times(1))
                .setInteger(Pref.BREACHED_CREDENTIALS_COUNT, breachedCredentialsCount);
        verify(mTaskFinishedCallback, times(1)).onResult(eq(/* needsReschedule= */ false));

        // Check previous job is cleaned up.
        verify(mTaskScheduler, times(1)).cancel(any(), eq(TaskIds.SAFETY_HUB_JOB_ID));

        // Check next job is scheduled after the specified period.
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
}
