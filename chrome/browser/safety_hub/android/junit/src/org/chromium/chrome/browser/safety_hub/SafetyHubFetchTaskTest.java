// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskParameters;

/** Unit tests for SafetyHubFetchTask. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyHubFetchTaskTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private Context mContext;
    @Mock private Profile mProfile;
    @Mock private BackgroundTask.TaskFinishedCallback mTaskFinishedCallback;
    @Mock private SafetyHubFetchService mSafetyHubFetchService;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        ProfileManager.setLastUsedProfileForTesting(mProfile);

        SafetyHubFetchServiceFactory.setSafetyHubFetchServiceForTesting(mSafetyHubFetchService);
        doAnswer(
                        (Answer<Void>)
                                invocation -> {
                                    ((Callback<Boolean>) invocation.getArgument(0)).onResult(true);
                                    return null;
                                })
                .when(mSafetyHubFetchService)
                .fetchBreachedCredentialsCount(any());
    }

    @Test
    public void testOnStartTaskBeforeNativeLoaded() {
        TaskParameters params = TaskParameters.create(TaskIds.SAFETY_HUB_JOB_ID).build();

        int result =
                new SafetyHubFetchTask()
                        .onStartTaskBeforeNativeLoaded(mContext, params, mTaskFinishedCallback);

        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.LOAD_NATIVE, result);
        verify(mSafetyHubFetchService, never()).fetchBreachedCredentialsCount(any());
        verify(mTaskFinishedCallback, never()).taskFinished(anyBoolean());
    }

    @Test
    public void testOnStartTaskWithNative() {
        TaskParameters params = TaskParameters.create(TaskIds.SAFETY_HUB_JOB_ID).build();

        new SafetyHubFetchTask().onStartTaskWithNative(mContext, params, mTaskFinishedCallback);

        verify(mSafetyHubFetchService, times(1)).fetchBreachedCredentialsCount(any());
        verify(mTaskFinishedCallback, times(1)).taskFinished(anyBoolean());
    }

    @Test
    public void testOnStopTaskBeforeNativeLoaded() {
        TaskParameters params = TaskParameters.create(TaskIds.SAFETY_HUB_JOB_ID).build();

        boolean shouldReschedule =
                new SafetyHubFetchTask().onStopTaskBeforeNativeLoaded(mContext, params);

        assertTrue(shouldReschedule);
    }

    @Test
    public void testOnStopTaskWithNative() {
        TaskParameters params = TaskParameters.create(TaskIds.SAFETY_HUB_JOB_ID).build();

        boolean shouldReschedule = new SafetyHubFetchTask().onStopTaskWithNative(mContext, params);

        assertFalse(shouldReschedule);
    }
}
