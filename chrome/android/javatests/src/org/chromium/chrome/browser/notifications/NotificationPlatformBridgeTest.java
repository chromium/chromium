// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.chromium.chrome.browser.notifications.NotificationContentDetectionManager.SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME;
import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;
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

import org.chromium.base.FeatureOverrides;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.notifications.NotificationContentDetectionManager.SuspiciousNotificationWarningInteractions;
import org.chromium.chrome.browser.permissions.PermissionTestRule;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy.NotificationEntry;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.content_settings.ContentSetting;
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
@Features.DisableFeatures(ChromeFeatureList.REPORT_NOTIFICATION_CONTENT_DETECTION_DATA)
public class NotificationPlatformBridgeTest {
    @Rule public PermissionTestRule mPermissionTestRule = new PermissionTestRule();

    @Rule public NotificationTestRule mNotificationTestRule = new NotificationTestRule();

    private static final String NOTIFICATION_TEST_PAGE =
            "/chrome/test/data/notifications/android_test.html";
    private static final int TITLE_UPDATE_TIMEOUT_SECONDS = (int) 5L;

    private static final String SUSPICIOUS_NOTIFICATION_COUNT_SHOW_ORIGINALS_HISTOGRAM_NAME =
            "SafeBrowsing.SuspiciousNotificationWarning."
                    + "ShowOriginalNotifications.SuspiciousNotificationCount";
    private static final String
            SUSPICIOUS_NOTIFICATION_COUNT_DROPPED_SHOW_ORIGINALS_HISTOGRAM_NAME =
                    "SafeBrowsing.SuspiciousNotificationWarning."
                            + "ShowOriginalNotifications.SuspiciousNotificationsDroppedCount";

    private static final String SAFE_BROWSING_NOTIFICATION_REVOCATION_SOURCE_HISTOGRAM_NAME =
            "SafeBrowsing.NotificationRevocationSource";
    // These represent the values logged in the
    // `SAFE_BROWSING_NOTIFICATION_REVOCATION_SOURCE_HISTOGRAM_NAME` histogram. The values are
    // defined in the safe_browsing::NotificationRevocationSource enum class.
    // Enum value corresponding to a revocation happening when a user unsubscribes on a notification
    // where a suspicious content warning was NOT shown.
    private static final int STANDARD_ONE_TAP_UNSUBSCRIBE_EVENT = 2;
    // Enum value corresponding to a revocation happening when a user unsubscribes on a notification
    // where a suspicious content warning was shown.
    private static final int SUSPICIOUS_WARNING_ONE_TAP_UNSUBSCRIBE_EVENT = 3;

    @Before
    public void setUp() {
        SiteEngagementService.setParamValuesForTesting();
        mNotificationTestRule.loadUrl(mPermissionTestRule.getURL(NOTIFICATION_TEST_PAGE));
        mPermissionTestRule.setActivity(mNotificationTestRule.getActivity());
    }

