// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Notification;
import android.content.Intent;
import android.content.ServiceConnection;

import androidx.core.app.ServiceCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.url.GURL;

import java.util.Collections;

/** Unit tests for {@link ActorForegroundServiceControllerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActorForegroundServiceControllerImplTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActorBackgroundActuationManager mMockBackgroundManager;
    @Mock private ActorForegroundServiceImpl mServiceImpl;
    @Mock private ActorForegroundServiceImpl.LocalBinder mBinder;
    @Mock private Notification mNotification;
    @Mock private ActorTask mActorTask;
    @Mock private AsyncInitializationActivity mChromeActivity;
    @Mock private SettingsActivity mSettingsActivity;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Tab mTab;

    private ActorForegroundServiceControllerImpl mController;
    private ShadowApplication mShadowApplication;

    @Before
    public void setUp() {
        mController = new ActorForegroundServiceControllerImpl();
        mShadowApplication = shadowOf(RuntimeEnvironment.getApplication());
        when(mBinder.getService()).thenReturn(mServiceImpl);
        ApplicationStatus.destroyForJUnitTests();
        ApplicationStatus.initialize(RuntimeEnvironment.getApplication());
        TabModelSelectorSupplier.setInstanceForTesting(mTabModelSelector);
    }

    @Test
    public void testStartAndBindService() throws Exception {
        CallbackHelper connectedCallback = new CallbackHelper();
        mController.startAndBindService(connectedCallback::notifyCalled);

        // Verify service was started
        Intent startedIntent = mShadowApplication.getNextStartedService();
        assertEquals(
                "Service class name should match.",
                ActorForegroundService.class.getName(),
                startedIntent.getComponent().getClassName());

        // Simulate service connection
        ServiceConnection connection = mController.getServiceConnectionForTesting();
        connection.onServiceConnected(null, mBinder);
        connectedCallback.waitForOnly();
        assertTrue(
                "Controller should be connected after onServiceConnected.",
                mController.isConnected());
    }

    @Test
    public void testOnServiceDisconnected() throws Exception {
        mController.startAndBindService(() -> {});
        ServiceConnection connection = mController.getServiceConnectionForTesting();
        connection.onServiceConnected(null, mBinder);
        assertTrue("Controller should be connected.", mController.isConnected());

        connection.onServiceDisconnected(null);
        assertFalse("Controller should be disconnected.", mController.isConnected());
    }

    @Test
    public void testProxyMethods() {
        mController.startAndBindService(() -> {});
        mController.getServiceConnectionForTesting().onServiceConnected(null, mBinder);

        mController.startOrUpdateForegroundService(
                /* newNotificationId= */ 1,
                mNotification,
                /* oldNotificationId= */ 2,
                /* killOldNotification= */ true);
        verify(mServiceImpl)
                .startOrUpdateForegroundService(
                        /* newNotificationId= */ 1,
                        mNotification,
                        /* oldNotificationId= */ 2,
                        /* killOldNotification= */ true);

        mController.stopActorForegroundService(/* flags= */ ServiceCompat.STOP_FOREGROUND_REMOVE);
        verify(mServiceImpl)
                .stopActorForegroundService(/* flags= */ ServiceCompat.STOP_FOREGROUND_REMOVE);
    }

    @Test
    public void testUnbindService() {
        mController.startAndBindService(() -> {});
        mController.getServiceConnectionForTesting().onServiceConnected(null, mBinder);
        assertTrue("Controller should be connected.", mController.isConnected());

        mController.unbindService();
        assertFalse("Controller should be disconnected after unbind.", mController.isConnected());
    }

    @Test
    public void testGetReturnsSingletonInstance() {
        ActorForegroundServiceController.setInstanceForTesting(null);
        ActorForegroundServiceController controller1 = ActorForegroundServiceController.get();
        ActorForegroundServiceController controller2 = ActorForegroundServiceController.get();
        assertSame("get() should return the cached singleton instance.", controller1, controller2);
    }

    @Test
    public void testTransitionActiveTasksToBackground_NotConnected_DoesNothing() {
        mController.setBackgroundManagerForTesting(mMockBackgroundManager);
        mController.transitionActiveTasksToBackground(mTabModelSelector);
        verify(mMockBackgroundManager, never()).transitionActiveTasksToBackground(any());
    }

    @Test
    public void testTransitionActiveTasksToBackground_Connected_DelegatesToManager() {
        mController.startAndBindService(() -> {});
        mController.getServiceConnectionForTesting().onServiceConnected(null, mBinder);
        mController.setBackgroundManagerForTesting(mMockBackgroundManager);

        mController.transitionActiveTasksToBackground(mTabModelSelector);
        verify(mMockBackgroundManager).transitionActiveTasksToBackground(mTabModelSelector);
    }

    @Test
    public void testOnMessageTriggerTaskStopped_DelegatesToBackgroundManager() {
        mController.setBackgroundManagerForTesting(mMockBackgroundManager);
        mController.onMessageTriggerTaskStopped("test_context_id");
        verify(mMockBackgroundManager).cleanupContext("test_context_id");
    }

    @Test
    public void testOnMessageTriggerTaskStopped_NullBackgroundManager_DoesNotCrash() {
        mController.setBackgroundManagerForTesting(null);
        mController.onMessageTriggerTaskStopped("test_context_id");
        verify(mMockBackgroundManager, never()).cleanupContext("test_context_id");
    }

    @Test
    public void testCreateTrustedBringTabToFrontIntent() {
        int tabId = 123;
        int taskId = 456;
        int taskState = ActorTaskState.ACTING;
        when(mActorTask.getId()).thenReturn(taskId);
        when(mActorTask.getState()).thenReturn(taskState);
        when(mActorTask.getLastActuatedTabId()).thenReturn(tabId);

        Intent intent = mController.createTrustedBringTabToFrontIntent(mActorTask);
        assertNotNull("Intent should not be null.", intent);
        assertEquals(
                "Intent extra should contain the correct tabId.",
                tabId,
                IntentHandler.getBringTabToFrontId(intent));
        assertTrue(
                "Intent should have EXTRA_SHOW_ACTOR_CONTROL.",
                intent.getBooleanExtra(ActorNotificationFactory.EXTRA_SHOW_ACTOR_CONTROL, false));
        assertEquals(
                "Intent should have the correct taskId.",
                taskId,
                intent.getIntExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, -1));
        assertEquals(
                "Intent should have the correct task state.",
                taskState,
                intent.getIntExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, -1));
    }

    @Test
    public void testCreateTrustedBringTabToFrontIntent_FinishedTaskWithValidTabId() {
        int tabId = 123;
        int taskId = 456;
        int taskState = ActorTaskState.FINISHED;
        when(mActorTask.getId()).thenReturn(taskId);
        when(mActorTask.getState()).thenReturn(taskState);
        when(mActorTask.getLastActuatedTabId()).thenReturn(tabId);

        Intent intent = mController.createTrustedBringTabToFrontIntent(mActorTask);
        assertNotNull("Intent should not be null.", intent);
        assertEquals(
                "Intent extra should contain the preserved tabId after task completion.",
                tabId,
                IntentHandler.getBringTabToFrontId(intent));
        assertTrue(
                "Intent should have EXTRA_SHOW_ACTOR_CONTROL.",
                intent.getBooleanExtra(ActorNotificationFactory.EXTRA_SHOW_ACTOR_CONTROL, false));
        assertEquals(
                "Intent should have the correct taskId.",
                taskId,
                intent.getIntExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, -1));
        assertEquals(
                "Intent should have the correct task state.",
                taskState,
                intent.getIntExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, -1));
    }

    @Test
    public void testCreateTrustedBringTabToFrontIntent_InvalidTabId() {
        int taskId = 456;
        int taskState = ActorTaskState.FINISHED;
        when(mActorTask.getId()).thenReturn(taskId);
        when(mActorTask.getState()).thenReturn(taskState);
        when(mActorTask.getLastActuatedTabId()).thenReturn(Tab.INVALID_TAB_ID);

        Intent intent = mController.createTrustedBringTabToFrontIntent(mActorTask);
        assertNotNull("Intent should not be null.", intent);
        assertEquals(
                "Intent extra should contain INVALID_TAB_ID for invalid tab ID.",
                Tab.INVALID_TAB_ID,
                IntentHandler.getBringTabToFrontId(intent));
        assertTrue(
                "Intent should have EXTRA_SHOW_ACTOR_CONTROL.",
                intent.getBooleanExtra(ActorNotificationFactory.EXTRA_SHOW_ACTOR_CONTROL, false));
        assertEquals(
                "Intent should have the correct taskId.",
                taskId,
                intent.getIntExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, -1));
        assertEquals(
                "Intent should have the correct task state.",
                taskState,
                intent.getIntExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, -1));
    }

    @Test
    public void testIsActivityVisibleForTabs_NoActivities() {
        assertFalse(mController.isActivityVisibleForTabs(Collections.emptySet()));
    }

    @Test
    public void testIsActivityVisibleForTabs_WithTabs_SilencesWhenTabInActivity() {
        int tabId = 123;
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.RESUMED);
        when(mTabModelSelector.getTabById(tabId)).thenReturn(mTab);

        assertTrue(mController.isActivityVisibleForTabs(Collections.singleton(tabId)));
    }

    @Test
    public void testIsActivityVisibleForTabs_WithTabs_NoSilenceWhenTabNotInActivity() {
        int tabId = 123;
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.CREATED);
        when(mTabModelSelector.getTabById(tabId)).thenReturn(null);

        assertFalse(mController.isActivityVisibleForTabs(Collections.singleton(tabId)));
    }

    @Test
    public void testIsActivityVisibleForTabs_NoSilenceWhenInPiP() {
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.CREATED);
        when(mChromeActivity.isInPictureInPictureMode()).thenReturn(true);

        assertFalse(mController.isActivityVisibleForTabs(Collections.singleton(123)));
    }

    @Test
    public void testIsActivityVisibleForTabs_NoSilenceWhenInIncognito() {
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.CREATED);
        when(mTabModelSelector.isIncognitoBrandedModelSelected()).thenReturn(true);

        assertFalse(mController.isActivityVisibleForTabs(Collections.singleton(123)));
    }

    @Test
    public void testIsActivityVisibleForTabs_SettingsActivity_NotVisible() {
        ApplicationStatus.onStateChangeForTesting(mSettingsActivity, ActivityState.CREATED);
        assertFalse(mController.isActivityVisibleForTabs(Collections.singleton(123)));
    }

    @Test
    public void testIsActivityVisibleForTabs_NoSilenceWhenActivityFinishing() {
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.RESUMED);
        when(mChromeActivity.isFinishing()).thenReturn(true);

        assertFalse(mController.isActivityVisibleForTabs(Collections.singleton(123)));
    }

    @Test
    public void testIsActivityVisibleForTabs_NoSilenceWhenActivityDestroyed() {
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.RESUMED);
        when(mChromeActivity.isDestroyed()).thenReturn(true);

        assertFalse(mController.isActivityVisibleForTabs(Collections.singleton(123)));
    }

    @Test
    public void testIsActivityVisibleForTabs_NoSilenceOnNtp() {
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mChromeActivity, ActivityState.RESUMED);

        GURL ntpUrl = new GURL("chrome-native://newtab/");
        when(mTab.getUrl()).thenReturn(ntpUrl);
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);

        assertFalse(mController.isActivityVisibleForTabs(Collections.singleton(123)));
    }

    @Test
    public void testResolveActorIntentTabId() {
        assertEquals(
                "Null intent should return INVALID_TAB_ID.",
                Tab.INVALID_TAB_ID,
                ActorForegroundServiceController.resolveActorIntentTabId(null));

        Intent nonActorIntent = new Intent(Intent.ACTION_VIEW);
        assertEquals(
                "Non-actor intent with invalid tab ID should return INVALID_TAB_ID.",
                Tab.INVALID_TAB_ID,
                ActorForegroundServiceController.resolveActorIntentTabId(nonActorIntent));

        Intent invalidTaskIntent = new Intent(Intent.ACTION_VIEW);
        invalidTaskIntent.putExtra(
                NotificationConstants.EXTRA_ACTOR_TASK_ID, ActorTask.INVALID_TASK_ID);
        assertEquals(
                "Intent with INVALID_TASK_ID should return INVALID_TAB_ID.",
                Tab.INVALID_TAB_ID,
                ActorForegroundServiceController.resolveActorIntentTabId(invalidTaskIntent));

        Intent actorIntent = new Intent(Intent.ACTION_VIEW);
        actorIntent.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, 123);

        Profile profile = mock(Profile.class);
        ProfileManager.setLastUsedProfileForTesting(profile);
        ActorKeyedService actorKeyedService = mock(ActorKeyedService.class);
        ActorKeyedServiceFactory.setForTesting(actorKeyedService);

        // When task does not exist, should return INVALID_TAB_ID.
        assertEquals(
                "Non-existent task should return INVALID_TAB_ID.",
                Tab.INVALID_TAB_ID,
                ActorForegroundServiceController.resolveActorIntentTabId(actorIntent));

        ActorTask task = mock(ActorTask.class);
        when(actorKeyedService.getTask(123)).thenReturn(task);
        when(task.getTargetTabId()).thenReturn(789);

        assertEquals(
                "Should resolve tab ID from task.getTargetTabId().",
                789,
                ActorForegroundServiceController.resolveActorIntentTabId(actorIntent));

        ActorKeyedServiceFactory.setForTesting(null);
    }

    @Test
    public void testActorTask_getTargetTabId() {
        ActorTask task = mock(ActorTask.class);
        when(task.getTargetTabId()).thenCallRealMethod();

        when(task.getLastActuatedTabId()).thenReturn(789);
        when(task.getTabs()).thenReturn(Collections.singleton(456));
        assertEquals(789, task.getTargetTabId());

        // Fall back to any associated tab when last actuated tab ID is invalid.
        when(task.getLastActuatedTabId()).thenReturn(Tab.INVALID_TAB_ID);
        assertEquals(456, task.getTargetTabId());

        when(task.getTabs()).thenReturn(Collections.emptySet());
        assertEquals(Tab.INVALID_TAB_ID, task.getTargetTabId());
    }

    public ServiceConnection getServiceConnectionForTesting() {
        return mController.getServiceConnectionForTesting();
    }
}
