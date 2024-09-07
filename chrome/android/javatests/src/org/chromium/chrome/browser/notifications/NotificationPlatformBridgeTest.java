// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.chromium.components.content_settings.PrefNames.NOTIFICATIONS_VIBRATE_ENABLED;

import android.app.Notification;
import android.app.PendingIntent;
import android.app.RemoteInput;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;

import androidx.annotation.RequiresApi;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.permissions.PermissionTestRule;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy.NotificationEntry;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.site_engagement.SiteEngagementService;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.url.GURL;

import java.net.URL;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for the Notification Bridge.
 *
 * <p>Web Notifications are only supported on Android JellyBean and beyond.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class NotificationPlatformBridgeTest {
    @Rule public PermissionTestRule mPermissionTestRule = new PermissionTestRule();

    @Rule public NotificationTestRule mNotificationTestRule = new NotificationTestRule();

    private static final String NOTIFICATION_TEST_PAGE =
            "/chrome/test/data/notifications/android_test.html";
    private static final int TITLE_UPDATE_TIMEOUT_SECONDS = (int) 5L;

    @Before
    public void setUp() {
        SiteEngagementService.setParamValuesForTesting();
        mNotificationTestRule.loadUrl(mPermissionTestRule.getURL(NOTIFICATION_TEST_PAGE));
        mPermissionTestRule.setActivity(mNotificationTestRule.getActivity());
    }

    @SuppressWarnings("MissingFail")
    private void waitForTitle(String expectedTitle) {
        Tab tab = mNotificationTestRule.getActivity().getActivityTab();
        TabTitleObserver titleObserver = new TabTitleObserver(tab, expectedTitle);
        try {
            titleObserver.waitForTitleUpdate(TITLE_UPDATE_TIMEOUT_SECONDS);

        } catch (TimeoutException e) {
            // The title is not as expected, this assertion neatly logs what the difference is.
            Assert.assertEquals(expectedTitle, ChromeTabUtils.getTitleOnUiThread(tab));
        }
    }

    private void checkThatShowNotificationIsDenied() throws Exception {
        showNotification("MyNotification", "{}");
        waitForTitle(
                "TypeError: Failed to execute 'showNotification' on 'ServiceWorkerRegistration': "
                        + "No notification permission has been granted for this origin.");

        // Ideally we'd wait a little here, but it's hard to wait for things that shouldn't happen.
        Assert.assertTrue(mNotificationTestRule.getNotificationEntries().isEmpty());
    }

    private double getEngagementScoreBlocking() {
        // TODO (https://crbug.com/1063807):  Add incognito mode tests.
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        SiteEngagementService.getForBrowserContext(
                                        ProfileManager.getLastUsedRegularProfile())
                                .getScore(mPermissionTestRule.getOrigin()));
    }

    /**
     * Verifies that notifcations cannot be shown without permission, and that dismissing or denying
     * the infobar works correctly.
     */
    @LargeTest
    @Feature({"Browser", "Notifications"})
    @Test
    @DisabledTest(message = "https://crbug.com/1435133")
    public void testPermissionDenied() throws Exception {
        // Notifications permission should initially be prompt, and showing should fail.
        Assert.assertEquals("\"default\"", runJavaScript("Notification.permission"));
        checkThatShowNotificationIsDenied();

        PermissionTestRule.PermissionUpdateWaiter updateWaiter =
                new PermissionTestRule.PermissionUpdateWaiter(
                        "denied: ", mNotificationTestRule.getActivity());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mNotificationTestRule.getActivity().getActivityTab().addObserver(updateWaiter);
                });

        mPermissionTestRule.runDenyTest(
                updateWaiter,
                NOTIFICATION_TEST_PAGE,
                "Notification.requestPermission(addCountAndSendToTest)",
                1,
                false,
                true);

        // This should have caused notifications permission to become denied.
        Assert.assertEquals("\"denied\"", runJavaScript("Notification.permission"));
        checkThatShowNotificationIsDenied();

        // Reload page to ensure the block is persisted.
        mNotificationTestRule.loadUrl(mPermissionTestRule.getURL(NOTIFICATION_TEST_PAGE));

        // Notification.requestPermission() should immediately pass denied to the callback without
        // showing a dialog.
        runJavaScript("Notification.requestPermission(sendToTest)");
        waitForTitle("denied");
        Assert.assertFalse(PermissionDialogController.getInstance().isDialogShownForTest());

        // Notifications permission should still be denied.
        Assert.assertEquals("\"denied\"", runJavaScript("Notification.permission"));
        checkThatShowNotificationIsDenied();
    }

    /** Verifies granting permission via the infobar. */
    @MediumTest
    @Feature({"Browser", "Notifications"})
    @Test
    @DisabledTest(message = "https://crbug.com/1435133")
    public void testPermissionGranted() throws Exception {
        // Notifications permission should initially be prompt, and showing should fail.
        Assert.assertEquals("\"default\"", runJavaScript("Notification.permission"));
        checkThatShowNotificationIsDenied();

        PermissionTestRule.PermissionUpdateWaiter updateWaiter =
                new PermissionTestRule.PermissionUpdateWaiter(
                        "granted: ", mNotificationTestRule.getActivity());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mNotificationTestRule.getActivity().getActivityTab().addObserver(updateWaiter);
                });

        mPermissionTestRule.runAllowTest(
                updateWaiter,
                NOTIFICATION_TEST_PAGE,
                "Notification.requestPermission(addCountAndSendToTest)",
                1,
                false,
                true);

        // Reload page to ensure the grant is persisted.
        mNotificationTestRule.loadUrl(mPermissionTestRule.getURL(NOTIFICATION_TEST_PAGE));

        // Notifications permission should now be granted, and showing should succeed.
        Assert.assertEquals("\"granted\"", runJavaScript("Notification.permission"));
        showAndGetNotification("MyNotification", "{}");
    }

    /**
     * Verifies that the intended default properties of a notification will indeed be set on the
     * Notification object that will be send to Android.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testDefaultNotificationProperties() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());
        Context context = ApplicationProvider.getApplicationContext();

        Notification notification = showAndGetNotification("MyNotification", "{body: 'Hello'}");

        String expectedOrigin =
                UrlFormatter.formatUrlForSecurityDisplay(
                        mPermissionTestRule.getOrigin(), SchemeDisplay.OMIT_HTTP_AND_HTTPS);

        // Validate the contents of the notification.
        Assert.assertEquals("MyNotification", NotificationTestUtil.getExtraTitle(notification));
        Assert.assertEquals("Hello", NotificationTestUtil.getExtraText(notification));
        Assert.assertEquals(expectedOrigin, NotificationTestUtil.getExtraSubText(notification));

        // Verify that the ticker text contains the notification's title and body.
        String tickerText = notification.tickerText.toString();

        Assert.assertTrue(tickerText.contains("MyNotification"));
        Assert.assertTrue(tickerText.contains("Hello"));

        // Verify the public version of the notification contains the notification's origin,
        // and that the body text has been replaced.
        Assert.assertNotNull(notification.publicVersion);
        Assert.assertEquals(
                context.getString(R.string.notification_hidden_text),
                NotificationTestUtil.getExtraText(notification.publicVersion));
        // On N+, origin should be set as the subtext of the public notification.
        Assert.assertEquals(
                expectedOrigin, NotificationTestUtil.getExtraSubText(notification.publicVersion));

        // Verify that the notification's timestamp is set in the past 60 seconds. This number has
        // no significance, but needs to be high enough to not cause flakiness as it's set by the
        // renderer process on notification creation.
        Assert.assertTrue(Math.abs(System.currentTimeMillis() - notification.when) < 60 * 1000);

        boolean timestampIsShown = NotificationTestUtil.getExtraShownWhen(notification);
        Assert.assertTrue("Timestamp should be shown", timestampIsShown);

        Assert.assertNotNull(
                NotificationTestUtil.getLargeIconFromNotification(context, notification));

        // Validate the notification's behavior. On Android O+ the defaults are ignored as vibrate
        // and silent moved to the notification channel. The silent flag is achieved by using a
        // group alert summary.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Assert.assertEquals(0, notification.defaults);
            Assert.assertEquals(Notification.GROUP_ALERT_ALL, notification.getGroupAlertBehavior());
        } else {
            Assert.assertEquals(Notification.DEFAULT_ALL, notification.defaults);
        }
        Assert.assertEquals(Notification.PRIORITY_DEFAULT, notification.priority);
    }

    /**
     * Verifies that specifying a notification action with type: 'text' results in a notification
     * with a remote input on the action.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testShowNotificationWithTextAction() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());

        Notification notification =
                showAndGetNotification(
                        "MyNotification",
                        "{ "
                                + " actions: [{action: 'myAction', title: 'reply', type: 'text',"
                                + " placeholder: 'hi' }]}");

        // The specified action should be present, as well as a default settings action.
        Assert.assertEquals(2, notification.actions.length);

        Notification.Action action = notification.actions[0];
        Assert.assertEquals("reply", action.title);
        Assert.assertNotNull(notification.actions[0].getRemoteInputs());
        Assert.assertEquals(1, action.getRemoteInputs().length);
        Assert.assertEquals("hi", action.getRemoteInputs()[0].getLabel());
    }

    /**
     * Verifies that setting a reply on the remote input of a notification action with type 'text'
     * and triggering the action's intent causes the same reply to be received in the subsequent
     * notificationclick event on the service worker. Verifies that site engagement is incremented
     * appropriately.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testReplyToNotification() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());
        Context context = ApplicationProvider.getApplicationContext();

        UserActionTester actionTester = new UserActionTester();

        // +0.5 engagement from navigating to the test page.
        Assert.assertEquals(0.5, getEngagementScoreBlocking(), 0);
        runJavaScript("SetupReplyForwardingForTests();");
        Notification notification =
                showAndGetNotification(
                        "MyNotification",
                        "{ "
                                + " actions: [{action: 'myAction', title: 'reply', type: 'text'}],"
                                + " data: 'ACTION_REPLY'}");

        // Check the action is present with a remote input attached.
        Notification.Action action = notification.actions[0];
        Assert.assertEquals("reply", action.title);
        RemoteInput[] remoteInputs = action.getRemoteInputs();
        Assert.assertNotNull(remoteInputs);

        // Set a reply using the action's remote input key and send it on the intent.
        sendIntentWithRemoteInput(
                context,
                action.actionIntent,
                remoteInputs,
                remoteInputs[0].getResultKey(),
                /* reply= */ "My Reply");

        // Check reply was received by the service worker (see android_test_worker.js).
        // Expect +1 engagement from interacting with the notification.
        waitForTitle("reply: My Reply");
        Assert.assertEquals(1.5, getEngagementScoreBlocking(), 0);

        // Replies are always delivered to an action button.
        assertThat(
                actionTester.toString(),
                getNotificationActions(actionTester),
                Matchers.hasItems(
                        "Notifications.Persistent.Shown",
                        "Notifications.Persistent.ClickedActionButton"));
    }

    /**
     * Verifies that setting an empty reply on the remote input of a notification action with type
     * 'text' and triggering the action's intent causes an empty reply string to be received in the
     * subsequent notificationclick event on the service worker. Verifies that site engagement is
     * incremented appropriately.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testReplyToNotificationWithEmptyReply() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());
        Context context = ApplicationProvider.getApplicationContext();

        // +0.5 engagement from navigating to the test page.
        Assert.assertEquals(0.5, getEngagementScoreBlocking(), 0);
        runJavaScript("SetupReplyForwardingForTests();");
        Notification notification =
                showAndGetNotification(
                        "MyNotification",
                        "{ "
                                + " actions: [{action: 'myAction', title: 'reply', type: 'text'}],"
                                + " data: 'ACTION_REPLY'}");

        // Check the action is present with a remote input attached.
        Notification.Action action = notification.actions[0];
        Assert.assertEquals("reply", action.title);
        RemoteInput[] remoteInputs = action.getRemoteInputs();
        Assert.assertNotNull(remoteInputs);

        // Set a reply using the action's remote input key and send it on the intent.
        sendIntentWithRemoteInput(
                context,
                action.actionIntent,
                remoteInputs,
                remoteInputs[0].getResultKey(),
                /* reply= */ "");

        // Check empty reply was received by the service worker (see android_test_worker.js).
        // Expect +1 engagement from interacting with the notification.
        waitForTitle("reply:");
        Assert.assertEquals(1.5, getEngagementScoreBlocking(), 0);
    }

    private static void sendIntentWithRemoteInput(
            Context context,
            PendingIntent pendingIntent,
            RemoteInput[] remoteInputs,
            String resultKey,
            String reply)
            throws PendingIntent.CanceledException {
        Bundle results = new Bundle();
        results.putString(resultKey, reply);
        Intent fillInIntent = new Intent().addFlags(Intent.FLAG_RECEIVER_FOREGROUND);
        RemoteInput.addResultsToIntent(remoteInputs, fillInIntent, results);

        // Send the pending intent filled in with the additional information from the new
        // intent.
        pendingIntent.send(context, /* code= */ 0, fillInIntent);
    }

    /**
     * Verifies that *not* setting a reply on the remote input of a notification action with type
     * 'text' and triggering the action's intent causes a null reply to be received in the
     * subsequent notificationclick event on the service worker. Verifies that site engagement is
     * incremented appropriately.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testReplyToNotificationWithNoRemoteInput() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());

        // +0.5 engagement from navigating to the test page.
        Assert.assertEquals(0.5, getEngagementScoreBlocking(), 0);
        runJavaScript("SetupReplyForwardingForTests();");
        Notification notification =
                showAndGetNotification(
                        "MyNotification",
                        "{ "
                                + " actions: [{action: 'myAction', title: 'reply', type: 'text'}],"
                                + " data: 'ACTION_REPLY'}");

        Assert.assertEquals("reply", notification.actions[0].title);
        notification.actions[0].actionIntent.send();

        // Check reply was received by the service worker (see android_test_worker.js).
        // Expect +1 engagement from interacting with the notification.
        waitForTitle("reply: null");
        Assert.assertEquals(1.5, getEngagementScoreBlocking(), 0);
    }

    /** Verifies that the ONLY_ALERT_ONCE flag is not set when renotify is true. */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationRenotifyProperty() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());

        Notification notification =
                showAndGetNotification("MyNotification", "{ tag: 'myTag', renotify: true }");

        Assert.assertEquals(0, notification.flags & Notification.FLAG_ONLY_ALERT_ONCE);
    }

    /**
     * Verifies that notifications created with the "silent" flag do not inherit system defaults in
     * regards to their sound, vibration and light indicators.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationSilentProperty() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());

        Notification notification = showAndGetNotification("MyNotification", "{ silent: true }");

        // Zero indicates that no defaults should be inherited from the system.
        Assert.assertEquals(0, notification.defaults);

        // On Android O+ the defaults are ignored as vibrate and silent moved to the notification
        // channel. The silent flag is achieved by using a group alert summary.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Assert.assertEquals(
                    Notification.GROUP_ALERT_SUMMARY, notification.getGroupAlertBehavior());
        }
    }

    private void verifyVibrationNotRequestedWhenDisabledInPrefs(String notificationOptions)
            throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());

        // Disable notification vibration in preferences.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                .setBoolean(NOTIFICATIONS_VIBRATE_ENABLED, false));

        Notification notification = showAndGetNotification("MyNotification", notificationOptions);

        // On Android O+ the defaults are ignored as vibrate and silent moved to the notification
        // channel.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Assert.assertEquals(0, notification.defaults);
        } else {
            // Vibration should not be in the defaults.
            Assert.assertEquals(
                    Notification.DEFAULT_ALL & ~Notification.DEFAULT_VIBRATE,
                    notification.defaults);

            // There should be a custom no-op vibration pattern.
            Assert.assertEquals(1, notification.vibrate.length);
            Assert.assertEquals(0L, notification.vibrate[0]);
        }
    }

    /**
     * Verifies that when notification vibration is disabled in preferences and no custom pattern is
     * specified, no vibration is requested from the framework.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationVibratePreferenceDisabledDefault() throws Exception {
        verifyVibrationNotRequestedWhenDisabledInPrefs("{}");
    }

    /**
     * Verifies that when notification vibration is disabled in preferences and a custom pattern is
     * specified, no vibration is requested from the framework.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationVibratePreferenceDisabledCustomPattern() throws Exception {
        verifyVibrationNotRequestedWhenDisabledInPrefs("{ vibrate: 42 }");
    }

    /**
     * Verifies that by default the notification vibration preference is enabled, and a custom
     * pattern is passed along.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationVibrateCustomPattern() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());

        // By default, vibration is enabled in notifications.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertTrue(
                                UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                        .getBoolean(NOTIFICATIONS_VIBRATE_ENABLED)));

        Notification notification = showAndGetNotification("MyNotification", "{ vibrate: 42 }");

        // On Android O+ the defaults are ignored as vibrate and silent moved to the notification
        // channel.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Assert.assertEquals(0, notification.defaults);
        } else {
            // Vibration should not be in the defaults, a custom pattern was provided.
            Assert.assertEquals(
                    Notification.DEFAULT_ALL & ~Notification.DEFAULT_VIBRATE,
                    notification.defaults);

            // The custom pattern should have been passed along.
            Assert.assertEquals(2, notification.vibrate.length);
            Assert.assertEquals(0L, notification.vibrate[0]);
            Assert.assertEquals(42L, notification.vibrate[1]);
        }
    }

    /**
     * Verifies that on Android M+, notifications which specify a badge will have that icon fetched
     * and included as the small icon in the notification and public version. If the test target is
     * L or below, verifies the small icon (and public small icon on L) is the expected chrome logo.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    @SuppressWarnings("UseNetworkAnnotations")
    public void testShowNotificationWithBadge() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());

        Notification notification =
                showAndGetNotification("MyNotification", "{badge: 'badge.png'}");

        Assert.assertEquals("MyNotification", NotificationTestUtil.getExtraTitle(notification));

        Context context = ApplicationProvider.getApplicationContext();
        Bitmap smallIcon = NotificationTestUtil.getSmallIconFromNotification(context, notification);
        Assert.assertNotNull(smallIcon);

        // Custom badges are only supported on M+.
        // 1. Check the notification badge.
        URL badgeUrl =
                new URL(mPermissionTestRule.getURL("/chrome/test/data/notifications/badge.png"));
        Bitmap bitmap = BitmapFactory.decodeStream(badgeUrl.openStream());
        Bitmap expected = bitmap.copy(bitmap.getConfig(), true);
        NotificationBuilderBase.applyWhiteOverlayToBitmap(expected);
        Assert.assertTrue(expected.sameAs(smallIcon));

        // 2. Check the public notification badge.
        Assert.assertNotNull(notification.publicVersion);
        Bitmap publicSmallIcon =
                NotificationTestUtil.getSmallIconFromNotification(
                        context, notification.publicVersion);
        Assert.assertNotNull(publicSmallIcon);
        Assert.assertEquals(expected.getWidth(), publicSmallIcon.getWidth());
        Assert.assertEquals(expected.getHeight(), publicSmallIcon.getHeight());
        Assert.assertTrue(expected.sameAs(publicSmallIcon));
    }

    /**
     * Verifies that notifications which specify an icon will have that icon fetched, converted into
     * a Bitmap and included as the large icon in the notification.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testShowNotificationWithIcon() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());

        Notification notification = showAndGetNotification("MyNotification", "{icon: 'red.png'}");

        Assert.assertEquals("MyNotification", NotificationTestUtil.getExtraTitle(notification));

        Context context = ApplicationProvider.getApplicationContext();
        Bitmap largeIcon = NotificationTestUtil.getLargeIconFromNotification(context, notification);
        Assert.assertNotNull(largeIcon);
        Assert.assertEquals(Color.RED, largeIcon.getPixel(0, 0));
    }

    /**
     * Verifies that notifications which don't specify an icon will get an automatically generated
     * icon based on their origin. The size of these icons are dependent on the resolution of the
     * device the test is being ran on, so we create it again in order to compare.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testShowNotificationWithoutIcon() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());

        Notification notification = showAndGetNotification("NoIconNotification", "{}");

        Assert.assertEquals("NoIconNotification", NotificationTestUtil.getExtraTitle(notification));

        Context context = ApplicationProvider.getApplicationContext();
        Assert.assertNotNull(
                NotificationTestUtil.getLargeIconFromNotification(context, notification));

        NotificationPlatformBridge notificationBridge =
                NotificationPlatformBridge.getInstanceForTests();
        Assert.assertNotNull(notificationBridge);

        // Create a second rounded icon for the test's origin, and compare its dimensions against
        // those of the icon associated to the notification itself.
        RoundedIconGenerator generator =
                NotificationBuilderBase.createIconGenerator(context.getResources());

        Bitmap generatedIcon =
                generator.generateIconForUrl(new GURL(mPermissionTestRule.getOrigin()));
        Assert.assertNotNull(generatedIcon);
        // Starts from Android O MR1, large icon can be downscaled by Android platform code.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            Assert.assertTrue(
                    generatedIcon.sameAs(
                            NotificationTestUtil.getLargeIconFromNotification(
                                    context, notification)));
        }
    }

    /*
     * Verifies that starting the PendingIntent stored as the notification's content intent will
     * start up the associated Service Worker, where the JavaScript code will close the notification
     * by calling event.notification.close().
     */
    @Test
    @LargeTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationContentIntentClosesNotification() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());
        // +0.5 engagement from navigating to the test page.
        Assert.assertEquals(0.5, getEngagementScoreBlocking(), 0);

        UserActionTester actionTester = new UserActionTester();

        Notification notification = showAndGetNotification("MyNotification", "{}");

        // Sending the PendingIntent resembles activating the notification.
        Assert.assertNotNull(notification.contentIntent);
        notification.contentIntent.send();

        // The Service Worker will close the notification upon receiving the notificationclick
        // event. This will eventually bubble up to a call to cancel() in the NotificationManager.
        // Expect +1 engagement from interacting with the notification.
        mNotificationTestRule.waitForNotificationManagerMutation();
        Assert.assertTrue(mNotificationTestRule.getNotificationEntries().isEmpty());
        Assert.assertEquals(1.5, getEngagementScoreBlocking(), 0);

        // This metric only applies on N+, where we schedule a job to handle the click.
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Notifications.Android.JobStartDelay"));

        // Clicking on a notification should record the right user metrics.
        assertThat(
                actionTester.toString(),
                getNotificationActions(actionTester),
                Matchers.hasItems(
                        "Notifications.Persistent.Shown", "Notifications.Persistent.Clicked"));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Notifications.AppNotificationStatus"));
    }

    /**
     * Verifies that starting the PendingIntent stored as the notification's content intent will
     * start up the associated Service Worker, where the JavaScript code will create a new tab for
     * displaying the notification's event to the user.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationContentIntentCreatesTab() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());

        Assert.assertEquals(
                "Expected the notification test page to be the sole tab in the current model",
                1,
                mNotificationTestRule.getActivity().getCurrentTabModel().getCount());

        Notification notification =
                showAndGetNotification("MyNotification", "{ data: 'ACTION_CREATE_TAB' }");

        // Sending the PendingIntent resembles activating the notification.
        Assert.assertNotNull(notification.contentIntent);
        notification.contentIntent.send();

        // The Service Worker, upon receiving the notificationclick event, will create a new tab
        // after which it closes the notification.
        mNotificationTestRule.waitForNotificationManagerMutation();
        Assert.assertTrue(mNotificationTestRule.getNotificationEntries().isEmpty());

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            "Expected a new tab to be created",
                            mNotificationTestRule.getActivity().getCurrentTabModel().getCount(),
                            Matchers.is(2));
                });
        // This metric only applies on N+, where we schedule a job to handle the click.
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Notifications.Android.JobStartDelay"));
    }

    /**
     * Verifies that activating the PendingIntent associated with the "Unsubscribe" button shows the
     * `provisionally unsubscribed` notification and suspends all existing notifications, and then,
     * clicking "Okay" commits this and revokes the notification permission.
     *
     * <p>One-tap Unsubscribe is supported on Android P and later.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "Notifications"})
    @Features.EnableFeatures(ChromeFeatureList.NOTIFICATION_ONE_TAP_UNSUBSCRIBE)
    @MinAndroidSdkLevel(Build.VERSION_CODES.P)
    public void testNotificationProvisionalUnsubscribeAndCommit() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());
        Assert.assertEquals("\"granted\"", runJavaScript("Notification.permission"));

        Notification notification1 = showAndGetNotification("Notification1", "{}");
        showNotification("Notification2", "{}");
        mNotificationTestRule.waitForNotificationCount(2);

        // Click the "Unsubscribe" button.
        Assert.assertEquals(1, notification1.actions.length);
        PendingIntent unsubscribeIntent = notification1.actions[0].actionIntent;
        Assert.assertNotNull(unsubscribeIntent);
        unsubscribeIntent.send();

        // Wait for the two notifications to be collapsed and the `provisionally unsubscribed`
        // notification to appear.
        mNotificationTestRule.waitForNotificationCount(1);

        // Click the "Okay" button to commit. This is the second button.
        Notification provisionallyUnsubscribedNotification =
                mNotificationTestRule.getNotificationEntries().get(0).getNotification();
        Assert.assertEquals(2, provisionallyUnsubscribedNotification.actions.length);
        PendingIntent commitIntent = provisionallyUnsubscribedNotification.actions[1].actionIntent;
        Assert.assertNotNull(commitIntent);
        commitIntent.send();

        // Wait for the `provisionally unsubscribed` notification to disappear.
        mNotificationTestRule.waitForNotificationCount(0);

        // This should have caused notifications permission to become reset.
        Assert.assertEquals("\"default\"", runJavaScript("Notification.permission"));
        checkThatShowNotificationIsDenied();
    }

    /**
     * Verifies that activating the PendingIntent associated with the "Unsubscribe" button shows the
     * `provisionally unsubscribed` notification and suspends all existing notifications, and then,
     * clicking "Undo" reverts this and does not revoke the notification permission.
     *
     * <p>This also verifies that the icon image, which is stored and then loaded from the native
     * `NotificationDatabase`, properly survives this journey.
     *
     * <p>One-tap Unsubscribe is supported on Android P and later.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "Notifications"})
    @Features.EnableFeatures(ChromeFeatureList.NOTIFICATION_ONE_TAP_UNSUBSCRIBE)
    @MinAndroidSdkLevel(Build.VERSION_CODES.P)
    public void testNotificationProvisionalUnsubscribeAndUndo() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());
        Assert.assertEquals("\"granted\"", runJavaScript("Notification.permission"));

        Notification notification1 = showAndGetNotification("Notification1", "{icon: 'red.png'}");
        showNotification("Notification2", "{}");
        mNotificationTestRule.waitForNotificationCount(2);

        // Verify that the origin notifications will play sound/vibration.
        var originalSBNotifications = mNotificationTestRule.getNotificationEntries();
        Assert.assertEquals(
                Notification.GROUP_ALERT_ALL,
                originalSBNotifications.get(0).getNotification().getGroupAlertBehavior());
        Assert.assertEquals(
                Notification.GROUP_ALERT_ALL,
                originalSBNotifications.get(1).getNotification().getGroupAlertBehavior());

        // Click the "Unsubscribe" button.
        Assert.assertEquals(1, notification1.actions.length);
        PendingIntent unsubscribeIntent = notification1.actions[0].actionIntent;
        Assert.assertNotNull(unsubscribeIntent);
        unsubscribeIntent.send();

        // Wait for the two notifications to be collapsed and the `provisionally unsubscribed`
        // notification to appear.
        mNotificationTestRule.waitForNotificationCount(1);

        // Click the "Undo" button to revert. This is the first button.
        Notification provisionallyUnsubscribedNotification =
                mNotificationTestRule.getNotificationEntries().get(0).getNotification();
        Assert.assertEquals(2, provisionallyUnsubscribedNotification.actions.length);
        PendingIntent undoIntent = provisionallyUnsubscribedNotification.actions[0].actionIntent;
        Assert.assertNotNull(undoIntent);
        undoIntent.send();

        // Wait for the `provisionally unsubscribed` notification to disappear and the two
        // notifications to be restored.
        mNotificationTestRule.waitForNotificationCount(2);

        // Verify the icon is restored correctly.
        Context context = ApplicationProvider.getApplicationContext();
        var restoredSBNotifications = mNotificationTestRule.getNotificationEntries();
        Bitmap largeIcon =
                NotificationTestUtil.getLargeIconFromNotification(
                        context, restoredSBNotifications.get(0).getNotification());
        Assert.assertNotNull(largeIcon);
        Assert.assertEquals(Color.RED, largeIcon.getPixel(0, 0));

        // Verify that both notifications are silent when they are restored.
        Assert.assertEquals(
                Notification.GROUP_ALERT_SUMMARY,
                restoredSBNotifications.get(0).getNotification().getGroupAlertBehavior());
        Assert.assertEquals(
                Notification.GROUP_ALERT_SUMMARY,
                restoredSBNotifications.get(1).getNotification().getGroupAlertBehavior());

        // This should not have caused notifications permission to become denied.
        Assert.assertEquals("\"granted\"", runJavaScript("Notification.permission"));
        showNotification("Notification3", "{}");
        mNotificationTestRule.waitForNotificationCount(3);
    }

    /**
     * Verifies that activating the PendingIntent associated with the "Unsubscribe" button shows the
     * `provisionally unsubscribed` notification and suspends all existing notifications, even when
     * we are using service-type intents.
     *
     * <p>One-tap Unsubscribe is supported on Android P and later.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "Notifications"})
    @CommandLineFlags.Add({
        "enable-features=" + ChromeFeatureList.NOTIFICATION_ONE_TAP_UNSUBSCRIBE + "<Study",
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:use_service_intent/true"
    })
    @MinAndroidSdkLevel(Build.VERSION_CODES.P)
    public void testNotificationProvisionalUnsubscribeWithServiceIntent() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());
        Assert.assertEquals("\"granted\"", runJavaScript("Notification.permission"));

        Notification notification1 = showAndGetNotification("Notification1", "{}");
        showNotification("Notification2", "{}");
        mNotificationTestRule.waitForNotificationCount(2);

        // Click the "Unsubscribe" button.
        Assert.assertEquals(1, notification1.actions.length);
        PendingIntent unsubscribeIntent = notification1.actions[0].actionIntent;
        Assert.assertNotNull(unsubscribeIntent);
        unsubscribeIntent.send();

        // Wait for the two notifications to be collapsed and the `provisionally unsubscribed`
        // notification to appear.
        mNotificationTestRule.waitForNotificationCount(1);
    }

    /**
     * Verifies that creating a notification with an associated "tag" will cause any previous
     * notification with the same tag to be dismissed prior to being shown.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationTagReplacement() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());
        // +0.5 engagement from navigating to the test page.
        Assert.assertEquals(0.5, getEngagementScoreBlocking(), 0);

        showNotification("First", "{tag: 'myTag'}");
        mNotificationTestRule.waitForNotificationManagerMutation();
        List<NotificationEntry> notifications = mNotificationTestRule.getNotificationEntries();
        String tag = notifications.get(0).tag;
        int id = notifications.get(0).id;

        showNotification("Second", "{tag: 'myTag'}");
        mNotificationTestRule.waitForNotificationManagerMutation();

        // Verify that the notification was successfully replaced.
        notifications = mNotificationTestRule.getNotificationEntries();
        Assert.assertEquals(1, notifications.size());
        Assert.assertEquals(
                "Second", NotificationTestUtil.getExtraTitle(notifications.get(0).notification));

        // Verify that for replaced notifications their tag was the same.
        Assert.assertEquals(tag, notifications.get(0).tag);

        // Verify that as always, the same integer is used, also for replaced notifications.
        Assert.assertEquals(id, notifications.get(0).id);
        Assert.assertEquals(NotificationPlatformBridge.PLATFORM_ID, notifications.get(0).id);

        // Engagement should not have changed since we didn't interact.
        Assert.assertEquals(0.5, getEngagementScoreBlocking(), 0);
    }

    /**
     * Verifies that multiple notifications without a tag can be opened and closed without affecting
     * eachother.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "Notifications"})
    public void testShowAndCloseMultipleNotifications() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());
        // +0.5 engagement from navigating to the test page.
        Assert.assertEquals(0.5, getEngagementScoreBlocking(), 0);

        // Open the first notification and verify it is displayed.
        showNotification("One", "{}");
        mNotificationTestRule.waitForNotificationManagerMutation();
        List<NotificationEntry> notifications = mNotificationTestRule.getNotificationEntries();
        Assert.assertEquals(1, notifications.size());
        Notification notificationOne = notifications.get(0).notification;
        Assert.assertEquals("One", NotificationTestUtil.getExtraTitle(notificationOne));

        // Open the second notification and verify it is displayed.
        showNotification("Two", "{}");
        mNotificationTestRule.waitForNotificationManagerMutation();
        notifications = mNotificationTestRule.getNotificationEntries();
        Assert.assertEquals(2, notifications.size());
        Notification notificationTwo = notifications.get(1).notification;
        Assert.assertEquals("Two", NotificationTestUtil.getExtraTitle(notificationTwo));

        // The same integer id is always used as it is not needed for uniqueness, we rely on the tag
        // for uniqueness when the replacement behavior is not needed.
        Assert.assertEquals(NotificationPlatformBridge.PLATFORM_ID, notifications.get(0).id);
        Assert.assertEquals(NotificationPlatformBridge.PLATFORM_ID, notifications.get(1).id);

        // As these notifications were not meant to replace eachother, they must not have the same
        // tag internally.
        Assert.assertFalse(notifications.get(0).tag.equals(notifications.get(1).tag));

        // Verify that the PendingIntent for content and delete is different for each notification.
        Assert.assertFalse(notificationOne.contentIntent.equals(notificationTwo.contentIntent));
        Assert.assertFalse(notificationOne.deleteIntent.equals(notificationTwo.deleteIntent));

        // Close the first notification and verify that only the second remains.
        // Sending the content intent resembles touching the notification. In response tho this the
        // notificationclick event is fired. The test service worker will close the notification
        // upon receiving the event.
        notificationOne.contentIntent.send();
        mNotificationTestRule.waitForNotificationManagerMutation();
        notifications = mNotificationTestRule.getNotificationEntries();
        Assert.assertEquals(1, notifications.size());
        Assert.assertEquals(
                "Two", NotificationTestUtil.getExtraTitle(notifications.get(0).notification));

        // Expect +1 engagement from interacting with the notification.
        Assert.assertEquals(1.5, getEngagementScoreBlocking(), 0);

        // Close the last notification and verify that none remain.
        notifications.get(0).notification.contentIntent.send();
        mNotificationTestRule.waitForNotificationManagerMutation();
        Assert.assertTrue(mNotificationTestRule.getNotificationEntries().isEmpty());

        // Expect +1 engagement from interacting with the notification.
        Assert.assertEquals(2.5, getEngagementScoreBlocking(), 0);
    }

    /**
     * The next two tests verify that the PendingIntent associated with the "Unsubscribe" button is
     * either a broadcast or service type intent based on field trial configuration.
     *
     * <p>One-tap Unsubscribe is supported on Android P and later, but these tests rely on
     * `isBroadcast` and `isService` that was added in API level 31.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    @CommandLineFlags.Add({
        "enable-features=" + ChromeFeatureList.NOTIFICATION_ONE_TAP_UNSUBSCRIBE + "<Study",
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:use_service_intent/false"
    })
    @MinAndroidSdkLevel(Build.VERSION_CODES.S)
    @RequiresApi(Build.VERSION_CODES.S)
    public void testNotificationProvisionalUnsubscribeIsBroadcast() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());

        Notification notification = showAndGetNotification("Notification1", "{}");

        // Verify the "Unsubscribe" button's intent.
        Assert.assertEquals(1, notification.actions.length);
        PendingIntent unsubscribeIntent = notification.actions[0].actionIntent;
        Assert.assertNotNull(unsubscribeIntent);
        Assert.assertTrue(unsubscribeIntent.isBroadcast());
    }

    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    @CommandLineFlags.Add({
        "enable-features=" + ChromeFeatureList.NOTIFICATION_ONE_TAP_UNSUBSCRIBE + "<Study",
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:use_service_intent/true"
    })
    @MinAndroidSdkLevel(Build.VERSION_CODES.S)
    @RequiresApi(Build.VERSION_CODES.S)
    public void testNotificationProvisionalUnsubscribeIsService() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mPermissionTestRule.getOrigin());

        Notification notification = showAndGetNotification("Notification1", "{}");

        // Verify the "Unsubscribe" button's intent.
        Assert.assertEquals(1, notification.actions.length);
        PendingIntent unsubscribeIntent = notification.actions[0].actionIntent;
        Assert.assertNotNull(unsubscribeIntent);
        Assert.assertTrue(unsubscribeIntent.isService());
    }

    /**
     * Shows a notification with |title| and |options|, waits until it has been displayed and then
     * returns the Notification object to the caller. Requires that only a single notification is
     * being displayed in the notification manager.
     *
     * @param title Title of the Web Notification to show.
     * @param options Optional map of options to include when showing the notification.
     * @return The Android Notification object, as shown in the framework.
     */
    private Notification showAndGetNotification(String title, String options)
            throws TimeoutException {
        showNotification(title, options);
        return mNotificationTestRule.waitForNotification().notification;
    }

    private void showNotification(String title, String options) throws TimeoutException {
        runJavaScript(
                "GetActivatedServiceWorkerForTest()"
                        + ".then(reg => reg.showNotification('"
                        + title
                        + "', "
                        + options
                        + "))"
                        + ".catch(sendToTest)");
    }

    private String runJavaScript(String code) throws TimeoutException {
        return mNotificationTestRule.runJavaScriptCodeInCurrentTab(code);
    }

    /** Get Notification related actions, filter all other actions to avoid flakes. */
    private List<String> getNotificationActions(UserActionTester actionTester) {
        List<String> actions = new ArrayList<>(actionTester.getActions());
        Iterator<String> it = actions.iterator();
        while (it.hasNext()) {
            if (!it.next().startsWith("Notifications.")) {
                it.remove();
            }
        }
        return actions;
    }
}
