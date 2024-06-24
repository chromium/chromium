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

import org.chromium.base.Callback;
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
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelperJni;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
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
    @Mock private PasswordManagerHelper.Natives mPasswordManagerHelperNativeMock;
    @Mock private BackgroundTaskScheduler mTaskScheduler;
    @Mock private Callback<Boolean> mTaskFinishedCallback;
    @Captor private ArgumentCaptor<TaskInfo> mTaskInfoCaptor;

    @Spy FakePasswordCheckupClientHelper mPasswordCheckupClientHelper;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNatives);
        mJniMocker.mock(
                PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeNativeMock);
        mJniMocker.mock(PasswordManagerHelperJni.TEST_HOOKS, mPasswordManagerHelperNativeMock);

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
        when(mPasswordManagerHelperNativeMock.hasChosenToSyncPasswords(mSyncService))
                .thenReturn(isSyncing);
    }

    private void setUPMStatus(boolean isUPMEnabled) {
        when(mPasswordManagerUtilBridgeNativeMock.areMinUpmRequirementsMet())
                .thenReturn(isUPMEnabled);
        when(mPasswordManagerUtilBridgeNativeMock.shouldUseUpmWiring(mSyncService, mPrefService))
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
        verify(mPrefService, times(1)).setInteger(Pref.BREACHED_CREDENTIALS_COUNT, 0);
        verify(mTaskScheduler, times(1)).cancel(any(), eq(TaskIds.SAFETY_HUB_JOB_ID));
        verify(mTaskScheduler, never()).schedule(any(), mTaskInfoCaptor.capture());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void testTaskCancelled_WhenSyncStatusChanged_SyncDisabled() {
        setPasswordSync(false);

        new SafetyHubFetchService(mProfile).syncStateChanged();

        // Verify prefs are cleaned up when task is cancelled.
        verify(mPrefService, times(1)).setInteger(Pref.BREACHED_CREDENTIALS_COUNT, 0);
        verify(mTaskScheduler, times(1)).cancel(any(), eq(TaskIds.SAFETY_HUB_JOB_ID));
        verify(mTaskScheduler, never()).schedule(any(), any());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void testTaskScheduled_WhenSyncStatusChanged_SyncEnabled() {
        setPasswordSync(true);

        new SafetyHubFetchService(mProfile).syncStateChanged();

        verify(mTaskScheduler, times(1)).schedule(any(), mTaskInfoCaptor.capture());
        TaskInfo taskInfo = mTaskInfoCaptor.getValue();
        assertEquals(TaskIds.SAFETY_HUB_JOB_ID, taskInfo.getTaskId());
        assertEquals(0, ((TaskInfo.OneOffInfo) taskInfo.getTimingInfo()).getWindowStartTimeMs());
        assertEquals(0, ((TaskInfo.OneOffInfo) taskInfo.getTimingInfo()).getWindowEndTimeMs());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void testTaskCancelled_WhenUPMDisabled() {
        setUPMStatus(false);
        new SafetyHubFetchService(mProfile).onForegroundSessionStart();

        // Verify prefs are cleaned up when task is cancelled.
        verify(mPrefService, times(1)).setInteger(Pref.BREACHED_CREDENTIALS_COUNT, 0);
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
