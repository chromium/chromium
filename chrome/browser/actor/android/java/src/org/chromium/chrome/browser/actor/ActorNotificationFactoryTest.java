// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Notification;
import android.content.Context;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.actor.ui.R;
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

    private Context mContext;
    private static final String TASK_TITLE = "Test Task";

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        ProfileResolverJni.setInstanceForTesting(mProfileResolverNatives);

        when(mTask.getId()).thenReturn(1);
        when(mTask.getTitle()).thenReturn(TASK_TITLE);
        when(mTask.getProfile()).thenReturn(mProfile);
    }

    @Test
    public void testBuildNotification_Acting() {
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
    public void testBuildNotification_Finished() {
        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(mTask, ActorTaskState.FINISHED);

        assertNotNull("Notification wrapper should not be null", wrapper);
        Notification notification = wrapper.getNotification();
        assertNotNull("Notification should not be null", notification);
        ShadowNotification shadowNotification = shadowOf(notification);

        assertEquals(
                "Content title should match task completed label",
                mContext.getString(R.string.actor_notification_title_task_completed),
                shadowNotification.getContentTitle());
        assertEquals(
                "Content text should match template with task title",
                mContext.getString(R.string.actor_notification_body_finished, TASK_TITLE),
                shadowNotification.getContentText());
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
    public void testBuildNotification_FallbackInterrupted() {
        // Use an unhandled state to trigger the fallback
        NotificationWrapper wrapper =
                ActorNotificationFactory.buildNotification(mTask, ActorTaskState.CREATED);

        assertNotNull("Notification wrapper should not be null", wrapper);
        Notification notification = wrapper.getNotification();
        assertNotNull("Notification should not be null", notification);
        ShadowNotification shadowNotification = shadowOf(notification);

        assertEquals(
                "Content title should match paused status for fallback",
                mContext.getString(R.string.actor_notification_title_task_paused),
                shadowNotification.getContentTitle());
        assertEquals(
                "Content text should match interrupted template",
                mContext.getString(R.string.actor_notification_body_interrupted, TASK_TITLE),
                shadowNotification.getContentText());
        assertTrue(
                "Notification should be ongoing",
                (notification.flags & Notification.FLAG_ONGOING_EVENT) != 0);
    }
}
