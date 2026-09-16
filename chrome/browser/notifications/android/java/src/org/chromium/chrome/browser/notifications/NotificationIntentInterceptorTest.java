// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.notifications.NotificationIntentInterceptor.INTENT_ACTION;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotificationManager;
import org.robolectric.shadows.ShadowPendingIntent;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;

/**
 * Test to verify {@link NotificationIntentInterceptor} can intercept the {@link PendingIntent} and
 * track metrics correctly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowNotificationManager.class, ShadowPendingIntent.class})
public class NotificationIntentInterceptorTest {
    private static final String TEST_NOTIFICATION_TITLE = "Test notification title";
    private static final String TEST_NOTIFICATION_ACTION_TITLE = "Test notification action title";
    private static final String EXTRA_INTENT_TYPE =
            "NotificationIntentInterceptorTest.EXTRA_INTENT_TYPE";

    private Context mContext;
    private ShadowNotificationManager mShadowNotificationManager;
    private TestReceiver mReceiver;

    /**
     * When the user clicks the notification, the intent will go through {@link
     * NotificationIntentInterceptor.Receiver} to track metrics and then arrive at this {@link
     * BroadcastReceiver}.
     */
    public static final class TestReceiver extends BroadcastReceiver {
        private static final String TEST_ACTION =
                "NotificationIntentInterceptorTest.TestReceiver.TEST_ACTION";
        private Intent mIntentReceived;

        public Intent intentReceived() {
            return mIntentReceived;
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            mIntentReceived = intent;
        }
    }

    @Before
    public void setUp() throws Exception {
        mContext = RuntimeEnvironment.application;
        mShadowNotificationManager =
                shadowOf(
                        (NotificationManager)
                                mContext.getSystemService(Context.NOTIFICATION_SERVICE));
        mContext.registerReceiver(
                new NotificationIntentInterceptor.Receiver(), new IntentFilter(INTENT_ACTION));
        mReceiver = new TestReceiver();
        mContext.registerReceiver(mReceiver, new IntentFilter(TestReceiver.TEST_ACTION));
    }

    // Builds a simple notification used in tests.
    private NotificationWrapper buildSimpleNotification(String title) {
        NotificationMetadata metaData =
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.DOWNLOAD_FILES, null, 0);
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                        ChromeChannelDefinitions.ChannelId.DOWNLOADS, metaData);

        // Set content intent.
        Intent contentIntent = new Intent(TestReceiver.TEST_ACTION);
        contentIntent.putExtra(
                EXTRA_INTENT_TYPE, NotificationIntentInterceptor.IntentType.CONTENT_INTENT);
        PendingIntentProvider contentPendingIntent =
                PendingIntentProvider.getBroadcast(mContext, 0, contentIntent, 0);
        builder.setContentIntent(contentPendingIntent);
        builder.setContentTitle(title);

        // Add a button.
        Intent actionIntent = new Intent(TestReceiver.TEST_ACTION);
        actionIntent.putExtra(
                EXTRA_INTENT_TYPE, NotificationIntentInterceptor.IntentType.ACTION_INTENT);

        // Need to use a different request code here since content intent and action intent shares
        // the same broadcast receiver.
        PendingIntentProvider actionPendingIntent =
                PendingIntentProvider.getBroadcast(mContext, /* requestCode= */ 1, actionIntent, 0);
        builder.addAction(
                0,
                TEST_NOTIFICATION_ACTION_TITLE,
                actionPendingIntent,
                NotificationUmaTracker.ActionType.DOWNLOAD_PAUSE);

        return builder.buildNotificationWrapper();
    }

    private void sendPendingIntent(PendingIntent pendingIntent) throws Exception {
        ShadowPendingIntent shadowPendingIntent = Shadows.shadowOf(pendingIntent);
        if (shadowPendingIntent.isActivity()) {
            // Simulate to send a PendingIntent by manually starting the TrampolineActivity.
            Robolectric.buildActivity(
                            NotificationIntentInterceptor.TrampolineActivity.class,
                            shadowPendingIntent.getSavedIntent())
                    .create();
        } else {
            pendingIntent.send();
        }
    }

    /**
     * Verifies {@link Notification#contentIntent} can be intercepted by {@link
     * NotificationIntentInterceptor}.
     */
    @Test
    public void testContentIntentInterception() throws Exception {
        // Send notification.
        NotificationManagerProxyImpl.getInstance()
                .notify(buildSimpleNotification(TEST_NOTIFICATION_TITLE));

        // Simulates a notification click.
        Notification notification = mShadowNotificationManager.getAllNotifications().get(0);
        Assert.assertEquals(
                TEST_NOTIFICATION_TITLE,
                notification.extras.getCharSequence(Notification.EXTRA_TITLE).toString());
        sendPendingIntent(notification.contentIntent);
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify the intent and histograms recorded.
        Intent receivedIntent = mReceiver.intentReceived();
        Assert.assertEquals(
                NotificationIntentInterceptor.IntentType.CONTENT_INTENT,
                receivedIntent.getExtras().getInt(EXTRA_INTENT_TYPE));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Content.Click2",
                        NotificationUmaTracker.SystemNotificationType.DOWNLOAD_FILES));
    }

    /**
     * Verifies {@link Notification#deleteIntent} can be intercepted by {@link
     * NotificationIntentInterceptor}.
     */
    @Test
    public void testDeleteIntentInterception() throws Exception {
        // Send notification.
        NotificationManagerProxyImpl.getInstance()
                .notify(buildSimpleNotification(TEST_NOTIFICATION_TITLE));

        // Simulates a notification cancel.
        Notification notification = mShadowNotificationManager.getAllNotifications().get(0);
        Assert.assertEquals(
                TEST_NOTIFICATION_TITLE,
                notification.extras.getCharSequence(Notification.EXTRA_TITLE).toString());
        notification.deleteIntent.send();
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify the histogram.
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Dismiss2",
                        NotificationUmaTracker.SystemNotificationType.DOWNLOAD_FILES));
        Assert.assertNull(mReceiver.intentReceived());
    }

    /** Verifies button clicks can be intercepted by {@link NotificationIntentInterceptor}. */
    @Test
    public void testActionIntentInterception() throws Exception {
        // Send notification.
        NotificationManagerProxyImpl.getInstance()
                .notify(buildSimpleNotification(TEST_NOTIFICATION_TITLE));

        // Simulates a button click.
        Notification notification = mShadowNotificationManager.getAllNotifications().get(0);
        Assert.assertEquals(
                TEST_NOTIFICATION_TITLE,
                notification.extras.getCharSequence(Notification.EXTRA_TITLE).toString());
        Assert.assertNotNull(notification.actions);
        Assert.assertEquals(1, notification.actions.length);
        Notification.Action action = notification.actions[0];
        Assert.assertNotNull(action.actionIntent);
        sendPendingIntent(action.actionIntent);
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify the intent and histograms recorded.
        Intent receivedIntent = mReceiver.intentReceived();
        Assert.assertEquals(
                NotificationIntentInterceptor.IntentType.ACTION_INTENT,
                receivedIntent.getExtras().getInt(EXTRA_INTENT_TYPE));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Mobile.SystemNotification.Action.Click",
                        NotificationUmaTracker.ActionType.DOWNLOAD_PAUSE));
    }

    /**
     * Verifies that multiple action buttons with different request codes produce distinct intercept
     * PendingIntents that do not overwrite each other.
     */
    @Test
    public void testMultipleActionIntentsHaveUniqueRequestCodes() {
        NotificationMetadata metadata =
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.SITES,
                        "notification_tag",
                        /* notificationId= */ 1);

        Intent intent0 = new Intent("action_0");
        PendingIntentProvider provider0 =
                PendingIntentProvider.getBroadcast(
                        RuntimeEnvironment.getApplication(),
                        /* requestCode= */ 0,
                        intent0,
                        PendingIntent.FLAG_UPDATE_CURRENT,
                        /* mutable= */ false);

        Intent intent1 = new Intent("action_1");
        PendingIntentProvider provider1 =
                PendingIntentProvider.getBroadcast(
                        RuntimeEnvironment.getApplication(),
                        /* requestCode= */ 1,
                        intent1,
                        PendingIntent.FLAG_UPDATE_CURRENT,
                        /* mutable= */ false);

        PendingIntent interceptIntent0 =
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.ACTION_INTENT,
                        NotificationUmaTracker.ActionType.UNKNOWN,
                        metadata,
                        provider0);

        PendingIntent interceptIntent1 =
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.ACTION_INTENT,
                        NotificationUmaTracker.ActionType.UNKNOWN,
                        metadata,
                        provider1);

        Assert.assertNotEquals(
                "PendingIntents for Action 0 and Action 1 must not be equal",
                interceptIntent0,
                interceptIntent1);
    }

    /**
     * Verifies that notifications with different tags that have identical String hash codes produce
     * distinct intercept PendingIntents and do not overwrite each other.
     */
    @Test
    public void testTagsWithSameHashCodeProduceDistinctPendingIntents() throws Exception {
        String tag1 = "p#http://localhost:8080/#1login-alert";
        String tag2 = "p#http://localhost:8081/#1(~tzmkq";
        Assert.assertEquals(tag1.hashCode(), tag2.hashCode());

        NotificationMetadata metadata1 =
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.SITES,
                        tag1,
                        /* notificationId= */ -1);
        NotificationMetadata metadata2 =
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.SITES,
                        tag2,
                        /* notificationId= */ -1);

        Intent intent1 = new Intent("action_1");
        PendingIntentProvider provider1 =
                PendingIntentProvider.getActivity(
                        RuntimeEnvironment.getApplication(),
                        /* requestCode= */ 0,
                        intent1,
                        PendingIntent.FLAG_UPDATE_CURRENT,
                        /* mutable= */ false);

        Intent intent2 = new Intent("action_2");
        PendingIntentProvider provider2 =
                PendingIntentProvider.getActivity(
                        RuntimeEnvironment.getApplication(),
                        /* requestCode= */ 0,
                        intent2,
                        PendingIntent.FLAG_UPDATE_CURRENT,
                        /* mutable= */ false);

        PendingIntent interceptIntent1 =
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.CONTENT_INTENT,
                        NotificationUmaTracker.ActionType.UNKNOWN,
                        metadata1,
                        provider1);

        PendingIntent interceptIntent2 =
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.CONTENT_INTENT,
                        NotificationUmaTracker.ActionType.UNKNOWN,
                        metadata2,
                        provider2);

        Assert.assertNotEquals(
                "PendingIntents for different notification tags must not be equal",
                interceptIntent1,
                interceptIntent2);

        ShadowPendingIntent shadowIntent1 = Shadows.shadowOf(interceptIntent1);
        ShadowPendingIntent shadowIntent2 = Shadows.shadowOf(interceptIntent2);
        Intent savedIntent1 = shadowIntent1.getSavedIntent();
        Intent savedIntent2 = shadowIntent2.getSavedIntent();
        Assert.assertNotNull(savedIntent1);
        Assert.assertNotNull(savedIntent2);

        PendingIntent innerIntent1 =
                NotificationIntentInterceptor.getPendingIntentForTesting(savedIntent1);
        PendingIntent innerIntent2 =
                NotificationIntentInterceptor.getPendingIntentForTesting(savedIntent2);
        Assert.assertEquals(provider1.getPendingIntent(), innerIntent1);
        Assert.assertEquals(provider2.getPendingIntent(), innerIntent2);
        Assert.assertNotEquals(innerIntent1, innerIntent2);

        // Verify broadcast delivery isolation for colliding tags as well.
        Intent broadcastIntent1 = new Intent(TestReceiver.TEST_ACTION);
        broadcastIntent1.setIdentifier("broadcast_inner_1");
        broadcastIntent1.putExtra("source", "provider1");
        PendingIntentProvider broadcastProvider1 =
                PendingIntentProvider.getBroadcast(
                        RuntimeEnvironment.getApplication(),
                        /* requestCode= */ 0,
                        broadcastIntent1,
                        PendingIntent.FLAG_UPDATE_CURRENT,
                        /* mutable= */ false);

        Intent broadcastIntent2 = new Intent(TestReceiver.TEST_ACTION);
        broadcastIntent2.setIdentifier("broadcast_inner_2");
        broadcastIntent2.putExtra("source", "provider2");
        PendingIntentProvider broadcastProvider2 =
                PendingIntentProvider.getBroadcast(
                        RuntimeEnvironment.getApplication(),
                        /* requestCode= */ 0,
                        broadcastIntent2,
                        PendingIntent.FLAG_UPDATE_CURRENT,
                        /* mutable= */ false);

        PendingIntent broadcastIntercept1 =
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.DELETE_INTENT,
                        NotificationUmaTracker.ActionType.UNKNOWN,
                        metadata1,
                        broadcastProvider1);

        PendingIntent broadcastIntercept2 =
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.DELETE_INTENT,
                        NotificationUmaTracker.ActionType.UNKNOWN,
                        metadata2,
                        broadcastProvider2);

        Assert.assertNotEquals(broadcastIntercept1, broadcastIntercept2);

        sendPendingIntent(broadcastIntercept1);
        RobolectricUtil.runAllBackgroundAndUi();
        Assert.assertNotNull(mReceiver.intentReceived());
        Assert.assertEquals("provider1", mReceiver.intentReceived().getStringExtra("source"));

        sendPendingIntent(broadcastIntercept2);
        RobolectricUtil.runAllBackgroundAndUi();
        Assert.assertNotNull(mReceiver.intentReceived());
        Assert.assertEquals("provider2", mReceiver.intentReceived().getStringExtra("source"));
    }

    /**
     * Verifies that null tags, empty tags, and literal "null" string tags produce distinct
     * intercept PendingIntents.
     */
    @Test
    public void testNullEmptyAndLiteralNullTagsProduceDistinctPendingIntents() {
        NotificationMetadata metadataNull =
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.SITES,
                        null,
                        /* notificationId= */ 0);
        NotificationMetadata metadataEmpty =
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.SITES,
                        "",
                        /* notificationId= */ 0);
        NotificationMetadata metadataLiteralNull =
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.SITES,
                        "null",
                        /* notificationId= */ 0);

        Intent sampleIntent = new Intent("sample_action");
        PendingIntentProvider provider =
                PendingIntentProvider.getActivity(
                        RuntimeEnvironment.getApplication(),
                        /* requestCode= */ 0,
                        sampleIntent,
                        PendingIntent.FLAG_UPDATE_CURRENT,
                        /* mutable= */ false);

        PendingIntent intentNull =
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.CONTENT_INTENT,
                        NotificationUmaTracker.ActionType.UNKNOWN,
                        metadataNull,
                        provider);
        PendingIntent intentEmpty =
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.CONTENT_INTENT,
                        NotificationUmaTracker.ActionType.UNKNOWN,
                        metadataEmpty,
                        provider);
        PendingIntent intentLiteralNull =
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.CONTENT_INTENT,
                        NotificationUmaTracker.ActionType.UNKNOWN,
                        metadataLiteralNull,
                        provider);

        Assert.assertNotEquals(intentNull, intentEmpty);
        Assert.assertNotEquals(intentNull, intentLiteralNull);
        Assert.assertNotEquals(intentEmpty, intentLiteralNull);
    }

    /**
     * Verifies that updating a notification with the same metadata and intent type correctly
     * updates the PendingIntent when FLAG_UPDATE_CURRENT is used.
     */
    @Test
    public void testSameMetadataUpdatesPendingIntent() {
        NotificationMetadata metadata =
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.SITES,
                        "sample_notification_tag",
                        /* notificationId= */ 100);

        Intent intentInitial = new Intent("initial_action");
        PendingIntentProvider providerInitial =
                PendingIntentProvider.getBroadcast(
                        RuntimeEnvironment.getApplication(),
                        /* requestCode= */ 0,
                        intentInitial,
                        PendingIntent.FLAG_UPDATE_CURRENT,
                        /* mutable= */ false);

        PendingIntent interceptIntentInitial =
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.CONTENT_INTENT,
                        NotificationUmaTracker.ActionType.UNKNOWN,
                        metadata,
                        providerInitial);

        Intent intentUpdated = new Intent("updated_action");
        PendingIntentProvider providerUpdated =
                PendingIntentProvider.getBroadcast(
                        RuntimeEnvironment.getApplication(),
                        /* requestCode= */ 0,
                        intentUpdated,
                        PendingIntent.FLAG_UPDATE_CURRENT,
                        /* mutable= */ false);

        PendingIntent interceptIntentUpdated =
                NotificationIntentInterceptor.createInterceptPendingIntent(
                        NotificationIntentInterceptor.IntentType.CONTENT_INTENT,
                        NotificationUmaTracker.ActionType.UNKNOWN,
                        metadata,
                        providerUpdated);

        Assert.assertEquals(
                "PendingIntents for the same notification metadata must be equal to allow updates",
                interceptIntentInitial,
                interceptIntentUpdated);

        ShadowPendingIntent shadow = Shadows.shadowOf(interceptIntentInitial);
        PendingIntent forwardedPendingIntent =
                NotificationIntentInterceptor.getPendingIntentForTesting(shadow.getSavedIntent());
        Assert.assertEquals(providerUpdated.getPendingIntent(), forwardedPendingIntent);
    }
}
