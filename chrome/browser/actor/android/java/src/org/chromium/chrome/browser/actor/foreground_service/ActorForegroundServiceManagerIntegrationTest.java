// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.foreground_service;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Notification;
import android.content.Intent;
import android.os.Handler;
import android.os.Looper;

import androidx.core.app.ServiceCompat;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.actor.ActorUtils;
import org.chromium.chrome.browser.actor.notifications.ActorNotificationFactory;
import org.chromium.chrome.browser.actor.notifications.ActorNotificationService;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy.NotificationEntry;

import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.TimeUnit;

/** Integration tests for {@link ActorForegroundServiceManager} notification pinning. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowSystemClock.class})
@EnableFeatures({
    ChromeFeatureList.ANDROID_ACTOR_TASK_TIMEOUT,
    ChromeFeatureList.ACTOR_LIVE_NOTIFICATION,
    ChromeFeatureList.ACTOR_STEP_PROGRESS_NOTIFICATION
})
@DisableFeatures(ChromeFeatureList.GLIC_BACKGROUND_TRIGGERING)
@NullMarked
public class ActorForegroundServiceManagerIntegrationTest {
    private static final int FOREGROUND_SERVICE_FLAGS =
            Notification.FLAG_FOREGROUND_SERVICE | Notification.FLAG_NO_CLEAR;
    private static final String UNPINNED_TAG = ActorNotificationService.UNPINNED_NOTIFICATION_TAG;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActorKeyedService mKeyedService;
    @Mock private ActorForegroundServiceController mServiceController;
    @Mock private ChromeBrowserInitializer mChromeBrowserInitializer;
    @Mock private Profile mMockProfile;

    private final Map<Integer, ActorTask> mActiveTasks = new LinkedHashMap<>();
    private final Handler mSystemHandler = new Handler(Looper.getMainLooper());
    private boolean mDefersSystemCancel;
    private FakeSystemNotificationManager mNotificationManager;
    private int mPinnedFgsNotificationId;
    private ActorForegroundServiceManager mManager;
    private ActorNotificationService mNotificationService;

    /**
     * Notification manager that applies Android's foreground service notification rules. When
     * {@link #mDefersSystemCancel} is set, the system cancel of the previously pinned notification
     * runs on the next main looper idle instead of right away.
     */
    private class FakeSystemNotificationManager extends MockNotificationManagerProxy {
        @Override
        public void notify(@Nullable String tag, int id, Notification notification) {
            Notification posted = notification.clone();
            Notification existing = findNotification(tag, id);
            boolean isAdoptedByService = tag == null && id == mPinnedFgsNotificationId;
            boolean keepsServiceFlag =
                    existing != null
                            && (existing.flags & Notification.FLAG_FOREGROUND_SERVICE) != 0;
            if (isAdoptedByService || keepsServiceFlag) {
                posted.flags |= FOREGROUND_SERVICE_FLAGS;
            }
            super.notify(tag, id, posted);
        }

        @Override
        public void cancel(@Nullable String tag, int id) {
            Notification existing = findNotification(tag, id);
            if (existing != null && (existing.flags & Notification.FLAG_FOREGROUND_SERVICE) != 0) {
                return;
            }
            super.cancel(tag, id);
        }

        void startForeground(int id, Notification notification) {
            Notification posted = notification.clone();
            posted.flags |= FOREGROUND_SERVICE_FLAGS;
            super.notify(/* tag= */ null, id, posted);
        }

        void cancelForSystem(int id) {
            if (mDefersSystemCancel) {
                mSystemHandler.post(() -> super.cancel(/* tag= */ null, id));
            } else {
                super.cancel(/* tag= */ null, id);
            }
        }
    }

    @Before
    public void setUp() {
        mPinnedFgsNotificationId = ActorForegroundServiceManager.INVALID_NOTIFICATION_ID;
        mNotificationManager = new FakeSystemNotificationManager();

        ProfileManager.resetForTesting();
        ProfileManager.setLastUsedProfileForTesting(mMockProfile);
        ChromeBrowserInitializer.setForTesting(mChromeBrowserInitializer);
        ActorForegroundServiceController.setInstanceForTesting(mServiceController);
        ActorKeyedServiceFactory.setForTesting(mKeyedService);
        ActorForegroundServiceManager.setWaitTimeForTesting(0);
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mNotificationManager);

        mManager =
                new ActorForegroundServiceManager() {
                    @Override
                    protected boolean canStartForeground() {
                        return true;
                    }
                };
        mManager.setKeyedServiceForTesting(mKeyedService);
        ActorForegroundServiceManager.setInstanceForTesting(mManager);

        lenient()
                .when(mKeyedService.getCurrentActiveTask())
                .thenAnswer(
                        invocation ->
                                mActiveTasks.isEmpty()
                                        ? null
                                        : mActiveTasks.values().iterator().next());
        lenient()
                .doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            if (runnable != null) {
                                when(mServiceController.isConnected()).thenReturn(true);
                                runnable.run();
                            }
                            return null;
                        })
                .when(mServiceController)
                .startAndBindService(any());
        lenient()
                .when(mServiceController.createTrustedBringTabToFrontIntent(any()))
                .thenReturn(new Intent());
        lenient()
                .doAnswer(
                        invocation -> {
                            int newNotificationId = invocation.getArgument(0);
                            Notification newNotification = invocation.getArgument(1);
                            int previousPinnedId = mPinnedFgsNotificationId;
                            mPinnedFgsNotificationId = newNotificationId;
                            if (previousPinnedId
                                            != ActorForegroundServiceManager.INVALID_NOTIFICATION_ID
                                    && previousPinnedId != newNotificationId) {
                                mNotificationManager.cancelForSystem(previousPinnedId);
                            }
                            mNotificationManager.startForeground(
                                    newNotificationId, newNotification);
                            return null;
                        })
                .when(mServiceController)
                .startOrUpdateForegroundService(anyInt(), any(), anyInt(), anyBoolean());
        lenient()
                .doAnswer(
                        invocation -> {
                            int flags = invocation.getArgument(0);
                            int previousPinnedId = mPinnedFgsNotificationId;
                            mPinnedFgsNotificationId =
                                    ActorForegroundServiceManager.INVALID_NOTIFICATION_ID;
                            if ((flags & ServiceCompat.STOP_FOREGROUND_REMOVE) != 0
                                    && previousPinnedId
                                            != ActorForegroundServiceManager
                                                    .INVALID_NOTIFICATION_ID) {
                                mNotificationManager.cancelForSystem(previousPinnedId);
                            }
                            return null;
                        })
                .when(mServiceController)
                .stopActorForegroundService(anyInt());

        shadowOf(Looper.getMainLooper()).idleFor(1, TimeUnit.SECONDS);
        mManager.resetForTesting();

        mNotificationService = new ActorNotificationService(mKeyedService);
        mManager.setNotificationServiceForTesting(mNotificationService);
    }

    @After
    public void tearDown() {
        mNotificationService.clearAll();
        ActorForegroundServiceManager.resetInstanceForTesting();
        ProfileManager.resetForTesting();
    }

    private ActorTask createTask(int taskId, String title) {
        ActorTask task = mock(ActorTask.class);
        lenient().when(task.getId()).thenReturn(taskId);
        lenient().when(task.getTitle()).thenReturn(title);
        lenient().when(task.getState()).thenReturn(ActorTaskState.CREATED);
        lenient().when(mKeyedService.getTask(taskId)).thenReturn(task);
        return task;
    }

    private void transitionTask(ActorTask task, @ActorTaskState int newState) {
        ShadowSystemClock.advanceBy(100, TimeUnit.MILLISECONDS);
        lenient().when(task.getState()).thenReturn(newState);
        if (!ActorUtils.isCompletedState(newState)) {
            mActiveTasks.put(task.getId(), task);
        } else {
            mActiveTasks.remove(task.getId());
        }
        mManager.onTaskStateChanged(task.getId(), newState);
        ShadowLooper.idleMainLooper();
    }

    private @Nullable Notification findNotification(@Nullable String tag, int taskId) {
        for (NotificationEntry entry : mNotificationManager.getNotifications()) {
            if (entry.id == taskId && Objects.equals(entry.tag, tag)) {
                return entry.notification;
            }
        }
        return null;
    }

    private static boolean isDismissible(Notification notification) {
        return (notification.flags
                        & (Notification.FLAG_ONGOING_EVENT
                                | Notification.FLAG_NO_CLEAR
                                | Notification.FLAG_FOREGROUND_SERVICE))
                == 0;
    }

    @Test
    public void testPinSwap_RepostBeforeSystemCancel_UnpinnedNotificationHasNoServiceFlags() {
        mDefersSystemCancel = true;
        ActorTask task1 = createTask(1, "Task 1");
        ActorTask task2 = createTask(2, "Task 2");
        transitionTask(task1, ActorTaskState.ACTING);

        when(mKeyedService.getCurrentActiveTask()).thenReturn(task2);
        transitionTask(task2, ActorTaskState.ACTING);

        assertEquals(2, mPinnedFgsNotificationId);
        Notification unpinned = findNotification(UNPINNED_TAG, 1);
        assertNotNull("The unpinned task's notification must stay in the shade", unpinned);
        assertEquals(0, unpinned.flags & FOREGROUND_SERVICE_FLAGS);
    }

    @Test
    public void testPinSwap_RepostBeforeSystemCancel_CompletedTaskIsDemotedAndDismissible() {
        mDefersSystemCancel = true;
        ActorTask task1 = createTask(1, "Task 1");
        ActorTask task2 = createTask(2, "Task 2");
        transitionTask(task1, ActorTaskState.ACTING);
        transitionTask(task2, ActorTaskState.ACTING);

        transitionTask(task1, ActorTaskState.FINISHED);

        assertEquals(2, mPinnedFgsNotificationId);
        assertNull(findNotification(/* tag= */ null, 1));
        Notification demoted = findNotification(UNPINNED_TAG, 1);
        assertNotNull("The completed task's notification must stay in the shade", demoted);
        assertTrue(isDismissible(demoted));
        assertFalse(
                demoted.extras.getBoolean(ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
    }

    @Test
    public void testServiceStop_RepostBeforeSystemCancel_DemotedNotificationStaysDismissible() {
        mDefersSystemCancel = true;
        ActorTask task1 = createTask(1, "Task 1");
        transitionTask(task1, ActorTaskState.ACTING);
        transitionTask(task1, ActorTaskState.FINISHED);

        shadowOf(Looper.getMainLooper())
                .idleFor(ActorNotificationService.getDemotionDelayMs() + 1, TimeUnit.MILLISECONDS);

        assertFalse(mManager.isServiceBoundForTesting());
        Notification demoted = findNotification(UNPINNED_TAG, 1);
        assertNotNull("The demoted notification must stay in the shade", demoted);
        assertTrue(isDismissible(demoted));
    }

    @Test
    public void testRepinningTask_RemovesTaggedRepost() {
        ActorTask task1 = createTask(1, "Task 1");
        ActorTask task2 = createTask(2, "Task 2");
        transitionTask(task1, ActorTaskState.ACTING);
        when(mKeyedService.getCurrentActiveTask()).thenReturn(task2);
        transitionTask(task2, ActorTaskState.ACTING);

        when(mKeyedService.getCurrentActiveTask()).thenReturn(task1);
        mManager.processTaskUpdateQueue();
        ShadowLooper.idleMainLooper();

        assertEquals(1, mPinnedFgsNotificationId);
        assertNull(findNotification(UNPINNED_TAG, 1));
        assertNotNull(findNotification(/* tag= */ null, 1));
    }

    @Test
    public void testPendingTaskPlaceholderSwap_RemovesPlaceholderWithoutTaggedRepost() {
        int placeholderId = ActorNotificationFactory.TASK_STARTS_SOON_NOTIFICATION_ID;
        mManager.onPendingTaskCountChanged(1);
        ShadowLooper.idleMainLooper();
        assertEquals(placeholderId, mPinnedFgsNotificationId);
        assertNotNull(findNotification(/* tag= */ null, placeholderId));

        ActorTask task1 = createTask(1, "Task 1");
        transitionTask(task1, ActorTaskState.ACTING);
        mManager.onPendingTaskCountChanged(0);
        ShadowLooper.idleMainLooper();

        assertEquals(1, mPinnedFgsNotificationId);
        assertNotNull(findNotification(/* tag= */ null, 1));
        assertNull(findNotification(/* tag= */ null, placeholderId));
        assertNull(findNotification(UNPINNED_TAG, placeholderId));
        assertNull(findNotification(UNPINNED_TAG, 1));
        verify(mServiceController, never()).stopActorForegroundService(anyInt());
    }
}
