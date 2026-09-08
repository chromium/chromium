// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.view.WindowManager;

import androidx.test.filters.SmallTest;

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
        ActorTask taskCreated = mock(ActorTask.class);
        when(taskCreated.getState()).thenReturn(ActorTaskState.CREATED);
        when(taskCreated.getTabs()).thenReturn(Collections.singleton(1));

        ActorTask taskActing = mock(ActorTask.class);
        when(taskActing.getState()).thenReturn(ActorTaskState.ACTING);
        when(taskActing.getTabs()).thenReturn(Collections.singleton(1));

        ActorTask taskReflecting = mock(ActorTask.class);
        when(taskReflecting.getState()).thenReturn(ActorTaskState.REFLECTING);
        when(taskReflecting.getTabs()).thenReturn(Collections.singleton(1));

        ActorTask taskPaused = mock(ActorTask.class);
        when(taskPaused.getState()).thenReturn(ActorTaskState.PAUSED_BY_USER);
        when(taskPaused.getTabs()).thenReturn(Collections.singleton(1));

        when(mActorService.getActiveTasks())
                .thenReturn(Arrays.asList(taskCreated, taskActing, taskReflecting, taskPaused));

        mActorTaskHelper.onStopWithNative();

        verify(taskCreated).pause();
        verify(taskActing).pause();
        verify(taskReflecting).pause();
        verify(taskPaused, never()).pause();
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
        TabModelSelector selector = mock(TabModelSelector.class);
        SettableMonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                ObservableSuppliers.createMonotonic();
        selectorSupplier.set(selector);

        ActorTaskHelper helper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        selectorSupplier,
                        mActivityLifecycleDispatcher);

        ActorTask taskInWindow = mock(ActorTask.class);
        when(taskInWindow.getState()).thenReturn(ActorTaskState.ACTING);
        when(taskInWindow.getTabs()).thenReturn(Collections.singleton(101));
        Tab tab101 = mock(Tab.class);
        when(selector.getTabById(101)).thenReturn(tab101);

        ActorTask taskOtherWindow = mock(ActorTask.class);
        when(taskOtherWindow.getState()).thenReturn(ActorTaskState.ACTING);
        when(taskOtherWindow.getTabs()).thenReturn(Collections.singleton(102));
        when(selector.getTabById(102)).thenReturn(null);

        when(mActorService.getActiveTasks())
                .thenReturn(Arrays.asList(taskInWindow, taskOtherWindow));

        helper.onStopWithNative();

        verify(taskInWindow).pause();
        verify(taskOtherWindow, never()).pause();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testOnStop_BackgroundActuationAndNotificationsEnabled_TransitionsToBackground() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        ActorForegroundServiceController mockFgsController =
                mock(ActorForegroundServiceController.class);
        ActorForegroundServiceController.setInstanceForTesting(mockFgsController);

        TabModelSelector selector = mock(TabModelSelector.class);
        SettableMonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                ObservableSuppliers.createMonotonic();
        selectorSupplier.set(selector);

        ActorTaskHelper helper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        selectorSupplier,
                        mActivityLifecycleDispatcher);

        helper.onStopWithNative();

        verify(mockFgsController).transitionActiveTasksToBackground(selector);
        verify(mActorService, never()).stopTask(anyInt(), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testOnStop_BackgroundActuationEnabled_NotificationsDisabled_PausesTasks() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        ActorForegroundServiceController mockFgsController =
                mock(ActorForegroundServiceController.class);
        ActorForegroundServiceController.setInstanceForTesting(mockFgsController);

        TabModelSelector selector = mock(TabModelSelector.class);
        SettableMonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                ObservableSuppliers.createMonotonic();
        selectorSupplier.set(selector);

        ActorTaskHelper helper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        selectorSupplier,
                        mActivityLifecycleDispatcher);

        ActorTask taskInWindow = mock(ActorTask.class);
        when(taskInWindow.getState()).thenReturn(ActorTaskState.ACTING);
        when(taskInWindow.getTabs()).thenReturn(Collections.singleton(101));
        Tab tab101 = mock(Tab.class);
        when(selector.getTabById(101)).thenReturn(tab101);

        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(taskInWindow));

        helper.onStopWithNative();

        verify(mockFgsController, never()).transitionActiveTasksToBackground(any());
        verify(taskInWindow).pause();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.GLIC_BACKGROUND_ACTUATION + ":require_notifications/false"})
    public void
            testOnStop_BackgroundActuation_RequireNotificationsFalse_NotificationsDisabled_TransitionsToBackground() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        ActorForegroundServiceController mockFgsController =
                mock(ActorForegroundServiceController.class);
        ActorForegroundServiceController.setInstanceForTesting(mockFgsController);

        TabModelSelector selector = mock(TabModelSelector.class);
        SettableMonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                ObservableSuppliers.createMonotonic();
        selectorSupplier.set(selector);

        ActorTaskHelper helper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        selectorSupplier,
                        mActivityLifecycleDispatcher);

        ActorTask taskInWindow = mock(ActorTask.class);
        when(taskInWindow.getState()).thenReturn(ActorTaskState.ACTING);
        when(taskInWindow.getTabs()).thenReturn(Collections.singleton(101));
        Tab tab101 = mock(Tab.class);
        when(selector.getTabById(101)).thenReturn(tab101);

        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(taskInWindow));

        helper.onStopWithNative();

        verify(mockFgsController).transitionActiveTasksToBackground(selector);
        verify(taskInWindow, never()).pause();
        verify(mActorService, never()).stopTask(anyInt(), anyInt());
    }

    @Test
    public void testOnDestroy_OnlyCurrentWindow() {
        TabModelSelector selector = mock(TabModelSelector.class);
        SettableMonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                ObservableSuppliers.createMonotonic();
        selectorSupplier.set(selector);

        ActorTaskHelper helper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        selectorSupplier,
                        mActivityLifecycleDispatcher);

        ActorTask taskInWindow = mock(ActorTask.class);
        when(taskInWindow.getId()).thenReturn(101);
        when(taskInWindow.getTabs()).thenReturn(Collections.singleton(101));
        Tab tab101 = mock(Tab.class);
        when(selector.getTabById(101)).thenReturn(tab101);

        ActorTask taskOtherWindow = mock(ActorTask.class);
        when(taskOtherWindow.getId()).thenReturn(102);
        when(taskOtherWindow.getTabs()).thenReturn(Collections.singleton(102));
        when(selector.getTabById(102)).thenReturn(null);

        when(mActorService.getActiveTasks())
                .thenReturn(Arrays.asList(taskInWindow, taskOtherWindow));

        helper.onDestroy();

        verify(mActorService).stopTask(101, StoppedReason.SHUTDOWN);
        verify(mActorService, never()).stopTask(102, StoppedReason.SHUTDOWN);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testOnDestroy_BackgroundActuationAndNotificationsEnabled_DoesNotStopTasks() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        TabModelSelector selector = mock(TabModelSelector.class);
        SettableMonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                ObservableSuppliers.createMonotonic();
        selectorSupplier.set(selector);

        ActorTaskHelper helper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        selectorSupplier,
                        mActivityLifecycleDispatcher);

        ActorTask taskInWindow = mock(ActorTask.class);
        when(taskInWindow.getId()).thenReturn(101);
        when(taskInWindow.getTabs()).thenReturn(Collections.singleton(101));
        Tab tab101 = mock(Tab.class);
        when(selector.getTabById(101)).thenReturn(tab101);

        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(taskInWindow));

        helper.onDestroy();

        verify(mActorService, never()).stopTask(anyInt(), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testOnDestroy_BackgroundActuationEnabled_NotificationsDisabled_StopsTasks() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        TabModelSelector selector = mock(TabModelSelector.class);
        SettableMonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                ObservableSuppliers.createMonotonic();
        selectorSupplier.set(selector);

        ActorTaskHelper helper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        selectorSupplier,
                        mActivityLifecycleDispatcher);

        ActorTask taskInWindow = mock(ActorTask.class);
        when(taskInWindow.getId()).thenReturn(101);
        when(taskInWindow.getTabs()).thenReturn(Collections.singleton(101));
        Tab tab101 = mock(Tab.class);
        when(selector.getTabById(101)).thenReturn(tab101);

        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(taskInWindow));

        helper.onDestroy();

        verify(mActorService).stopTask(101, StoppedReason.SHUTDOWN);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testOnStop_BackgroundActuationEnabled_ChannelBlocked_PausesTasks() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        NotificationManagerProxy mockNotificationManager = mock(NotificationManagerProxy.class);
        NotificationChannel channel = mock(NotificationChannel.class);
        when(channel.getImportance()).thenReturn(NotificationManager.IMPORTANCE_NONE);
        when(mockNotificationManager.getNotificationChannel(
                        ChromeChannelDefinitions.ChannelId.ACTOR))
                .thenReturn(channel);
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mockNotificationManager);

        ActorForegroundServiceController mockFgsController =
                mock(ActorForegroundServiceController.class);
        ActorForegroundServiceController.setInstanceForTesting(mockFgsController);

        TabModelSelector selector = mock(TabModelSelector.class);
        SettableMonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                ObservableSuppliers.createMonotonic();
        selectorSupplier.set(selector);

        ActorTaskHelper helper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        selectorSupplier,
                        mActivityLifecycleDispatcher);

        ActorTask taskInWindow = mock(ActorTask.class);
        when(taskInWindow.getState()).thenReturn(ActorTaskState.ACTING);
        when(taskInWindow.getTabs()).thenReturn(Collections.singleton(101));
        Tab tab101 = mock(Tab.class);
        when(selector.getTabById(101)).thenReturn(tab101);

        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(taskInWindow));

        helper.onStopWithNative();

        verify(mockFgsController, never()).transitionActiveTasksToBackground(any());
        verify(taskInWindow).pause();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testOnDestroy_BackgroundActuationEnabled_ChannelBlocked_StopsTasks() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        NotificationManagerProxy mockNotificationManager = mock(NotificationManagerProxy.class);
        NotificationChannel channel = mock(NotificationChannel.class);
        when(channel.getImportance()).thenReturn(NotificationManager.IMPORTANCE_NONE);
        when(mockNotificationManager.getNotificationChannel(
                        ChromeChannelDefinitions.ChannelId.ACTOR))
                .thenReturn(channel);
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mockNotificationManager);

        TabModelSelector selector = mock(TabModelSelector.class);
        SettableMonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                ObservableSuppliers.createMonotonic();
        selectorSupplier.set(selector);

        ActorTaskHelper helper =
                new ActorTaskHelper(
                        mActivity,
                        mProfileSupplier,
                        selectorSupplier,
                        mActivityLifecycleDispatcher);

        ActorTask taskInWindow = mock(ActorTask.class);
        when(taskInWindow.getId()).thenReturn(101);
        when(taskInWindow.getTabs()).thenReturn(Collections.singleton(101));
        Tab tab101 = mock(Tab.class);
        when(selector.getTabById(101)).thenReturn(tab101);

        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(taskInWindow));

        helper.onDestroy();

        verify(mActorService).stopTask(101, StoppedReason.SHUTDOWN);
    }

    @Test
    @SmallTest
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
    @SmallTest
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testBackgroundActuation_RequireNotificationsDefault_WithNotificationsEnabled() {
        setNotificationsEnabled(true);
        assertTrue(ActorUtils.isBackgroundActuationEnabled());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testBackgroundActuation_RequireNotificationsDefault_WithNotificationsDisabled() {
        setNotificationsEnabled(false);
        assertFalse(ActorUtils.isBackgroundActuationEnabled());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.GLIC_BACKGROUND_ACTUATION + ":require_notifications/false"})
    public void testBackgroundActuation_RequireNotificationsFalse_WithNotificationsDisabled() {
        setNotificationsEnabled(false);
        assertTrue(ActorUtils.isBackgroundActuationEnabled());
    }

    @Test
    @SmallTest
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
        ActorForegroundServiceController controller = mock(ActorForegroundServiceController.class);
        ActorForegroundServiceController.setInstanceForTesting(controller);
        ActorForegroundServiceManager manager = mock(ActorForegroundServiceManager.class);
        ActorForegroundServiceManager.setInstanceForTesting(manager);

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        mActorTaskHelper.onStopWithNative();

        verify(controller).transitionActiveTasksToBackground(mTabModelSelector);
        verify(manager).resendWorkingNotifications();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testOnStop_GlicBackgroundActuation_WithVisibleActivities_DoesNotCallManager() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        ActorForegroundServiceController controller = mock(ActorForegroundServiceController.class);
        ActorForegroundServiceController.setInstanceForTesting(controller);
        ActorForegroundServiceManager manager = mock(ActorForegroundServiceManager.class);
        ActorForegroundServiceManager.setInstanceForTesting(manager);

        Activity otherActivity = Robolectric.buildActivity(Activity.class).setup().get();
        ApplicationStatus.onStateChangeForTesting(otherActivity, ActivityState.RESUMED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        mActorTaskHelper.onStopWithNative();

        verify(controller).transitionActiveTasksToBackground(mTabModelSelector);
        verify(manager, never()).resendWorkingNotifications();

        ApplicationStatus.onStateChangeForTesting(otherActivity, ActivityState.DESTROYED);
    }

    @Test
    public void testOnStop_BackgroundActuationDisabled_DoesNotCallManager() {
        ActorForegroundServiceManager manager = mock(ActorForegroundServiceManager.class);
        ActorForegroundServiceManager.setInstanceForTesting(manager);

        mActorTaskHelper.onStopWithNative();

        verify(manager, never()).resendWorkingNotifications();
    }
}