    @SuppressWarnings("MissingFail")
    private void waitForTitle(String expectedTitle) {
        Tab tab = mNotificationTestRule.getActivityTab();
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
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());
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
        Assert.assertEquals(0, notification.defaults);
        Assert.assertEquals(Notification.GROUP_ALERT_ALL, notification.getGroupAlertBehavior());
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
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());

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
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());
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
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());
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
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());

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
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());

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
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());

        Notification notification = showAndGetNotification("MyNotification", "{ silent: true }");

        // Zero indicates that no defaults should be inherited from the system.
        Assert.assertEquals(0, notification.defaults);

        // On Android O+ the defaults are ignored as vibrate and silent moved to the notification
        // channel. The silent flag is achieved by using a group alert summary.
        Assert.assertEquals(Notification.GROUP_ALERT_SUMMARY, notification.getGroupAlertBehavior());
    }

    private void verifyVibrationNotRequestedWhenDisabledInPrefs(String notificationOptions)
            throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());

        // Disable notification vibration in preferences.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                .setBoolean(NOTIFICATIONS_VIBRATE_ENABLED, false));

        Notification notification = showAndGetNotification("MyNotification", notificationOptions);

        // On Android O+ the defaults are ignored as vibrate and silent moved to the notification
        // channel.
        Assert.assertEquals(0, notification.defaults);
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
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());

        // By default, vibration is enabled in notifications.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertTrue(
                                UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                        .getBoolean(NOTIFICATIONS_VIBRATE_ENABLED)));

        Notification notification = showAndGetNotification("MyNotification", "{ vibrate: 42 }");

        // On Android O+ the defaults are ignored as vibrate and silent moved to the notification
        // channel.
        Assert.assertEquals(0, notification.defaults);
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
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());

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
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());

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
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());

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
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());
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
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());

        Assert.assertEquals(
                "Expected the notification test page to be the sole tab in the current model",
                1,
                getTabCountOnUiThread(mNotificationTestRule.getActivity().getCurrentTabModel()));

        Notification notification =
                showAndGetNotification("MyNotification", "{ data: 'ACTION_CREATE_TAB' }");

        // Sending the PendingIntent resembles activating the notification.
        Assert.assertNotNull(notification.contentIntent);
        notification.contentIntent.send();

        // The Service Worker, upon receiving the notificationclick event, will create a new tab
        // after which it closes the notification.
        mNotificationTestRule.waitForNotificationManagerMutation();
        Assert.assertTrue(mNotificationTestRule.getNotificationEntries().isEmpty());

        CriteriaHelper.pollUiThread(
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
    public void testNotificationProvisionalUnsubscribeAndCommit() throws Exception {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SAFE_BROWSING_NOTIFICATION_REVOCATION_SOURCE_HISTOGRAM_NAME,
                                STANDARD_ONE_TAP_UNSUBSCRIBE_EVENT)
                        .build();

        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());
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

        // Validate histogram is logged correctly.
        histogramWatcher.assertExpected();
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
    public void testNotificationProvisionalUnsubscribeAndUndo() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());
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
     * Verifies that creating a notification with an associated "tag" will cause any previous
     * notification with the same tag to be dismissed prior to being shown.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    public void testNotificationTagReplacement() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());
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
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());
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
     * The next test verify that the PendingIntent associated with the "Unsubscribe" button is
     * a broadcast type intent based on field trial configuration.
     *
     * <p>One-tap Unsubscribe is supported on Android P and later, but the tests rely on
     * `isBroadcast` that was added in API level 31.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "Notifications"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.S)
    @RequiresApi(Build.VERSION_CODES.S)
    public void testNotificationProvisionalUnsubscribeIsBroadcast() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());

        Notification notification = showAndGetNotification("Notification1", "{}");

        // Verify the "Unsubscribe" button's intent.
        Assert.assertEquals(1, notification.actions.length);
        PendingIntent unsubscribeIntent = notification.actions[0].actionIntent;
        Assert.assertNotNull(unsubscribeIntent);
        Assert.assertTrue(unsubscribeIntent.isBroadcast());
    }

    /**
     * Verifies that when `SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS` is enabled, suspicious
     * notifications are replaced by warning notifications. Then dismiss one notification and
     * tapping the "Unsubscribe" button on another notification to perform both expected behaviors.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "Notifications"})
    @Features.EnableFeatures({
        ChromeFeatureList.SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS
    })
    public void testShowWarningNotificationsThenDismissAndUnsubscribe() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());
        Assert.assertEquals("\"granted\"", runJavaScript("Notification.permission"));

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME,
                                SuspiciousNotificationWarningInteractions.WARNING_SHOWN,
                                2)
                        .expectIntRecords(
                                SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME,
                                SuspiciousNotificationWarningInteractions
                                        .SHOW_ORIGINAL_NOTIFICATION,
                                SuspiciousNotificationWarningInteractions.DISMISS,
                                SuspiciousNotificationWarningInteractions
                                        .SUPPRESS_DUPLICATE_WARNING)
                        .expectIntRecord(
                                SUSPICIOUS_NOTIFICATION_COUNT_SHOW_ORIGINALS_HISTOGRAM_NAME, 1)
                        .expectIntRecord(
                                SUSPICIOUS_NOTIFICATION_COUNT_DROPPED_SHOW_ORIGINALS_HISTOGRAM_NAME,
                                0)
                        .build();

        NotificationPlatformBridge notificationBridge =
                NotificationPlatformBridge.getInstanceForTests();
        Assert.assertNotNull(notificationBridge);
        notificationBridge.setIsSuspiciousParameterForTesting(true);

        // Display a suspicious notification and show the original.
        String expectedOrigin =
                UrlFormatter.formatUrlForSecurityDisplay(
                        mPermissionTestRule.getOrigin(), SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        Notification warningNotification = showAndGetNotification("MyNotification1", "{}");
        Assert.assertEquals(
                "Possible spam", NotificationTestUtil.getExtraTitle(warningNotification));
        Assert.assertTrue(
                NotificationTestUtil.getExtraText(warningNotification)
                        .contains("Chrome detected possible spam from " + expectedOrigin));
        Assert.assertEquals(
                expectedOrigin, NotificationTestUtil.getExtraSubText(warningNotification));
        Assert.assertEquals(2, warningNotification.actions.length);
        PendingIntent showNotificationIntent = warningNotification.actions[1].actionIntent;
        Assert.assertNotNull(showNotificationIntent);
        showNotificationIntent.send();
        mNotificationTestRule.waitForNotificationManagerMutation();
        Notification restoredNotificationFromWarning =
                mNotificationTestRule.getNotificationEntries().get(0).getNotification();
        Assert.assertEquals(
                Notification.GROUP_ALERT_SUMMARY,
                restoredNotificationFromWarning.getGroupAlertBehavior());
        Assert.assertEquals(
                "MyNotification1",
                NotificationTestUtil.getExtraTitle(restoredNotificationFromWarning));

        // Display another 2 suspicious notifications that will be replaced by a single warning.
        showNotification("MyNotification2", "{}");
        mNotificationTestRule.waitForNotificationCount(2);
        showNotification("MyNotification3", "{}");
        mNotificationTestRule.waitForNotificationManagerMutation();
        Notification newWarningNotification =
                mNotificationTestRule.getNotificationEntries().get(1).getNotification();
        Assert.assertEquals(
                "Possible spam (2)", NotificationTestUtil.getExtraTitle(newWarningNotification));
        Assert.assertTrue(
                NotificationTestUtil.getExtraText(newWarningNotification)
                        .contains("Chrome detected possible spam from " + expectedOrigin));
        Assert.assertEquals(
                expectedOrigin, NotificationTestUtil.getExtraSubText(newWarningNotification));
        Assert.assertEquals(2, newWarningNotification.actions.length);

        // Display 1 non-suspicious notification.
        notificationBridge.setIsSuspiciousParameterForTesting(false);
        showNotification("MyNotification4", "{}");
        mNotificationTestRule.waitForNotificationCount(3);
        Notification nonSuspiciousNotification =
                mNotificationTestRule.getNotificationEntries().get(2).getNotification();
        Assert.assertEquals(
                "MyNotification4", NotificationTestUtil.getExtraTitle(nonSuspiciousNotification));

        // Verify that the suspicious notification interactions will be logged.
        Assert.assertEquals(
                3,
                NotificationContentDetectionManager.sSuspiciousNotificationsMap
                        .get(mPermissionTestRule.getOrigin())
                        .size());

        // Dismiss the warning notification showing 2 spam notifications.
        newWarningNotification.deleteIntent.send();
        mNotificationTestRule.waitForNotificationManagerMutation();

        // Tap the "Unsubscribe" button on the original spam notification.
        PendingIntent unsubscribeIntent = restoredNotificationFromWarning.actions[0].actionIntent;
        unsubscribeIntent.send();

        // Wait for the provisionally unsubscribe notification to appear.
        mNotificationTestRule.waitForNotificationCount(1);
        Notification provisionallyUnsubscribedNotification =
                mNotificationTestRule.getNotificationEntries().get(0).getNotification();
        Assert.assertEquals(2, provisionallyUnsubscribedNotification.actions.length);
        Assert.assertEquals(
                "Unsubscribed from notifications",
                NotificationTestUtil.getExtraTitle(provisionallyUnsubscribedNotification));
        Assert.assertTrue(
                NotificationTestUtil.getExtraText(provisionallyUnsubscribedNotification)
                        .contains("You'll no longer receive notifications from " + expectedOrigin));
        Assert.assertEquals(
                expectedOrigin, NotificationTestUtil.getExtraSubText(newWarningNotification));

        // Tap the "Okay" button to commit. This is the second button.
        PendingIntent commitIntent = provisionallyUnsubscribedNotification.actions[1].actionIntent;
        Assert.assertNotNull(commitIntent);
        commitIntent.send();

        // Wait for the `provisionally unsubscribed` notification to disappear.
        mNotificationTestRule.waitForNotificationCount(0);

        // Validate histogram is logged correctly.
        histogramWatcher.assertExpected();

        // Verify interactions will no longer be logged.
        Assert.assertTrue(
                NotificationContentDetectionManager.sSuspiciousNotificationsMap.isEmpty());
    }

    /**
     * Verifies that when `SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS` is enabled, suspicious
     * notifications are replaced by a warning and tapping the "Show Notification" button performs
     * the show notification behaviour. Then, tapping the "Always allow" button displays the
     * original backups of the warning notifications from the same origin.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "Notifications"})
    @Features.EnableFeatures({
        ChromeFeatureList.SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS
    })
    public void testNotificationShowWarningNotificationThenShowNotificationThenAlwaysAllow()
            throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());
        Assert.assertEquals("\"granted\"", runJavaScript("Notification.permission"));

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME,
                                SuspiciousNotificationWarningInteractions.WARNING_SHOWN,
                                2)
                        .expectIntRecordTimes(
                                SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME,
                                SuspiciousNotificationWarningInteractions
                                        .SHOW_ORIGINAL_NOTIFICATION,
                                2)
                        .expectIntRecords(
                                SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME,
                                SuspiciousNotificationWarningInteractions.ALWAYS_ALLOW,
                                SuspiciousNotificationWarningInteractions
                                        .SUPPRESS_DUPLICATE_WARNING)
                        // The first "Show notification(s)" tap is on a warning with 1 suspicious
                        // notification.
                        .expectIntRecord(
                                SUSPICIOUS_NOTIFICATION_COUNT_SHOW_ORIGINALS_HISTOGRAM_NAME, 1)
                        // The second "Show notification(s)" tap is on a warning with 2 suspicious
                        // notifications.
                        .expectIntRecord(
                                SUSPICIOUS_NOTIFICATION_COUNT_SHOW_ORIGINALS_HISTOGRAM_NAME, 2)
                        // There should be not be any dropped suspicious notifications for either of
                        // the "Show notification(s)" actions.
                        .expectIntRecordTimes(
                                SUSPICIOUS_NOTIFICATION_COUNT_DROPPED_SHOW_ORIGINALS_HISTOGRAM_NAME,
                                0,
                                2)
                        .build();

        NotificationPlatformBridge notificationBridge =
                NotificationPlatformBridge.getInstanceForTests();
        Assert.assertNotNull(notificationBridge);
        notificationBridge.setIsSuspiciousParameterForTesting(true);

        // Display 1 suspicious notification.
        showAndGetNotification("MyNotification1", "{body: 'Hello'}");
        mNotificationTestRule.waitForNotificationCount(1);

        // Click the "Show Notification" button.
        List<NotificationEntry> notifications = mNotificationTestRule.getNotificationEntries();
        Notification warningNotification = notifications.get(0).getNotification();
        Assert.assertEquals(2, warningNotification.actions.length);
        PendingIntent showNotificationIntent = warningNotification.actions[1].actionIntent;
        Assert.assertNotNull(showNotificationIntent);
        showNotificationIntent.send();

        // Check the original notification is restored silently.
        Notification restoredNotificationFromWarning =
                mNotificationTestRule.waitForNotification().notification;
        Assert.assertEquals(
                Notification.GROUP_ALERT_SUMMARY,
                restoredNotificationFromWarning.getGroupAlertBehavior());
        Assert.assertEquals(
                "MyNotification1",
                NotificationTestUtil.getExtraTitle(restoredNotificationFromWarning));
        Assert.assertEquals(
                "Hello", NotificationTestUtil.getExtraText(restoredNotificationFromWarning));

        // Display 2 notifications that will be replaced by a single warning.
        showNotification("MyNotification2", "{}");
        mNotificationTestRule.waitForNotificationCount(2);
        showNotification("MyNotification3", "{}");
        mNotificationTestRule.waitForNotificationManagerMutation();
        notifications = mNotificationTestRule.getNotificationEntries();
        Notification notification2 = notifications.get(1).getNotification();
        Assert.assertEquals("Possible spam (2)", NotificationTestUtil.getExtraTitle(notification2));
        String expectedOrigin =
                UrlFormatter.formatUrlForSecurityDisplay(
                        mPermissionTestRule.getOrigin(), SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        Assert.assertTrue(
                NotificationTestUtil.getExtraText(notification2)
                        .contains("Chrome detected possible spam from " + expectedOrigin));
        Assert.assertEquals(expectedOrigin, NotificationTestUtil.getExtraSubText(notification2));
        Assert.assertEquals(2, notification2.actions.length);

        // Tap the "Show notification" button on the 2nd notification.
        PendingIntent showNotificationIntent2 = notification2.actions[1].actionIntent;
        Assert.assertNotNull(showNotificationIntent2);
        showNotificationIntent2.send();
        mNotificationTestRule.waitForNotificationCount(3);

        // Set to false so the "Always allow" confirmation notification will not be marked as
        // suspicious.
        notificationBridge.setIsSuspiciousParameterForTesting(false);

        // Tap the "Always allow" button on the restored original notification.
        Assert.assertEquals(2, restoredNotificationFromWarning.actions.length);
        PendingIntent alwaysAllowIntent = restoredNotificationFromWarning.actions[1].actionIntent;
        Assert.assertNotNull(alwaysAllowIntent);
        alwaysAllowIntent.send();
        mNotificationTestRule.waitForNotificationCount(4);

        // Verify 3 notifications have their original titles and only have one button
        // (Unsubscribe).
        notifications = mNotificationTestRule.getNotificationEntries();
        for (int i = 0; i < 3; i++) {
            Notification restoredNotification = notifications.get(i).getNotification();
            Assert.assertEquals(
                    "MyNotification" + (i + 1),
                    NotificationTestUtil.getExtraTitle(restoredNotification));
            Assert.assertEquals(1, restoredNotification.actions.length);
        }

        // Verify the confirmation notification.
        Notification confirmationNotification = notifications.get(3).getNotification();
        Assert.assertEquals(
                "Always allow", NotificationTestUtil.getExtraTitle(confirmationNotification));
        Assert.assertEquals(
                "Chrome will stop flagging notifications from this site as spam",
                NotificationTestUtil.getExtraText(confirmationNotification));
        Assert.assertNull(confirmationNotification.actions);

        // Validate histogram is logged.
        histogramWatcher.assertExpected();

        // Verify interactions will no longer be logged.
        Assert.assertTrue(
                NotificationContentDetectionManager.sSuspiciousNotificationsMap.isEmpty());
    }

    /**
     * Verifies that when `SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS` is enabled, non-suspicious
     * notifications behave the same and are not replaced by warnings.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "Notifications"})
    @Features.EnableFeatures({
        ChromeFeatureList.SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS
    })
    public void testShowWarningFeatureDoesNotWarnForUnsuspiciousNotification() throws Exception {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());
        Assert.assertEquals("\"granted\"", runJavaScript("Notification.permission"));
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME)
                        .build();

        NotificationPlatformBridge notificationBridge =
                NotificationPlatformBridge.getInstanceForTests();
        Assert.assertNotNull(notificationBridge);
        notificationBridge.setIsSuspiciousParameterForTesting(false);

        showNotification("Notification0", "{body: 'Hello'}");
        showNotification("Notification1", "{body: 'Hello'}");
        showNotification("Notification2", "{body: 'Hello'}");
        mNotificationTestRule.waitForNotificationCount(3);

        // Check notification contents were not replaced by a warning.
        List<NotificationEntry> notifications = mNotificationTestRule.getNotificationEntries();
        Notification notification0 = notifications.get(0).getNotification();
        String expectedOrigin =
                UrlFormatter.formatUrlForSecurityDisplay(
                        mPermissionTestRule.getOrigin(), SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        // Validate the contents of the notification.
        Assert.assertEquals("Notification0", NotificationTestUtil.getExtraTitle(notification0));
        Assert.assertEquals("Hello", NotificationTestUtil.getExtraText(notification0));
        Assert.assertEquals(expectedOrigin, NotificationTestUtil.getExtraSubText(notification0));

        // Dismiss notification1.
        Notification notification1 = notifications.get(1).getNotification();
        notification1.deleteIntent.send();
        mNotificationTestRule.waitForNotificationManagerMutation();

        // Click the "Unsubscribe" button on warning notification0.
        PendingIntent unsubscribeIntent = notification0.actions[0].actionIntent;
        unsubscribeIntent.send();

        // Wait for the provisionally unsubscribe notification to appear and click the "Okay" button
        // to commit.
        mNotificationTestRule.waitForNotificationCount(1);
        Notification provisionallyUnsubscribedNotification =
                mNotificationTestRule.getNotificationEntries().get(0).getNotification();
        PendingIntent commitIntent = provisionallyUnsubscribedNotification.actions[1].actionIntent;
        Assert.assertNotNull(commitIntent);
        commitIntent.send();

        // Wait for the `provisionally unsubscribed` notification to disappear.
        mNotificationTestRule.waitForNotificationCount(0);

        // Validate nothing is logged.
        Assert.assertTrue(
                NotificationContentDetectionManager.sSuspiciousNotificationsMap.isEmpty());
        histogramWatcher.assertExpected();
    }

    /**
     * Verifies that when `SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS` is enabled and the
     * ShowWarningsForSuspiciousNotificationsShouldSwapButtons parameter is true, the buttons are
     * switched.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "Notifications"})
    @Features.EnableFeatures({
        ChromeFeatureList.SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS
    })
    public void testShowWarningFeatureSwitchButtons() throws Exception {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS,
                NotificationContentDetectionManager
                        .SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS_SHOULD_SWAP_BUTTONS_PARAM_NAME,
                true);
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());
        Assert.assertEquals("\"granted\"", runJavaScript("Notification.permission"));

        NotificationPlatformBridge notificationBridge =
                NotificationPlatformBridge.getInstanceForTests();
        Assert.assertNotNull(notificationBridge);
        notificationBridge.setIsSuspiciousParameterForTesting(true);

        // Display 1 notification.
        showAndGetNotification("MyNotification1", "{body: 'Hello'}");
        mNotificationTestRule.waitForNotificationCount(1);

        // Click the "Show Notification" button, which is now the primary button.
        List<NotificationEntry> notifications = mNotificationTestRule.getNotificationEntries();
        Notification warningNotification = notifications.get(0).getNotification();
        Assert.assertEquals(2, warningNotification.actions.length);
        PendingIntent showNotificationIntent = warningNotification.actions[0].actionIntent;
        Assert.assertNotNull(showNotificationIntent);
        showNotificationIntent.send();

        // Check the original notification is restored silently.
        Notification restoredNotificationFromWarning =
                mNotificationTestRule.waitForNotification().notification;
        Assert.assertEquals(
                Notification.GROUP_ALERT_SUMMARY,
                restoredNotificationFromWarning.getGroupAlertBehavior());

        // Set to false so the "Always allow" confirmation notification will not be marked as
        // suspicious. Then click "Always allow".
        notificationBridge.setIsSuspiciousParameterForTesting(false);
        Assert.assertEquals(2, restoredNotificationFromWarning.actions.length);
        PendingIntent alwaysAllowIntent = restoredNotificationFromWarning.actions[0].actionIntent;
        Assert.assertNotNull(alwaysAllowIntent);
        alwaysAllowIntent.send();
        mNotificationTestRule.waitForNotificationCount(2);

        // Verify notification has the original title and only has one button(Unsubscribe).
        notifications = mNotificationTestRule.getNotificationEntries();
        Notification restoredNotification = notifications.get(0).getNotification();
        Assert.assertEquals(
                "MyNotification1", NotificationTestUtil.getExtraTitle(restoredNotification));
        Assert.assertEquals(1, restoredNotification.actions.length);

        // Verify the confirmation notification.
        Notification confirmationNotification = notifications.get(1).getNotification();
        Assert.assertEquals(
                "Always allow", NotificationTestUtil.getExtraTitle(confirmationNotification));
        Assert.assertEquals(
                "Chrome will stop flagging notifications from this site as spam",
                NotificationTestUtil.getExtraText(confirmationNotification));
        Assert.assertNull(confirmationNotification.actions);
    }

    /**
     * Verifies that when `SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS` and
     * `REPORT_NOTIFICATION_CONTENT_DETECTION_DATA` are enabled, tapping "Always allow" then "Report
     * as safe" shows a confirmation notification.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "Notifications"})
    @Features.EnableFeatures({
        ChromeFeatureList.REPORT_NOTIFICATION_CONTENT_DETECTION_DATA,
        ChromeFeatureList.SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS
    })
    public void testReportAsSafe() throws Exception {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME,
                                SuspiciousNotificationWarningInteractions.WARNING_SHOWN,
                                SuspiciousNotificationWarningInteractions
                                        .SHOW_ORIGINAL_NOTIFICATION,
                                SuspiciousNotificationWarningInteractions.ALWAYS_ALLOW,
                                SuspiciousNotificationWarningInteractions.REPORT_AS_SAFE)
                        .expectIntRecord(
                                SUSPICIOUS_NOTIFICATION_COUNT_SHOW_ORIGINALS_HISTOGRAM_NAME, 1)
                        .expectIntRecord(
                                SUSPICIOUS_NOTIFICATION_COUNT_DROPPED_SHOW_ORIGINALS_HISTOGRAM_NAME,
                                0)
                        .build();

        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());
        Assert.assertEquals("\"granted\"", runJavaScript("Notification.permission"));

        NotificationPlatformBridge notificationBridge =
                NotificationPlatformBridge.getInstanceForTests();
        Assert.assertNotNull(notificationBridge);
        notificationBridge.setIsSuspiciousParameterForTesting(true);

        // Display 1 notification.
        showAndGetNotification("MyNotification1", "{body: 'Hello'}");
        mNotificationTestRule.waitForNotificationCount(1);

        // Tap the "Show Notification" button.
        List<NotificationEntry> notifications = mNotificationTestRule.getNotificationEntries();
        Notification warningNotification = notifications.get(0).getNotification();
        PendingIntent showNotificationIntent = warningNotification.actions[1].actionIntent;
        Assert.assertNotNull(showNotificationIntent);
        showNotificationIntent.send();

        // Tap "Always allow" on the original notification.
        Notification restoredNotificationFromWarning =
                mNotificationTestRule.waitForNotification().notification;
        // Set to false so the "Always allow" confirmation notification will not be marked as
        // suspicious.
        notificationBridge.setIsSuspiciousParameterForTesting(false);
        PendingIntent alwaysAllowIntent = restoredNotificationFromWarning.actions[1].actionIntent;
        Assert.assertNotNull(alwaysAllowIntent);
        alwaysAllowIntent.send();
        mNotificationTestRule.waitForNotificationCount(2);

        // Tap the "Report" button on the confirmation notification.
        notifications = mNotificationTestRule.getNotificationEntries();
        Notification alwaysAllowConfirmationNotification = notifications.get(1).getNotification();
        String expectedOrigin =
                UrlFormatter.formatUrlForSecurityDisplay(
                        mPermissionTestRule.getOrigin(), SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        Assert.assertEquals(
                "Notifications allowed from " + expectedOrigin,
                NotificationTestUtil.getExtraTitle(alwaysAllowConfirmationNotification));
        Assert.assertEquals(
                "Help improve spam protection by sharing notification content and the site's URL"
                        + " with Google. Content may be reviewed by humans.",
                NotificationTestUtil.getExtraText(alwaysAllowConfirmationNotification));
        Assert.assertEquals(1, alwaysAllowConfirmationNotification.actions.length);
        PendingIntent reportIntent = alwaysAllowConfirmationNotification.actions[0].actionIntent;
        Assert.assertNotNull(reportIntent);
        reportIntent.send();

        // Notification with "report" button should have been dismissed.
        mNotificationTestRule.waitForNotificationCount(1);
        Assert.assertEquals(1, mNotificationTestRule.getNotificationEntries().size());

        // Validate histogram is logged correctly.
        histogramWatcher.assertExpected();
    }

    /**
     * Verifies that when `SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS` and
     * `REPORT_NOTIFICATION_CONTENT_DETECTION_DATA` are enabled and a notification warning is shown,
     * unsubscribing allows the user to "Report as spam".
     */
    @Test
    @LargeTest
    @Feature({"Browser", "Notifications"})
    @Features.EnableFeatures({
        ChromeFeatureList.REPORT_NOTIFICATION_CONTENT_DETECTION_DATA,
        ChromeFeatureList.SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS
    })
    public void testReportWarnedNotificationAsSpam() throws Exception {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME,
                                SuspiciousNotificationWarningInteractions.WARNING_SHOWN,
                                SuspiciousNotificationWarningInteractions
                                        .SHOW_ORIGINAL_NOTIFICATION,
                                SuspiciousNotificationWarningInteractions.UNSUBSCRIBE,
                                SuspiciousNotificationWarningInteractions
                                        .REPORT_WARNED_NOTIFICATION_AS_SPAM)
                        .expectIntRecord(
                                SUSPICIOUS_NOTIFICATION_COUNT_SHOW_ORIGINALS_HISTOGRAM_NAME, 1)
                        .expectIntRecord(
                                SUSPICIOUS_NOTIFICATION_COUNT_DROPPED_SHOW_ORIGINALS_HISTOGRAM_NAME,
                                0)
                        .expectIntRecord(
                                SAFE_BROWSING_NOTIFICATION_REVOCATION_SOURCE_HISTOGRAM_NAME,
                                SUSPICIOUS_WARNING_ONE_TAP_UNSUBSCRIBE_EVENT)
                        .build();

        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());
        Assert.assertEquals("\"granted\"", runJavaScript("Notification.permission"));

        NotificationPlatformBridge notificationBridge =
                NotificationPlatformBridge.getInstanceForTests();
        Assert.assertNotNull(notificationBridge);
        notificationBridge.setIsSuspiciousParameterForTesting(true);

        // Display 1 notification.
        showAndGetNotification("MyNotification1", "{body: 'Hello'}");
        mNotificationTestRule.waitForNotificationCount(1);

        // Tap the "Show Notification" button.
        List<NotificationEntry> notifications = mNotificationTestRule.getNotificationEntries();
        Notification warningNotification = notifications.get(0).getNotification();
        PendingIntent showNotificationIntent = warningNotification.actions[1].actionIntent;
        Assert.assertNotNull(showNotificationIntent);
        showNotificationIntent.send();

        // Before unsubscribing, check that the suspicious notification map contains 1 entry for the
        // origin.
        Assert.assertEquals(
                1,
                NotificationContentDetectionManager.sSuspiciousNotificationsMap
                        .get(mPermissionTestRule.getOrigin())
                        .size());

        // Tap "Unsubscribe" on the original notification.
        Notification restoredNotificationFromWarning =
                mNotificationTestRule.waitForNotification().notification;
        // Set to false so the "Unsubscribe" confirmation notification will not be marked as
        // suspicious.
        notificationBridge.setIsSuspiciousParameterForTesting(false);
        PendingIntent unsubscribeIntent = restoredNotificationFromWarning.actions[0].actionIntent;
        Assert.assertNotNull(unsubscribeIntent);
        unsubscribeIntent.send();

        // Tap the "Report" button on the unsubscribe confirmation notification.
        Notification unsubscribeConfirmationNotification =
                mNotificationTestRule.waitForNotification().notification;
        Assert.assertEquals(
                "Unsubscribed",
                NotificationTestUtil.getExtraTitle(unsubscribeConfirmationNotification));
        Assert.assertEquals(
                "Help improve spam protection by sharing notification content and the site's URL"
                        + " with Google. Content may be reviewed by humans.",
                NotificationTestUtil.getExtraText(unsubscribeConfirmationNotification));
        Assert.assertEquals(2, unsubscribeConfirmationNotification.actions.length);
        PendingIntent reportIntent = unsubscribeConfirmationNotification.actions[1].actionIntent;
        Assert.assertNotNull(reportIntent);
        reportIntent.send();

        // Notification with "report" button should have been dismissed.
        mNotificationTestRule.waitForNotificationCount(0);

        // This should have caused notifications permission to become reset.
        Assert.assertEquals("\"default\"", runJavaScript("Notification.permission"));
        checkThatShowNotificationIsDenied();

        // Validate histogram is logged correctly.
        histogramWatcher.assertExpected();
    }

    /**
     * Verifies that when `SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS` and
     * `REPORT_NOTIFICATION_CONTENT_DETECTION_DATA` are enabled, unsubscribing on an unwarned
     * notification allows the user to "Report as spam".
     */
    @Test
    @LargeTest
    @Feature({"Browser", "Notifications"})
    @Features.EnableFeatures({
        ChromeFeatureList.REPORT_NOTIFICATION_CONTENT_DETECTION_DATA,
        ChromeFeatureList.SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS
    })
    @DisabledTest(message = "Flaky, see crbug.com/431949515")
    public void testReportUnwarnedNotificationAsSpam() throws Exception {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME,
                                SuspiciousNotificationWarningInteractions
                                        .REPORT_UNWARNED_NOTIFICATION_AS_SPAM)
                        .expectIntRecord(
                                SAFE_BROWSING_NOTIFICATION_REVOCATION_SOURCE_HISTOGRAM_NAME,
                                STANDARD_ONE_TAP_UNSUBSCRIBE_EVENT)
                        .build();

        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());
        Assert.assertEquals("\"granted\"", runJavaScript("Notification.permission"));

        NotificationPlatformBridge notificationBridge =
                NotificationPlatformBridge.getInstanceForTests();
        Assert.assertNotNull(notificationBridge);
        notificationBridge.setIsSuspiciousParameterForTesting(false);

        // Display 1 notification.
        showAndGetNotification("MyNotification1", "{body: 'Hello'}");
        mNotificationTestRule.waitForNotificationCount(1);

        // Before unsubscribing, check that the suspicious notification map contains 0 entries.
        Assert.assertEquals(
                0, NotificationContentDetectionManager.sSuspiciousNotificationsMap.size());

        // Tap the "Unsubscribe" button.
        List<NotificationEntry> notifications = mNotificationTestRule.getNotificationEntries();
        Notification unwarnedNotification = notifications.get(0).getNotification();
        PendingIntent unsubscribeIntent = unwarnedNotification.actions[0].actionIntent;
        Assert.assertNotNull(unsubscribeIntent);
        unsubscribeIntent.send();

        // Tap the "Report" button on the unsubscribe confirmation notification.
        Notification unsubscribeConfirmationNotification =
                mNotificationTestRule.waitForNotification().notification;
        Assert.assertEquals(
                "Unsubscribed",
                NotificationTestUtil.getExtraTitle(unsubscribeConfirmationNotification));
        Assert.assertEquals(
                "Help improve spam protection by sharing notification content and the site's URL"
                        + " with Google. Content may be reviewed by humans.",
                NotificationTestUtil.getExtraText(unsubscribeConfirmationNotification));
        Assert.assertEquals(2, unsubscribeConfirmationNotification.actions.length);
        PendingIntent reportIntent = unsubscribeConfirmationNotification.actions[1].actionIntent;
        Assert.assertNotNull(reportIntent);
        reportIntent.send();

        // Notification with "report" button should have been dismissed.
        mNotificationTestRule.waitForNotificationCount(0);

        // This should have caused notifications permission to become reset.
        Assert.assertEquals("\"default\"", runJavaScript("Notification.permission"));
        checkThatShowNotificationIsDenied();

        // Validate histogram is logged correctly.
        histogramWatcher.assertExpected();
    }

    /**
     * Verifies that when `SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS` and
     * `REPORT_NOTIFICATION_CONTENT_DETECTION_DATA` are enabled and a notification warning is shown,
     * the "Report as spam" option is not available if the user did not show the original
     * notification first.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "Notifications"})
    @Features.EnableFeatures({
        ChromeFeatureList.REPORT_NOTIFICATION_CONTENT_DETECTION_DATA,
        ChromeFeatureList.SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS
    })
    public void testReportOptionNotAvailableIfNoOriginalShown() throws Exception {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME,
                                SuspiciousNotificationWarningInteractions.WARNING_SHOWN,
                                SuspiciousNotificationWarningInteractions.UNSUBSCRIBE)
                        .expectIntRecord(
                                SAFE_BROWSING_NOTIFICATION_REVOCATION_SOURCE_HISTOGRAM_NAME,
                                SUSPICIOUS_WARNING_ONE_TAP_UNSUBSCRIBE_EVENT)
                        .build();

        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());
        Assert.assertEquals("\"granted\"", runJavaScript("Notification.permission"));

        NotificationPlatformBridge notificationBridge =
                NotificationPlatformBridge.getInstanceForTests();
        Assert.assertNotNull(notificationBridge);
        notificationBridge.setIsSuspiciousParameterForTesting(true);

        // Display 1 notification.
        showAndGetNotification("MyNotification1", "{body: 'Hello'}");
        mNotificationTestRule.waitForNotificationCount(1);

        // Before unsubscribing, check that the suspicious notification map contains 1 entry for the
        // origin.
        Assert.assertEquals(
                1,
                NotificationContentDetectionManager.sSuspiciousNotificationsMap
                        .get(mPermissionTestRule.getOrigin())
                        .size());

        // Set to false so the "Unsubscribe" confirmation notification will not be marked as
        // suspicious.
        notificationBridge.setIsSuspiciousParameterForTesting(false);

        // Tap the "Unsubscribe" button.
        List<NotificationEntry> notifications = mNotificationTestRule.getNotificationEntries();
        Notification warningNotification = notifications.get(0).getNotification();
        PendingIntent unsubscribeIntent = warningNotification.actions[0].actionIntent;
        Assert.assertNotNull(unsubscribeIntent);
        unsubscribeIntent.send();

        // Check that unsubscribe confirmation notification does not have "Report" option.
        Notification unsubscribeConfirmationNotification =
                mNotificationTestRule.waitForNotification().notification;
        Assert.assertEquals(
                "Unsubscribed",
                NotificationTestUtil.getExtraTitle(unsubscribeConfirmationNotification));
        String expectedOrigin =
                UrlFormatter.formatUrlForSecurityDisplay(
                        mPermissionTestRule.getOrigin(), SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        Assert.assertTrue(
                NotificationTestUtil.getExtraText(unsubscribeConfirmationNotification)
                        .contains("You'll no longer receive notifications from " + expectedOrigin));
        Assert.assertEquals(2, unsubscribeConfirmationNotification.actions.length);
        // Click the "Okay" button to commit. This is the second button.
        PendingIntent commitIntent = unsubscribeConfirmationNotification.actions[1].actionIntent;
        Assert.assertNotNull(commitIntent);
        commitIntent.send();

        // Wait for the `provisionally unsubscribed` notification to disappear.
        mNotificationTestRule.waitForNotificationCount(0);

        // This should have caused notifications permission to become reset.
        Assert.assertEquals("\"default\"", runJavaScript("Notification.permission"));
        checkThatShowNotificationIsDenied();

        // Validate histogram is logged correctly.
        histogramWatcher.assertExpected();
    }

    /**
     * Verifies that committing an unsubscribe from notifications cleans up suspicious notification
     * backups. When the user resubscribes, the warning should show that there is only 1 spam
     * notification.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "Notifications"})
    @Features.EnableFeatures({
        ChromeFeatureList.REPORT_NOTIFICATION_CONTENT_DETECTION_DATA,
        ChromeFeatureList.SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS
    })
    public void
            testWarningShowsOneNotificationAfterMultipleWarnedSpamThenUnsubscribeThenResubscribe()
                    throws Exception {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME,
                                SuspiciousNotificationWarningInteractions.WARNING_SHOWN,
                                2)
                        .expectIntRecords(
                                SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME,
                                SuspiciousNotificationWarningInteractions.UNSUBSCRIBE,
                                SuspiciousNotificationWarningInteractions
                                        .SHOW_ORIGINAL_NOTIFICATION,
                                SuspiciousNotificationWarningInteractions
                                        .SUPPRESS_DUPLICATE_WARNING)
                        .expectIntRecord(
                                SUSPICIOUS_NOTIFICATION_COUNT_SHOW_ORIGINALS_HISTOGRAM_NAME, 1)
                        .expectIntRecord(
                                SUSPICIOUS_NOTIFICATION_COUNT_DROPPED_SHOW_ORIGINALS_HISTOGRAM_NAME,
                                0)
                        .expectIntRecord(
                                SAFE_BROWSING_NOTIFICATION_REVOCATION_SOURCE_HISTOGRAM_NAME,
                                SUSPICIOUS_WARNING_ONE_TAP_UNSUBSCRIBE_EVENT)
                        .build();

        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());
        Assert.assertEquals("\"granted\"", runJavaScript("Notification.permission"));

        NotificationPlatformBridge notificationBridge =
                NotificationPlatformBridge.getInstanceForTests();
        Assert.assertNotNull(notificationBridge);
        notificationBridge.setIsSuspiciousParameterForTesting(true);

        // Display 2 suspicious notifications and unsubscribe.
        showNotification("MyNotification1", "{body: 'Hello'}");
        mNotificationTestRule.waitForNotificationCount(1);
        showNotification("MyNotification2", "{body: 'Hello'}");
        mNotificationTestRule.waitForNotificationManagerMutation();
        Notification warningNotification =
                mNotificationTestRule.getNotificationEntries().get(0).getNotification();
        Assert.assertEquals(
                "Possible spam (2)", NotificationTestUtil.getExtraTitle(warningNotification));
        PendingIntent unsubscribeIntent = warningNotification.actions[0].actionIntent;
        Assert.assertNotNull(unsubscribeIntent);
        unsubscribeIntent.send();

        // Tap confirm on the "provisionally unsubscribed" notification.
        Notification provisionallyUnsubscribedNotification =
                mNotificationTestRule.waitForNotification().notification;
        Assert.assertEquals(2, provisionallyUnsubscribedNotification.actions.length);
        PendingIntent commitIntent = provisionallyUnsubscribedNotification.actions[1].actionIntent;
        Assert.assertNotNull(commitIntent);
        commitIntent.send();

        // Wait for the notification to be removed.
        mNotificationTestRule.waitForNotificationCount(0);

        // Re-subscribe to notifications.
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());
        Assert.assertEquals("\"granted\"", runJavaScript("Notification.permission"));

        // Show a new warning, which shows that there is only 1 spam notification.
        Notification newWarning = showAndGetNotification("MyNotification3", "{body: 'Hello'}");
        Assert.assertEquals("Possible spam", NotificationTestUtil.getExtraTitle(newWarning));
        PendingIntent showOriginalPendingIntent = newWarning.actions[1].actionIntent;
        showOriginalPendingIntent.send();
        Notification originalNotification =
                mNotificationTestRule.waitForNotification().notification;
        Assert.assertEquals(
                "MyNotification3", NotificationTestUtil.getExtraTitle(originalNotification));

        // Validate histogram is logged correctly.
        histogramWatcher.assertExpected();
    }

    /**
     * Verifies that when the front end storage of suspicious notification backups is deleted, the
     * first notification is still displayed and the
     * `SUSPICIOUS_NOTIFICATION_COUNT_DROPPED_SHOW_ORIGINALS_HISTOGRAM_NAME` histogram logs the
     * correct number of suspicious notifications that were unexpectedly dropped.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "Notifications"})
    @Features.EnableFeatures({
        ChromeFeatureList.SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS
    })
    public void testShowOriginalNotificationsAfterDeletingBackups() throws Exception {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME,
                                SuspiciousNotificationWarningInteractions.WARNING_SHOWN,
                                1)
                        .expectIntRecordTimes(
                                SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME,
                                SuspiciousNotificationWarningInteractions
                                        .SUPPRESS_DUPLICATE_WARNING,
                                2)
                        .expectIntRecordTimes(
                                SUSPICIOUS_NOTIFICATION_WARNING_INTERACTIONS_HISTOGRAM_NAME,
                                SuspiciousNotificationWarningInteractions
                                        .SHOW_ORIGINAL_NOTIFICATION,
                                1)
                        .expectIntRecord(
                                SUSPICIOUS_NOTIFICATION_COUNT_SHOW_ORIGINALS_HISTOGRAM_NAME, 3)
                        .expectIntRecord(
                                SUSPICIOUS_NOTIFICATION_COUNT_DROPPED_SHOW_ORIGINALS_HISTOGRAM_NAME,
                                2)
                        .build();

        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSetting.ALLOW, mPermissionTestRule.getOrigin());
        Assert.assertEquals("\"granted\"", runJavaScript("Notification.permission"));

        // Display 3 suspicious notifications which are replaced by a single warning.
        NotificationPlatformBridge notificationBridge =
                NotificationPlatformBridge.getInstanceForTests();
        Assert.assertNotNull(notificationBridge);
        notificationBridge.setIsSuspiciousParameterForTesting(true);
        showNotification("MyNotification1", "{}");
        mNotificationTestRule.waitForNotificationCount(1);
        showNotification("MyNotification2", "{}");
        mNotificationTestRule.waitForNotificationManagerMutation();
        showNotification("MyNotification3", "{}");
        mNotificationTestRule.waitForNotificationManagerMutation();
        Notification warningNotification =
                mNotificationTestRule.getNotificationEntries().get(0).getNotification();
        Assert.assertEquals(
                "Possible spam (3)", NotificationTestUtil.getExtraTitle(warningNotification));

        // Delete suspicious notification backups stored in the
        // `NotificationContentDetectionManager`.
        NotificationContentDetectionManager.sWarningNotificationAttributesByOrigin.clear();

        // Tap "Show notification(s)".
        PendingIntent showOriginalsIntent = warningNotification.actions[1].actionIntent;
        Assert.assertNotNull(showOriginalsIntent);
        showOriginalsIntent.send();

        // Check that the first notification was still delivered.
        mNotificationTestRule.waitForNotificationManagerMutation();
        Assert.assertEquals(1, mNotificationTestRule.getNotificationEntries().size());
        Notification originalNotification =
                mNotificationTestRule.getNotificationEntries().get(0).getNotification();
        Assert.assertEquals(
                "MyNotification1", NotificationTestUtil.getExtraTitle(originalNotification));

        // Validate histogram is logged correctly.
        histogramWatcher.assertExpected();
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
        mNotificationTestRule.flushNotificationManagerMutations();
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
