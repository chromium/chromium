// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.shadows.ShadowNotification;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.actor.ui.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationIntentInterceptor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;

/** Unit tests for {@link ActorNotificationFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ACTOR_LIVE_NOTIFICATION)
public class ActorNotificationFactoryTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActorTask mTask;
    @Mock private Profile mProfile;
    @Mock private ProfileResolver.Natives mProfileResolverNatives;
    @Mock private ActorForegroundServiceController mServiceController;
    @Mock private Activity mActivity;

    private Context mContext;
    private static final String TASK_TITLE = "Test Task";

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        ProfileResolverJni.setInstanceForTesting(mProfileResolverNatives);
        ActorForegroundServiceController.setInstanceForTesting(mServiceController);
        if (!ApplicationStatus.isInitialized()) {
            ApplicationStatus.initialize(RuntimeEnvironment.application);
        }
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);

        when(mTask.getId()).thenReturn(1);
        when(mTask.getTitle()).thenReturn(TASK_TITLE);
        when(mTask.getProfile()).thenReturn(mProfile);
        when(mServiceController.createTrustedBringTabToFrontIntent(mTask))
                .thenReturn(new Intent("DEFAULT_ACTION"));
    }

    @After
    public void tearDown() {
        ApplicationStatus.destroyForJUnitTests();
    }

    @Test
    public void testBuildNotification_Running() {
        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(
                        mTask,
                        ActorTaskState.ACTING,
                        /* isSilent= */ false,
                        /* isWarning= */ false);

        assertNotNull("Notification wrapper should not be null", wrapper);
        Notification notification = wrapper.getNotification();
        assertNotNull("Notification should not be null", notification);
        ShadowNotification shadowNotification = shadowOf(notification);

        assertEquals(
                "Content title should match status",
                mContext.getString(R.string.actor_notification_title_working_on_task),
                shadowNotification.getContentTitle());
        assertEquals(
                "Content text should match template with task title",
                mContext.getString(R.string.actor_notification_body_working, TASK_TITLE),
                shadowNotification.getContentText());
        assertEquals(
                "Big text should match content text",
                mContext.getString(R.string.actor_notification_body_working, TASK_TITLE),
                notification.extras.getCharSequence(Notification.EXTRA_BIG_TEXT));
        assertTrue(
                "Notification should be ongoing",
                (notification.flags & Notification.FLAG_ONGOING_EVENT) != 0);

        assertSmallIcon(notification);
        assertAction(notification);
    }

    @Test
    public void testBuildNotification_Paused() {
        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(
                        mTask,
                        ActorTaskState.PAUSED_BY_USER,
                        /* isSilent= */ false,
                        /* isWarning= */ false);

        assertNotNull("Notification wrapper should not be null", wrapper);
        Notification notification = wrapper.getNotification();
        assertNotNull("Notification should not be null", notification);
        ShadowNotification shadowNotification = shadowOf(notification);

        assertEquals(
                "Content title should match status",
                mContext.getString(R.string.actor_notification_title_task_paused),
                shadowNotification.getContentTitle());
        assertEquals(
                "Content text should match template with task title",
                mContext.getString(R.string.actor_notification_body_paused, TASK_TITLE),
                shadowNotification.getContentText());
        assertEquals(
                "Big text should match content text",
                mContext.getString(R.string.actor_notification_body_paused, TASK_TITLE),
                notification.extras.getCharSequence(Notification.EXTRA_BIG_TEXT));
        assertTrue(
                "Notification should be ongoing",
                (notification.flags & Notification.FLAG_ONGOING_EVENT) != 0);

        assertSmallIcon(notification);
        assertAction(notification);
    }

    @Test
    public void testBuildNotification_WaitingOnUser() {
        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(
                        mTask,
                        ActorTaskState.WAITING_ON_USER,
                        /* isSilent= */ false,
                        /* isWarning= */ false);

        assertNotNull("Notification wrapper should not be null", wrapper);
        Notification notification = wrapper.getNotification();
        assertNotNull("Notification should not be null", notification);
        ShadowNotification shadowNotification = shadowOf(notification);

        assertEquals(
                "Content title should indicate action required",
                mContext.getString(R.string.actor_notification_title_check_your_task),
                shadowNotification.getContentTitle());
        assertEquals(
                "Content text should match template with task title",
                mContext.getString(R.string.actor_notification_body_user_input, TASK_TITLE),
                shadowNotification.getContentText());
        assertEquals(
                "Big text should match content text",
                mContext.getString(R.string.actor_notification_body_user_input, TASK_TITLE),
                notification.extras.getCharSequence(Notification.EXTRA_BIG_TEXT));
        assertTrue(
                "Notification should be ongoing",
                (notification.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertNotNull("Content intent should not be null", notification.contentIntent);

        assertSmallIcon(notification);
        assertAction(notification);
    }

    @Test
    public void testBuildNotification_UsesControllerForTabRouting() {
        Intent mockIntent = new Intent("MOCK_ACTION");
        when(mServiceController.createTrustedBringTabToFrontIntent(mTask)).thenReturn(mockIntent);

        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(
                        mTask,
                        ActorTaskState.WAITING_ON_USER,
                        /* isSilent= */ false,
                        /* isWarning= */ false);

        verify(mServiceController, atLeastOnce()).createTrustedBringTabToFrontIntent(mTask);
        Notification notification = wrapper.getNotification();
        assertNotNull("Content intent should not be null", notification.contentIntent);

        Intent intent = shadowOf(notification.contentIntent).getSavedIntent();
        if (NotificationIntentInterceptor.INTENT_ACTION.equals(intent.getAction())) {
            PendingIntent wrappedPendingIntent =
                    NotificationIntentInterceptor.getPendingIntentForTesting(intent);
            intent = shadowOf(wrappedPendingIntent).getSavedIntent();
        }
        assertEquals("MOCK_ACTION", intent.getAction());
        assertSmallIcon(notification);
    }

    @Test
    public void testBuildNotification_Complete() {
        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(
                        mTask,
                        ActorTaskState.FINISHED,
                        /* isSilent= */ false,
                        /* isWarning= */ false);

        assertNotNull("Notification wrapper should not be null", wrapper);
        Notification notification = wrapper.getNotification();
        assertNotNull("Notification should not be null", notification);
        ShadowNotification shadowNotification = shadowOf(notification);

        assertEquals(
                "Content title should match task complete label",
                mContext.getString(R.string.actor_notification_title_task_complete),
                shadowNotification.getContentTitle());
        assertEquals(
                "Content text should match template with task title",
                mContext.getString(R.string.actor_notification_body_complete, TASK_TITLE),
                shadowNotification.getContentText());
        assertEquals(
                "Big text should match content text",
                mContext.getString(R.string.actor_notification_body_complete, TASK_TITLE),
                notification.extras.getCharSequence(Notification.EXTRA_BIG_TEXT));
        assertTrue(
                "Notification should be ongoing when live",
                (notification.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertTrue(
                "Notification should have auto-cancel enabled",
                (notification.flags & Notification.FLAG_AUTO_CANCEL) != 0);

        assertSmallIcon(notification);
        assertAction(notification);

        // Test non-live completed notification (after demotion).
        NotificationWrapper nonLiveWrapper =
                ActorNotificationFactory.buildNotification(
                        mTask,
                        ActorTaskState.FINISHED,
                        /* isSilent= */ true,
                        /* isWarning= */ false,
                        /* isLive= */ false);
        assertNotNull(nonLiveWrapper);
        Notification nonLiveNotification = nonLiveWrapper.getNotification();
        assertFalse(
                "Non-live completed notification should not be ongoing",
                (nonLiveNotification.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertFalse(
                "Non-live completed notification should not request promoted ongoing",
                nonLiveNotification.extras.getBoolean(
                        ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
        assertNull(
                (Object)
                        nonLiveNotification.extras.getCharSequence(
                                ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));
        assertTrue(
                "Non-live completed notification should have auto-cancel enabled",
                (nonLiveNotification.flags & Notification.FLAG_AUTO_CANCEL) != 0);
    }

    @Test
    public void testBuildNotification_Reflecting() {
        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(
                        mTask,
                        ActorTaskState.REFLECTING,
                        /* isSilent= */ false,
                        /* isWarning= */ false);

        assertNotNull("Notification wrapper should not be null", wrapper);
        Notification notification = wrapper.getNotification();
        assertNotNull("Notification should not be null", notification);
        ShadowNotification shadowNotification = shadowOf(notification);

        assertEquals(
                "Content title should match reflecting status",
                mContext.getString(R.string.actor_notification_title_working_on_task),
                shadowNotification.getContentTitle());
        assertEquals(
                "Content text should match reflecting body template",
                mContext.getString(R.string.actor_notification_body_working, TASK_TITLE),
                shadowNotification.getContentText());
        assertEquals(
                "Big text should match content text",
                mContext.getString(R.string.actor_notification_body_working, TASK_TITLE),
                notification.extras.getCharSequence(Notification.EXTRA_BIG_TEXT));
        assertTrue(
                "Notification should be ongoing",
                (notification.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertNotNull("Content intent should be set", notification.contentIntent);
        assertSmallIcon(notification);
        assertAction(notification);
    }

    @Test
    public void testBuildNotification_PausedByActor() {
        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(
                        mTask,
                        ActorTaskState.PAUSED_BY_ACTOR,
                        /* isSilent= */ false,
                        /* isWarning= */ false);

        assertNotNull("Notification wrapper should not be null", wrapper);
        Notification notification = wrapper.getNotification();
        assertEquals(
                "Content title should match status",
                mContext.getString(R.string.actor_notification_title_task_paused),
                shadowOf(notification).getContentTitle());
        assertNotNull("Content intent should be set", notification.contentIntent);
        assertSmallIcon(notification);
        assertAction(notification);
    }

    @Test
    public void testBuildNotification_Stopped() {
        // Use an unhandled state to trigger the fallback
        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(
                        mTask,
                        ActorTaskState.FAILED,
                        /* isSilent= */ false,
                        /* isWarning= */ false);

        assertNotNull("Notification wrapper should not be null", wrapper);
        Notification notification = wrapper.getNotification();
        assertNotNull("Notification should not be null", notification);
        ShadowNotification shadowNotification = shadowOf(notification);

        assertEquals(
                "Content title should match stopped status for fallback",
                mContext.getString(R.string.actor_notification_title_task_stopped),
                shadowNotification.getContentTitle());
        assertEquals(
                "Content text should match stopped template",
                mContext.getString(R.string.actor_notification_body_stopped, TASK_TITLE),
                shadowNotification.getContentText());
        assertEquals(
                "Big text should match content text",
                mContext.getString(R.string.actor_notification_body_stopped, TASK_TITLE),
                notification.extras.getCharSequence(Notification.EXTRA_BIG_TEXT));
        assertTrue(
                "Stopped notification should be ongoing when live",
                (notification.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertTrue(
                "Stopped notification should request promoted ongoing",
                notification.extras.getBoolean(
                        ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
        assertEquals(
                "Stopped notification status chip should be Stopped",
                mContext.getString(R.string.actor_notification_live_status_stopped),
                notification.extras.getCharSequence(
                        ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));
        assertNotNull("Content intent should be set", notification.contentIntent);
        assertSmallIcon(notification);
        assertAction(notification);

        // Test non-live stopped notification (after demotion).
        NotificationWrapper nonLiveWrapper =
                ActorNotificationFactory.buildNotification(
                        mTask,
                        ActorTaskState.FAILED,
                        /* isSilent= */ true,
                        /* isWarning= */ false,
                        /* isLive= */ false);
        assertNotNull(nonLiveWrapper);
        Notification nonLiveNotification = nonLiveWrapper.getNotification();
        assertFalse(
                "Non-live stopped notification should not be ongoing",
                (nonLiveNotification.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertFalse(
                "Non-live stopped notification should not request promoted ongoing",
                nonLiveNotification.extras.getBoolean(
                        ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
        assertNull(
                (Object)
                        nonLiveNotification.extras.getCharSequence(
                                ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));
        assertTrue(
                "Non-live stopped notification should have auto-cancel enabled",
                (nonLiveNotification.flags & Notification.FLAG_AUTO_CANCEL) != 0);
    }

    @Test
    public void testBuildNotification_Silencing_Background() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(
                        mTask, ActorTaskState.ACTING, /* isSilent= */ true, /* isWarning= */ false);

        assertNotNull("Notification wrapper should not be null", wrapper);
        assertTrue("Notification should be silent", wrapper.isSilent());
    }

    @Test
    public void testBuildNotification_Warning_LongRunning() {
        // PAUSED_BY_ACTOR indicates the task hit the running budget.
        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(
                        mTask,
                        ActorTaskState.PAUSED_BY_ACTOR,
                        /* isSilent= */ false,
                        /* isWarning= */ true);

        assertNotNull("Notification wrapper should not be null", wrapper);
        Notification notification = wrapper.getNotification();
        ShadowNotification shadowNotification = shadowOf(notification);

        assertEquals(
                "Content text should match long running warning template",
                mContext.getString(
                        R.string.actor_notification_body_will_stop_task_long_running, TASK_TITLE),
                shadowNotification.getContentText());
        assertTrue(
                "Warning notification should be ongoing",
                (notification.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertTrue(
                "Warning notification should request promoted ongoing",
                notification.extras.getBoolean(
                        ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
        assertEquals(
                "Warning notification status chip should be Review",
                mContext.getString(R.string.actor_notification_live_status_review),
                notification.extras.getCharSequence(
                        ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));
    }

    @Test
    public void testBuildNotification_Warning_NoResponse() {
        // Other idle states use the 'no response' string.
        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(
                        mTask,
                        ActorTaskState.WAITING_ON_USER,
                        /* isSilent= */ false,
                        /* isWarning= */ true);

        assertNotNull("Notification wrapper should not be null", wrapper);
        Notification notification = wrapper.getNotification();
        ShadowNotification shadowNotification = shadowOf(notification);

        assertEquals(
                "Content text should match no response warning template",
                mContext.getString(
                        R.string.actor_notification_body_will_stop_task_no_response, TASK_TITLE),
                shadowNotification.getContentText());
        assertTrue(
                "Warning notification should be ongoing",
                (notification.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertTrue(
                "Warning notification should request promoted ongoing",
                notification.extras.getBoolean(
                        ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
        assertEquals(
                "Warning notification status chip should be Review",
                mContext.getString(R.string.actor_notification_live_status_review),
                notification.extras.getCharSequence(
                        ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));
    }

    @Test
    public void testShouldUpdateNotification() {
        // State change same category
        assertFalse(
                ActorNotificationFactory.shouldUpdateNotification(
                        ActorTaskState.ACTING, ActorTaskState.REFLECTING));

        // State change different category
        assertTrue(
                ActorNotificationFactory.shouldUpdateNotification(
                        ActorTaskState.ACTING, ActorTaskState.PAUSED_BY_USER));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ACTOR_STEP_PROGRESS_NOTIFICATION)
    public void testBuildNotification_WithStepText_FlagEnabled() {
        when(mTask.getCurrentActionName()).thenReturn("Navigating to site");
        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(
                        mTask,
                        ActorTaskState.ACTING,
                        /* isSilent= */ false,
                        /* isWarning= */ false);

        assertNotNull("Notification wrapper should not be null", wrapper);
        Notification notification = wrapper.getNotification();
        assertNotNull("Notification should not be null", notification);
        ShadowNotification shadowNotification = shadowOf(notification);

        String expectedBody =
                mContext.getString(
                        R.string.actor_notification_body_working_with_step_info,
                        TASK_TITLE,
                        "Navigating to site");
        assertEquals(
                "Content text should match task title and step text",
                expectedBody,
                shadowNotification.getContentText());
        assertEquals(
                "Big text should match task title and step text",
                expectedBody,
                notification.extras.getCharSequence(Notification.EXTRA_BIG_TEXT));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ACTOR_STEP_PROGRESS_NOTIFICATION)
    public void testBuildNotification_WithStepText_FlagDisabled() {
        when(mTask.getCurrentActionName()).thenReturn("Navigating to site");
        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(
                        mTask,
                        ActorTaskState.ACTING,
                        /* isSilent= */ false,
                        /* isWarning= */ false);

        assertNotNull("Notification wrapper should not be null", wrapper);
        Notification notification = wrapper.getNotification();
        assertNotNull("Notification should not be null", notification);
        ShadowNotification shadowNotification = shadowOf(notification);

        String expectedBody =
                mContext.getString(R.string.actor_notification_body_working, TASK_TITLE);
        assertEquals(
                "Content text should use default working body when flag is disabled",
                expectedBody,
                shadowNotification.getContentText());
    }

    @Test
    public void testBuildTaskStartsSoonNotification() {
        NotificationWrapper wrapper = ActorNotificationFactory.buildTaskStartsSoonNotification();

        assertNotNull("Notification wrapper should not be null", wrapper);
        Notification notification = wrapper.getNotification();
        assertNotNull("Notification should not be null", notification);
        ShadowNotification shadowNotification = shadowOf(notification);

        assertEquals(
                "Content title should match",
                mContext.getString(R.string.actor_notification_title_preparing_to_start_task),
                shadowNotification.getContentTitle());
        assertNull("Content text should be null", shadowNotification.getContentText());
        assertEquals(
                "Small icon should be ic_chrome",
                R.drawable.ic_chrome,
                notification.getSmallIcon().getResId());

        assertEquals(
                "Notification ID should match",
                ActorNotificationFactory.TASK_STARTS_SOON_NOTIFICATION_ID,
                wrapper.getMetadata().id);
    }

    @Test
    public void testBuildNotification_LiveNotificationProperties() {
        // Active acting task: requested promoted ongoing, null shortCriticalText (icon only chip).
        NotificationWrapper actingWrapper =
                ActorNotificationFactory.buildNotification(
                        mTask,
                        ActorTaskState.ACTING,
                        /* isSilent= */ false,
                        /* isWarning= */ false);
        assertNotNull(actingWrapper);
        Notification actingNotif = actingWrapper.getNotification();
        assertTrue(
                "Acting notification should be ongoing",
                (actingNotif.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertTrue(
                "Acting notification should request promoted ongoing",
                actingNotif.extras.getBoolean(
                        ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
        assertNull(
                (Object)
                        actingNotif.extras.getCharSequence(
                                ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));

        // Waiting on user task: requested promoted ongoing, shortCriticalText = "Review".
        NotificationWrapper waitingWrapper =
                ActorNotificationFactory.buildNotification(
                        mTask,
                        ActorTaskState.WAITING_ON_USER,
                        /* isSilent= */ false,
                        /* isWarning= */ false);
        assertNotNull(waitingWrapper);
        Notification waitingNotif = waitingWrapper.getNotification();
        assertEquals(
                "Waiting on user status chip should be Review",
                mContext.getString(R.string.actor_notification_live_status_review),
                waitingNotif.extras.getCharSequence(
                        ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));

        // Paused task: requested promoted ongoing, shortCriticalText = "Paused".
        NotificationWrapper pausedWrapper =
                ActorNotificationFactory.buildNotification(
                        mTask,
                        ActorTaskState.PAUSED_BY_USER,
                        /* isSilent= */ false,
                        /* isWarning= */ false);
        assertNotNull(pausedWrapper);
        Notification pausedNotif = pausedWrapper.getNotification();
        assertEquals(
                "Paused task status chip should be Paused",
                mContext.getString(R.string.actor_notification_live_status_paused),
                pausedNotif.extras.getCharSequence(
                        ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));

        // Warning state (running task in warning): requested promoted ongoing,
        // shortCriticalText = "Review".
        NotificationWrapper warningWrapper =
                ActorNotificationFactory.buildNotification(
                        mTask, ActorTaskState.ACTING, /* isSilent= */ false, /* isWarning= */ true);
        assertNotNull(warningWrapper);
        Notification warningNotif = warningWrapper.getNotification();
        assertTrue(
                "Warning notification should request promoted ongoing",
                warningNotif.extras.getBoolean(
                        ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
        assertEquals(
                "Warning state status chip should be Review",
                mContext.getString(R.string.actor_notification_live_status_review),
                warningNotif.extras.getCharSequence(
                        ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));

        // Warning state (waiting on user task in warning): requested promoted ongoing,
        // shortCriticalText = "Review".
        NotificationWrapper waitingWarningWrapper =
                ActorNotificationFactory.buildNotification(
                        mTask,
                        ActorTaskState.WAITING_ON_USER,
                        /* isSilent= */ false,
                        /* isWarning= */ true);
        assertNotNull(waitingWarningWrapper);
        Notification waitingWarningNotif = waitingWarningWrapper.getNotification();
        assertEquals(
                "Waiting on user warning status chip should still be Review",
                mContext.getString(R.string.actor_notification_live_status_review),
                waitingWarningNotif.extras.getCharSequence(
                        ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));

        // Finished task: requested promoted ongoing, shortCriticalText = "Done".
        NotificationWrapper finishedWrapper =
                ActorNotificationFactory.buildNotification(
                        mTask,
                        ActorTaskState.FINISHED,
                        /* isSilent= */ false,
                        /* isWarning= */ false);
        assertNotNull(finishedWrapper);
        Notification finishedNotif = finishedWrapper.getNotification();
        assertTrue(
                "Finished notification should be ongoing when live",
                (finishedNotif.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertTrue(
                "Finished notification should request promoted ongoing",
                finishedNotif.extras.getBoolean(
                        ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
        assertEquals(
                "Finished notification status chip should be Done",
                mContext.getString(R.string.actor_notification_live_status_done),
                finishedNotif.extras.getCharSequence(
                        ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));

        // Stopped / Cancelled terminal task: should request promoted ongoing,
        // shortCriticalText = "Stopped", ongoing when live.
        NotificationWrapper stoppedWrapper =
                ActorNotificationFactory.buildNotification(
                        mTask,
                        ActorTaskState.FAILED,
                        /* isSilent= */ false,
                        /* isWarning= */ false);
        assertNotNull(stoppedWrapper);
        Notification stoppedNotif = stoppedWrapper.getNotification();
        assertTrue(
                "Stopped live notification should be ongoing",
                (stoppedNotif.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertTrue(
                "Stopped live notification should request promoted ongoing",
                stoppedNotif.extras.getBoolean(
                        ActorNotificationFactory.EXTRA_REQUEST_PROMOTED_ONGOING));
        assertEquals(
                "Stopped live notification status chip should be Stopped",
                mContext.getString(R.string.actor_notification_live_status_stopped),
                stoppedNotif.extras.getCharSequence(
                        ActorNotificationFactory.EXTRA_SHORT_CRITICAL_TEXT));
    }

    private void assertSmallIcon(Notification notification) {
        assertNotNull("Small icon should not be null", notification.getSmallIcon());
        assertEquals(
                "Small icon should be ic_chrome",
                R.drawable.ic_chrome,
                notification.getSmallIcon().getResId());
    }

    private void assertAction(Notification notification) {
        assertNotNull("Actions should not be null", notification.actions);
        assertEquals("Should have 1 action", 1, notification.actions.length);
        assertEquals(
                "Action title should match",
                mContext.getString(R.string.actor_notification_button_go_to_chrome),
                notification.actions[0].title);
        assertEquals(
                "Action icon should be ic_chrome",
                R.drawable.ic_chrome,
                notification.actions[0].getIcon().getResId());
    }
}
