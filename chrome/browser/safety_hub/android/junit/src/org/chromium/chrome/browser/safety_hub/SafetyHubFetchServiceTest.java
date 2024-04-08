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
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.CollectionUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.FakePasswordCheckupClientHelper;
import org.chromium.chrome.browser.password_manager.FakePasswordCheckupClientHelperFactoryImpl;
import org.chromium.chrome.browser.password_manager.FakePasswordManagerBackendSupportHelper;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelperFactory;
import org.chromium.chrome.browser.password_manager.PasswordManagerBackendSupportHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.HashSet;
import java.util.concurrent.TimeUnit;

/** Unit tests for SafetyHubFetchService. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyHubFetchServiceTest {
    private static final int ONE_DAY_IN_MILLISECONDS = (int) TimeUnit.DAYS.toMillis(1);
    private static final String TEST_EMAIL_ADDRESS = "test@email.com";

    @Rule public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private SyncService mSyncService;
    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;
    @Mock private UserPrefs.Natives mUserPrefsNatives;
    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeNativeMock;
    @Mock private BackgroundTaskScheduler mTaskScheduler;
    @Mock private Context mContext;
    @Mock private BackgroundTask.TaskFinishedCallback mTaskFinishedCallback;
    @Captor private ArgumentCaptor<TaskInfo> mTaskInfoCaptor;

    @Spy FakePasswordCheckupClientHelper mPasswordCheckupClientHelper;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNatives);
        mJniMocker.mock(
                PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeNativeMock);

        ProfileManager.setLastUsedProfileForTesting(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefService);

        SyncServiceFactory.setInstanceForTesting(mSyncService);
        setPasswordSync(true);
        setUpPasswordManagerBackendForTesting();
        setUPMStatus(true);

        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mTaskScheduler);
    }

    private void setPasswordSync(boolean isSyncing) {
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(isSyncing);
        when(mSyncService.getSelectedTypes())
                .thenReturn(
                        isSyncing
                                ? CollectionUtil.newHashSet(UserSelectableType.PASSWORDS)
                                : new HashSet<>());
        when(mSyncService.getAccountInfo())
                .thenReturn(CoreAccountInfo.createFromEmailAndGaiaId(TEST_EMAIL_ADDRESS, "0"));
    }

    private void setUPMStatus(boolean isUPMEnabled) {
        when(mPasswordManagerUtilBridgeNativeMock.shouldUseUpmWiring(true, mPrefService))
                .thenReturn(isUPMEnabled);
    }

    private void setUpPasswordManagerBackendForTesting() {
        FakePasswordManagerBackendSupportHelper helper =
                new FakePasswordManagerBackendSupportHelper();
        helper.setBackendPresent(true);
        PasswordManagerBackendSupportHelper.setInstanceForTesting(helper);

        setUpFakePasswordCheckupClientHelper();
    }

    private void setUpFakePasswordCheckupClientHelper() {
        FakePasswordCheckupClientHelperFactoryImpl passwordCheckupClientHelperFactory =
                new FakePasswordCheckupClientHelperFactoryImpl();
        mPasswordCheckupClientHelper =
                (FakePasswordCheckupClientHelper) passwordCheckupClientHelperFactory.createHelper();
        PasswordCheckupClientHelperFactory.setFactoryForTesting(passwordCheckupClientHelperFactory);
    }

    @Test
    public void testTaskRescheduled_whenSyncDisabled() {
        setPasswordSync(false);
        TaskParameters params = TaskParameters.create(TaskIds.SAFETY_HUB_JOB_ID).build();

        new SafetyHubFetchService().onStartTask(mContext, params, mTaskFinishedCallback);

        verify(mPrefService, never()).setInteger(eq(Pref.BREACHED_CREDENTIALS_COUNT), anyInt());
        verify(mTaskFinishedCallback, times(1)).taskFinished(eq(/* needsReschedule= */ true));
    }

    @Test
    public void testTaskRescheduled_whenUPMDisabled() {
        setUPMStatus(false);
        TaskParameters params = TaskParameters.create(TaskIds.SAFETY_HUB_JOB_ID).build();

        new SafetyHubFetchService().onStartTask(mContext, params, mTaskFinishedCallback);

        verify(mPrefService, never()).setInteger(eq(Pref.BREACHED_CREDENTIALS_COUNT), anyInt());
        verify(mTaskFinishedCallback, times(1)).taskFinished(eq(/* needsReschedule= */ true));
    }

    @Test
    public void testTaskRescheduled_whenFetchFails() {
        TaskParameters params = TaskParameters.create(TaskIds.SAFETY_HUB_JOB_ID).build();
        mPasswordCheckupClientHelper.setError(new Exception());

        new SafetyHubFetchService().onStartTask(mContext, params, mTaskFinishedCallback);

        verify(mPrefService, never()).setInteger(eq(Pref.BREACHED_CREDENTIALS_COUNT), anyInt());
        verify(mTaskFinishedCallback, times(1)).taskFinished(eq(/* needsReschedule= */ true));
    }

    @Test
    public void testStartTask_FetchSucceeds() {
        int breachedCredentialsCount = 5;
        TaskParameters params = TaskParameters.create(TaskIds.SAFETY_HUB_JOB_ID).build();
        mPasswordCheckupClientHelper.setBreachedCredentialsCount(breachedCredentialsCount);

        new SafetyHubFetchService().onStartTask(mContext, params, mTaskFinishedCallback);

        verify(mPrefService, times(1))
                .setInteger(Pref.BREACHED_CREDENTIALS_COUNT, breachedCredentialsCount);
        verify(mTaskFinishedCallback, times(1)).taskFinished(eq(/* needsReschedule= */ false));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void onSessionStart_WithSafetyHubEnabled_SchedulesTask() {
        SafetyHubFetchService.onForegroundSessionStart();
        verify(mTaskScheduler, times(1)).schedule(any(), mTaskInfoCaptor.capture());
        TaskInfo taskInfo = mTaskInfoCaptor.getValue();
        assertEquals(TaskIds.SAFETY_HUB_JOB_ID, taskInfo.getTaskId());
        assertTrue(taskInfo.isPeriodic());
        assertTrue(taskInfo.isPersisted());
        assertFalse(taskInfo.shouldUpdateCurrent());
        assertEquals(
                ONE_DAY_IN_MILLISECONDS,
                ((TaskInfo.PeriodicInfo) taskInfo.getTimingInfo()).getIntervalMs());
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void onSessionStart_WithSafetyHubDisabled_CancelsTask() {
        SafetyHubFetchService.onForegroundSessionStart();
        verify(mTaskScheduler, times(1)).cancel(any(), eq(TaskIds.SAFETY_HUB_JOB_ID));
    }
}
