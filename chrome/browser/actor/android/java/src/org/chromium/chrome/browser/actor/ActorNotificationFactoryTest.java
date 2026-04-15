// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
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
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotification;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.actor.ui.R;
import org.chromium.chrome.browser.notifications.NotificationIntentInterceptor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;

/** Unit tests for {@link ActorNotificationFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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
                ActorNotificationFactory.buildNotification(mTask, ActorTaskState.ACTING);

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

        assertEquals("Should have 2 actions", 2, notification.actions.length);
        assertEquals(
                "First action should be 'View task'",
                mContext.getString(R.string.actor_notification_button_view_task),
                notification.actions[0].title);
        assertEquals(
                "Second action should be 'Pause task'",
                mContext.getString(R.string.actor_notification_button_pause_task),
                notification.actions[1].title);
    }

    @Test
    public void testBuildNotification_Paused() {
        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(mTask, ActorTaskState.PAUSED_BY_USER);

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

        assertEquals("Should have 2 actions", 2, notification.actions.length);
        assertEquals(
                "First action should be 'View task'",
                mContext.getString(R.string.actor_notification_button_view_task),
                notification.actions[0].title);
        assertEquals(
                "Second action should be 'Resume task'",
                mContext.getString(R.string.actor_notification_button_resume_task),
                notification.actions[1].title);
    }

    @Test
    public void testBuildNotification_WaitingOnUser() {
        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(mTask, ActorTaskState.WAITING_ON_USER);

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

        assertEquals("Should have 1 action", 1, notification.actions.length);
        assertEquals(
                "First action should be 'View task'",
                mContext.getString(R.string.actor_notification_button_view_task),
                notification.actions[0].title);
    }

    @Test
    public void testBuildNotification_UsesControllerForTabRouting() {
        Intent mockIntent = new Intent("MOCK_ACTION");
        when(mServiceController.createTrustedBringTabToFrontIntent(mTask)).thenReturn(mockIntent);

        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(mTask, ActorTaskState.WAITING_ON_USER);

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
    }

    @Test
    public void testBuildNotification_Complete() {
        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(mTask, ActorTaskState.FINISHED);

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
        assertFalse(
                "Notification should not be ongoing",
                (notification.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertTrue(
                "Notification should have auto-cancel enabled",
                (notification.flags & Notification.FLAG_AUTO_CANCEL) != 0);

        assertEquals("Should have 1 action", 1, notification.actions.length);
        assertEquals(
                "First action should be 'View task'",
                mContext.getString(R.string.actor_notification_button_view_task),
                notification.actions[0].title);
    }

    @Test
    public void testBuildNotification_Reflecting() {
        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(mTask, ActorTaskState.REFLECTING);

        assertNotNull("Notification wrapper should not be null", wrapper);
        Notification notification = wrapper.getNotification();
        assertEquals(
                "Content title should match status",
                mContext.getString(R.string.actor_notification_title_working_on_task),
                shadowOf(notification).getContentTitle());
        assertNotNull("Content intent should be set", notification.contentIntent);
        assertEquals("Should have 2 actions", 2, notification.actions.length);
        assertEquals(
                "First action should be 'View task'",
                mContext.getString(R.string.actor_notification_button_view_task),
                notification.actions[0].title);
    }

    @Test
    public void testBuildNotification_PausedByActor() {
        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(mTask, ActorTaskState.PAUSED_BY_ACTOR);

        assertNotNull("Notification wrapper should not be null", wrapper);
        Notification notification = wrapper.getNotification();
        assertEquals(
                "Content title should match status",
                mContext.getString(R.string.actor_notification_title_task_paused),
                shadowOf(notification).getContentTitle());
        assertNotNull("Content intent should be set", notification.contentIntent);
        assertEquals("Should have 2 actions", 2, notification.actions.length);
        assertEquals(
                "First action should be 'View task'",
                mContext.getString(R.string.actor_notification_button_view_task),
                notification.actions[0].title);
    }

    @Test
    public void testBuildNotification_Interrupted() {
        // Use an unhandled state to trigger the fallback
        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(mTask, ActorTaskState.FAILED);

        assertNotNull("Notification wrapper should not be null", wrapper);
        Notification notification = wrapper.getNotification();
        assertNotNull("Notification should not be null", notification);
        ShadowNotification shadowNotification = shadowOf(notification);

        assertEquals(
                "Content title should match interrupted status for fallback",
                mContext.getString(R.string.actor_notification_title_task_interrupted),
                shadowNotification.getContentTitle());
        assertEquals(
                "Content text should match interrupted template",
                mContext.getString(R.string.actor_notification_body_interrupted, TASK_TITLE),
                shadowNotification.getContentText());
        assertEquals(
                "Big text should match content text",
                mContext.getString(R.string.actor_notification_body_interrupted, TASK_TITLE),
                notification.extras.getCharSequence(Notification.EXTRA_BIG_TEXT));
        assertFalse(
                "Notification should not be ongoing",
                (notification.flags & Notification.FLAG_ONGOING_EVENT) != 0);
        assertNotNull("Content intent should be set", notification.contentIntent);
    }

    @Test
    public void testBuildNotification_NullRoutingIntent() {
        when(mServiceController.createTrustedBringTabToFrontIntent(mTask)).thenReturn(null);

        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(mTask, ActorTaskState.ACTING);

        Notification notification = wrapper.getNotification();
        // Acting notification normally has 2 actions: View and Pause.
        // If View is missing, it should only have 1 (Pause).
        assertEquals(
                "Should have only 1 action when routing intent is null",
                1,
                notification.actions.length);
        assertEquals(
                "Remaining action should be 'Pause task'",
                mContext.getString(R.string.actor_notification_button_pause_task),
                notification.actions[0].title);
    }

    @Test
    public void testBuildNotification_Silencing_Background() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(mTask, ActorTaskState.WAITING_ON_USER);
        assertFalse("Notification should not be silent in background", wrapper.isSilent());
    }

    @Test
    public void testBuildNotification_Silencing_Foreground() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        when(mActivity.isInPictureInPictureMode()).thenReturn(false);

        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(mTask, ActorTaskState.WAITING_ON_USER);
        assertTrue("Notification should be silent in foreground", wrapper.isSilent());
    }

    @Test
    public void testBuildNotification_Silencing_PiP() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        when(mActivity.isInPictureInPictureMode()).thenReturn(true);

        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(mTask, ActorTaskState.WAITING_ON_USER);
        assertFalse("Notification should not be silent in PiP", wrapper.isSilent());
    }
}
