// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.view.WindowManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;

import java.util.Arrays;
import java.util.Collections;

/** Unit tests for {@link ActorTaskHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
public class ActorTaskHelperTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorService;
    @Mock private ActorTask mActorTask;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Tab mTab;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private OffscreenRenderingManager mOffscreenRenderingManager;
    @Mock private ActorTask mTaskCreated;
    @Mock private ActorTask mTaskActing;
    @Mock private ActorTask mTaskReflecting;
    @Mock private ActorTask mTaskPaused;
    @Mock private TabModelSelector mSelector;
    @Mock private ActorTask mTaskInWindow;
    @Mock private Tab mTab101;
    @Mock private ActorTask mTaskOtherWindow;
    @Mock private ActorForegroundServiceController mActorForegroundServiceController;
    @Mock private NotificationManagerProxy mNotificationManagerProxy;
    @Mock private NotificationChannel mNotificationChannel;
    @Mock private ActorForegroundServiceManager mActorForegroundServiceManager;

    private Activity mActivity;
    private SettableMonotonicObservableSupplier<Profile> mProfileSupplier;
    private SettableMonotonicObservableSupplier<TabModelSelector> mSelectorSupplier;
    private ActorTaskHelper mActorTaskHelper;

    @Before
    public void setUp() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        mActivity = Robolectric.buildActivity(Activity.class).create().get();

        mProfileSupplier = ObservableSuppliers.createMonotonic();
        mProfileSupplier.set(mProfile);
        mSelectorSupplier = ObservableSuppliers.createMonotonic();
        mSelectorSupplier.set(mTabModelSelector);

        when(mTabModelSelector.getTabById(1)).thenReturn(mTab);
        when(mActorTask.getTabs()).thenReturn(Collections.singleton(1));
        ActorKeyedServiceFactory.setForTesting(mActorService);
        OffscreenRenderingManager.setInstanceForTesting(mOffscreenRenderingManager);

        mActorTaskHelper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        mSelectorSupplier,
                        mActivityLifecycleDispatcher);
    }

    @After
    public void tearDown() {
        OffscreenRenderingManager.setInstanceForTesting(null);
    }

    private void setNotificationsEnabled(boolean enabled) {
        NotificationProxyUtils.setNotificationEnabledForTest(enabled);
    }

    @Test
    public void testKeepScreenOn_TaskActive() {
        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(mActorTask));
        when(mActorTask.getState()).thenReturn(ActorTaskState.ACTING);

        mActorTaskHelper.onTaskStateChanged(1, ActorTaskState.ACTING);

        assertTrue(
                (mActivity.getWindow().getAttributes().flags
                                & WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                        != 0);
        verify(mActorService).addObserver(mActorTaskHelper);
    }

    @Test
    public void testKeepScreenOn_TaskInactive() {
        // Start with active task
        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(mActorTask));
        when(mActorTask.getState()).thenReturn(ActorTaskState.ACTING);
        mActorTaskHelper.onTaskStateChanged(1, ActorTaskState.ACTING);
        assertTrue(
                (mActivity.getWindow().getAttributes().flags
                                & WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                        != 0);

        // Task finished
        when(mActorTask.getState()).thenReturn(ActorTaskState.FINISHED);
        mActorTaskHelper.onTaskStateChanged(1, ActorTaskState.FINISHED);

        assertFalse(
                (mActivity.getWindow().getAttributes().flags
                                & WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                        != 0);
    }

    @Test
    public void testOnStop() {
        when(mTaskCreated.getState()).thenReturn(ActorTaskState.CREATED);
        when(mTaskCreated.getTabs()).thenReturn(Collections.singleton(1));

        when(mTaskActing.getState()).thenReturn(ActorTaskState.ACTING);
        when(mTaskActing.getTabs()).thenReturn(Collections.singleton(1));

        when(mTaskReflecting.getState()).thenReturn(ActorTaskState.REFLECTING);
        when(mTaskReflecting.getTabs()).thenReturn(Collections.singleton(1));

        when(mTaskPaused.getState()).thenReturn(ActorTaskState.PAUSED_BY_USER);
        when(mTaskPaused.getTabs()).thenReturn(Collections.singleton(1));

        when(mActorService.getActiveTasks())
                .thenReturn(Arrays.asList(mTaskCreated, mTaskActing, mTaskReflecting, mTaskPaused));

        mActorTaskHelper.onStopWithNative();

        verify(mTaskCreated).pause();
        verify(mTaskActing).pause();
        verify(mTaskReflecting).pause();
        verify(mTaskPaused, never()).pause();
    }

    @Test
    public void testDestroy() {
        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(mActorTask));
        when(mActorTask.getState()).thenReturn(ActorTaskState.ACTING);
        mActorTaskHelper.onTaskStateChanged(1, ActorTaskState.ACTING);

        mActorTaskHelper.destroy();

        verify(mActivityLifecycleDispatcher).unregister(mActorTaskHelper);
        verify(mActorService, atLeastOnce()).removeObserver(mActorTaskHelper);
        assertFalse(
                (mActivity.getWindow().getAttributes().flags
                                & WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                        != 0);
    }

    @Test
    public void testOnStop_OnlyCurrentWindow() {
        SettableMonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                ObservableSuppliers.createMonotonic();
        selectorSupplier.set(mSelector);

        ActorTaskHelper helper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        selectorSupplier,
                        mActivityLifecycleDispatcher);

        when(mTaskInWindow.getState()).thenReturn(ActorTaskState.ACTING);
        when(mTaskInWindow.getTabs()).thenReturn(Collections.singleton(101));
        when(mSelector.getTabById(101)).thenReturn(mTab101);

        when(mTaskOtherWindow.getState()).thenReturn(ActorTaskState.ACTING);
        when(mTaskOtherWindow.getTabs()).thenReturn(Collections.singleton(102));
        when(mSelector.getTabById(102)).thenReturn(null);

        when(mActorService.getActiveTasks())
                .thenReturn(Arrays.asList(mTaskInWindow, mTaskOtherWindow));

        helper.onStopWithNative();

        verify(mTaskInWindow).pause();
        verify(mTaskOtherWindow, never()).pause();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testOnStop_BackgroundActuationAndNotificationsEnabled_TransitionsToBackground() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        ActorForegroundServiceController.setInstanceForTesting(mActorForegroundServiceController);

        SettableMonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                ObservableSuppliers.createMonotonic();
        selectorSupplier.set(mSelector);

        ActorTaskHelper helper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        selectorSupplier,
                        mActivityLifecycleDispatcher);

        helper.onStopWithNative();

        verify(mActorForegroundServiceController).transitionActiveTasksToBackground(mSelector);
        verify(mActorService, never()).stopTask(anyInt(), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testOnStop_BackgroundActuationEnabled_NotificationsDisabled_PausesTasks() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        ActorForegroundServiceController.setInstanceForTesting(mActorForegroundServiceController);

        SettableMonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                ObservableSuppliers.createMonotonic();
        selectorSupplier.set(mSelector);

        ActorTaskHelper helper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        selectorSupplier,
                        mActivityLifecycleDispatcher);

        when(mTaskInWindow.getState()).thenReturn(ActorTaskState.ACTING);
        when(mTaskInWindow.getTabs()).thenReturn(Collections.singleton(101));
        when(mSelector.getTabById(101)).thenReturn(mTab101);

        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(mTaskInWindow));

        helper.onStopWithNative();

        verify(mActorForegroundServiceController, never()).transitionActiveTasksToBackground(any());
        verify(mTaskInWindow).pause();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.GLIC_BACKGROUND_ACTUATION + ":require_notifications/false"})
    public void
            testOnStop_BackgroundActuation_RequireNotificationsFalse_NotificationsDisabled_TransitionsToBackground() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        ActorForegroundServiceController.setInstanceForTesting(mActorForegroundServiceController);

        SettableMonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                ObservableSuppliers.createMonotonic();
        selectorSupplier.set(mSelector);

        ActorTaskHelper helper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        selectorSupplier,
                        mActivityLifecycleDispatcher);

        when(mTaskInWindow.getState()).thenReturn(ActorTaskState.ACTING);
        when(mTaskInWindow.getTabs()).thenReturn(Collections.singleton(101));
        when(mSelector.getTabById(101)).thenReturn(mTab101);

        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(mTaskInWindow));

        helper.onStopWithNative();

        verify(mActorForegroundServiceController).transitionActiveTasksToBackground(mSelector);
        verify(mTaskInWindow, never()).pause();
        verify(mActorService, never()).stopTask(anyInt(), anyInt());
    }

    @Test
    public void testOnDestroy_OnlyCurrentWindow() {
        SettableMonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                ObservableSuppliers.createMonotonic();
        selectorSupplier.set(mSelector);

        ActorTaskHelper helper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        selectorSupplier,
                        mActivityLifecycleDispatcher);

        when(mTaskInWindow.getId()).thenReturn(101);
        when(mTaskInWindow.getTabs()).thenReturn(Collections.singleton(101));
        when(mSelector.getTabById(101)).thenReturn(mTab101);

        when(mTaskOtherWindow.getId()).thenReturn(102);
        when(mTaskOtherWindow.getTabs()).thenReturn(Collections.singleton(102));
        when(mSelector.getTabById(102)).thenReturn(null);

        when(mActorService.getActiveTasks())
                .thenReturn(Arrays.asList(mTaskInWindow, mTaskOtherWindow));

        helper.onDestroy();

        verify(mActorService).stopTask(101, StoppedReason.SHUTDOWN);
        verify(mActorService, never()).stopTask(102, StoppedReason.SHUTDOWN);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testOnDestroy_BackgroundActuationAndNotificationsEnabled_DoesNotStopTasks() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        SettableMonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                ObservableSuppliers.createMonotonic();
        selectorSupplier.set(mSelector);

        ActorTaskHelper helper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        selectorSupplier,
                        mActivityLifecycleDispatcher);

        when(mTaskInWindow.getId()).thenReturn(101);
        when(mTaskInWindow.getTabs()).thenReturn(Collections.singleton(101));
        when(mSelector.getTabById(101)).thenReturn(mTab101);

        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(mTaskInWindow));

        helper.onDestroy();

        verify(mActorService, never()).stopTask(anyInt(), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testOnDestroy_BackgroundActuationEnabled_NotificationsDisabled_StopsTasks() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        SettableMonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                ObservableSuppliers.createMonotonic();
        selectorSupplier.set(mSelector);

        ActorTaskHelper helper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        selectorSupplier,
                        mActivityLifecycleDispatcher);

        when(mTaskInWindow.getId()).thenReturn(101);
        when(mTaskInWindow.getTabs()).thenReturn(Collections.singleton(101));
        when(mSelector.getTabById(101)).thenReturn(mTab101);

        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(mTaskInWindow));

        helper.onDestroy();

        verify(mActorService).stopTask(101, StoppedReason.SHUTDOWN);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testOnStop_BackgroundActuationEnabled_ChannelBlocked_PausesTasks() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        when(mNotificationChannel.getImportance()).thenReturn(NotificationManager.IMPORTANCE_NONE);
        when(mNotificationManagerProxy.getNotificationChannel(
                        ChromeChannelDefinitions.ChannelId.ACTOR))
                .thenReturn(mNotificationChannel);
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mNotificationManagerProxy);

        ActorForegroundServiceController.setInstanceForTesting(mActorForegroundServiceController);

        SettableMonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                ObservableSuppliers.createMonotonic();
        selectorSupplier.set(mSelector);

        ActorTaskHelper helper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        selectorSupplier,
                        mActivityLifecycleDispatcher);

        when(mTaskInWindow.getState()).thenReturn(ActorTaskState.ACTING);
        when(mTaskInWindow.getTabs()).thenReturn(Collections.singleton(101));
        when(mSelector.getTabById(101)).thenReturn(mTab101);

        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(mTaskInWindow));

        helper.onStopWithNative();

        verify(mActorForegroundServiceController, never()).transitionActiveTasksToBackground(any());
        verify(mTaskInWindow).pause();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testOnDestroy_BackgroundActuationEnabled_ChannelBlocked_StopsTasks() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        when(mNotificationChannel.getImportance()).thenReturn(NotificationManager.IMPORTANCE_NONE);
        when(mNotificationManagerProxy.getNotificationChannel(
                        ChromeChannelDefinitions.ChannelId.ACTOR))
                .thenReturn(mNotificationChannel);
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mNotificationManagerProxy);

        SettableMonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                ObservableSuppliers.createMonotonic();
        selectorSupplier.set(mSelector);

        ActorTaskHelper helper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        selectorSupplier,
                        mActivityLifecycleDispatcher);

        when(mTaskInWindow.getId()).thenReturn(101);
        when(mTaskInWindow.getTabs()).thenReturn(Collections.singleton(101));
        when(mSelector.getTabById(101)).thenReturn(mTab101);

        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(mTaskInWindow));

        helper.onDestroy();

        verify(mActorService).stopTask(101, StoppedReason.SHUTDOWN);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testBackgroundActuation_BaseFeatureDisabled_AlwaysReturnsFalse() {
        setNotificationsEnabled(true);
        ChromeFeatureList.sGlicBackgroundActuationRequireNotifications.setForTesting(true);
        assertFalse(ActorUtils.isBackgroundActuationEnabled());
        setNotificationsEnabled(false);
        ChromeFeatureList.sGlicBackgroundActuationRequireNotifications.setForTesting(false);
        assertFalse(ActorUtils.isBackgroundActuationEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testBackgroundActuation_RequireNotificationsDefault_WithNotificationsEnabled() {
        setNotificationsEnabled(true);
        assertTrue(ActorUtils.isBackgroundActuationEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testBackgroundActuation_RequireNotificationsDefault_WithNotificationsDisabled() {
        setNotificationsEnabled(false);
        assertFalse(ActorUtils.isBackgroundActuationEnabled());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.GLIC_BACKGROUND_ACTUATION + ":require_notifications/false"})
    public void testBackgroundActuation_RequireNotificationsFalse_WithNotificationsDisabled() {
        setNotificationsEnabled(false);
        assertTrue(ActorUtils.isBackgroundActuationEnabled());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.GLIC_BACKGROUND_ACTUATION + ":require_notifications/false"})
    public void testBackgroundActuation_RequireNotificationsFalse_WithNotificationsEnabled() {
        setNotificationsEnabled(true);
        assertTrue(ActorUtils.isBackgroundActuationEnabled());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testOnStop_Tablet_StartsOffscreenRendering() {
        when(mActorService.getCurrentActiveTask()).thenReturn(mActorTask);
        when(mActorTask.getLastActedTabs()).thenReturn(Collections.singleton(1));
        when(mTabModelSelector.getTabById(1)).thenReturn(mTab);

        mActivity.findViewById(android.R.id.content).layout(0, 0, 800, 1200);

        mActorTaskHelper.onStopWithNative();

        verify(mOffscreenRenderingManager).startOffscreenRendering(mTab, 800, 1200);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testOnStop_Tablet_NoActingTab_NoOffscreenRendering() {
        when(mActorService.getCurrentActiveTask()).thenReturn(mActorTask);
        when(mActorTask.getLastActedTabs()).thenReturn(Collections.emptySet());

        mActorTaskHelper.onStopWithNative();

        verify(mOffscreenRenderingManager, never())
                .startOffscreenRendering(any(), anyInt(), anyInt());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testOnStart_StopsOffscreenRendering() {
        when(mActorService.getCurrentActiveTask()).thenReturn(mActorTask);
        when(mActorTask.getLastActedTabs()).thenReturn(Collections.singleton(1));
        when(mTabModelSelector.getTabById(1)).thenReturn(mTab);
        mActivity.findViewById(android.R.id.content).layout(0, 0, 800, 1200);

        mActorTaskHelper.onStopWithNative();
        verify(mOffscreenRenderingManager).startOffscreenRendering(mTab, 800, 1200);

        mActorTaskHelper.onStartWithNative();

        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testOnTaskStateChanged_CompletedState_StopsOffscreenRendering() {
        when(mActorService.getCurrentActiveTask()).thenReturn(mActorTask);
        when(mActorTask.getLastActedTabs()).thenReturn(Collections.singleton(1));
        when(mTabModelSelector.getTabById(1)).thenReturn(mTab);
        mActivity.findViewById(android.R.id.content).layout(0, 0, 800, 1200);

        mActorTaskHelper.onStopWithNative();
        verify(mOffscreenRenderingManager).startOffscreenRendering(mTab, 800, 1200);

        mActorTaskHelper.onTaskStateChanged(1, ActorTaskState.FINISHED);

        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testOnTaskStateChanged_NonCompletedState_DoesNotStopOffscreenRendering() {
        when(mActorService.getCurrentActiveTask()).thenReturn(mActorTask);
        when(mActorTask.getLastActedTabs()).thenReturn(Collections.singleton(1));
        when(mTabModelSelector.getTabById(1)).thenReturn(mTab);
        mActivity.findViewById(android.R.id.content).layout(0, 0, 800, 1200);

        mActorTaskHelper.onStopWithNative();
        verify(mOffscreenRenderingManager).startOffscreenRendering(mTab, 800, 1200);

        mActorTaskHelper.onTaskStateChanged(1, ActorTaskState.ACTING);

        verify(mOffscreenRenderingManager, never()).stopOffscreenRendering(mTab);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testDestroy_StopsOffscreenRendering() {
        when(mActorService.getCurrentActiveTask()).thenReturn(mActorTask);
        when(mActorTask.getLastActedTabs()).thenReturn(Collections.singleton(1));
        when(mTabModelSelector.getTabById(1)).thenReturn(mTab);
        mActivity.findViewById(android.R.id.content).layout(0, 0, 800, 1200);

        mActorTaskHelper.onStopWithNative();
        verify(mOffscreenRenderingManager).startOffscreenRendering(mTab, 800, 1200);

        mActorTaskHelper.destroy();

        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testOnStop_GlicBackgroundActuation_NoVisibleActivities_CallsTransitionAndManager() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        ActorForegroundServiceController.setInstanceForTesting(mActorForegroundServiceController);
        ActorForegroundServiceManager.setInstanceForTesting(mActorForegroundServiceManager);

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        mActorTaskHelper.onStopWithNative();

        verify(mActorForegroundServiceController)
                .transitionActiveTasksToBackground(mTabModelSelector);
        verify(mActorForegroundServiceManager).resendWorkingNotifications();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testOnStop_GlicBackgroundActuation_WithVisibleActivities_DoesNotCallManager() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        ActorForegroundServiceController.setInstanceForTesting(mActorForegroundServiceController);
        ActorForegroundServiceManager.setInstanceForTesting(mActorForegroundServiceManager);

        Activity otherActivity = Robolectric.buildActivity(Activity.class).setup().get();
        ApplicationStatus.onStateChangeForTesting(otherActivity, ActivityState.RESUMED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        mActorTaskHelper.onStopWithNative();

        verify(mActorForegroundServiceController)
                .transitionActiveTasksToBackground(mTabModelSelector);
        verify(mActorForegroundServiceManager, never()).resendWorkingNotifications();

        ApplicationStatus.onStateChangeForTesting(otherActivity, ActivityState.DESTROYED);
    }

    @Test
    public void testOnStop_BackgroundActuationDisabled_DoesNotCallManager() {
        ActorForegroundServiceManager.setInstanceForTesting(mActorForegroundServiceManager);

        mActorTaskHelper.onStopWithNative();

        verify(mActorForegroundServiceManager, never()).resendWorkingNotifications();
    }
}
