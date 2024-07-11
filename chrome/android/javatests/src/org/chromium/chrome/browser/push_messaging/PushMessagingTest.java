// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.push_messaging;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import android.app.Notification;
import android.os.Bundle;
import android.util.Pair;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.notifications.NotificationTestRule;
import org.chromium.chrome.browser.permissions.PermissionTestRule;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy.NotificationEntry;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.gcm_driver.GCMDriver;
import org.chromium.components.gcm_driver.GCMMessage;
import org.chromium.components.gcm_driver.instance_id.FakeInstanceIDWithSubtype;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for the Push API and the integration with the Notifications API on Android.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures({ChromeFeatureList.PUSH_MESSAGING_DISALLOW_SENDER_IDS})
public class PushMessagingTest implements PushMessagingServiceObserver.Listener {
    @Rule public EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();

    @Rule public NotificationTestRule mNotificationTestRule = new NotificationTestRule();

    private static final String PUSH_TEST_PAGE =
            "/chrome/test/data/push_messaging/push_messaging_test_android.html";
    private static final String ABOUT_BLANK = "about:blank";
    private static final int TITLE_UPDATE_TIMEOUT_SECONDS = 5;

    private final CallbackHelper mMessageHandledHelper;
    private String mPushTestPage;

    public PushMessagingTest() {
        mMessageHandledHelper = new CallbackHelper();
    }

    @Before
    public void setUp() {
        final PushMessagingServiceObserver.Listener listener = this;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FakeInstanceIDWithSubtype.clearDataAndSetEnabled(true);
                    PushMessagingServiceObserver.setListenerForTesting(listener);
                });
        mPushTestPage = mEmbeddedTestServerRule.getServer().getURL(PUSH_TEST_PAGE);
        mNotificationTestRule.loadUrl(mPushTestPage);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PushMessagingServiceObserver.setListenerForTesting(null);
                    FakeInstanceIDWithSubtype.clearDataAndSetEnabled(false);
                });
    }

    @Override
    public void onMessageHandled() {
        mMessageHandledHelper.notifyCalled();
    }

    /**
     * Verifies that PushManager.subscribe() fails if Notifications permission was already denied.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "PushMessaging"})
    public void testNotificationsPermissionDenied() throws TimeoutException {
        // Deny Notifications permission before trying to subscribe Push.
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.BLOCK, mEmbeddedTestServerRule.getOrigin());
        Assert.assertEquals("\"denied\"", runScriptBlocking("Notification.permission"));

        // Reload page to ensure the block is persisted.
        mNotificationTestRule.loadUrl(mPushTestPage);

        // PushManager.subscribePush() should fail immediately without showing a prompt.
        runScriptAndWaitForTitle(
                "subscribePush()",
                "subscribe fail: NotAllowedError: Registration failed - permission denied");
        Assert.assertFalse(
                "Permission prompt should not be shown",
                PermissionDialogController.getInstance().isDialogShownForTest());

        // Notifications permission should still be denied.
        Assert.assertEquals("\"denied\"", runScriptBlocking("Notification.permission"));
    }

    /** Verifies that PushManager.subscribe() fails if permission is dismissed or blocked. */
    @Test
    @MediumTest
    @Feature({"Browser", "PushMessaging"})
    public void testPushPermissionDenied() throws TimeoutException {
        // Notifications permission should initially be prompt.
        Assert.assertEquals("\"default\"", runScriptBlocking("Notification.permission"));

        // PushManager.subscribePush() should show the notifications permission prompt.
        Assert.assertFalse(
                "Permission prompt should not be shown",
                PermissionDialogController.getInstance().isDialogShownForTest());
        runScript("subscribePush()");

        // Dismissing the prompt should cause subscribe() to fail.
        PermissionTestRule.waitForDialog(mNotificationTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mNotificationTestRule.getActivity().onBackPressed();
                });

        waitForTitle(
                mNotificationTestRule.getActivity().getActivityTab(),
                "subscribe fail: NotAllowedError: Registration failed - permission denied");

        // Notifications permission should still be prompt.
        Assert.assertEquals("\"default\"", runScriptBlocking("Notification.permission"));

        runScriptAndWaitForTitle("sendToTest('reset title')", "reset title");

        // PushManager.subscribePush() should show the notifications permission prompt again.
        runScript("subscribePush()");

        // Denying the prompt should cause subscribe() to fail.
        PermissionTestRule.waitForDialog(mNotificationTestRule.getActivity());
        PermissionTestRule.replyToDialog(
                PermissionTestRule.PromptDecision.DENY, mNotificationTestRule.getActivity());
        waitForTitle(
                mNotificationTestRule.getActivity().getActivityTab(),
                "subscribe fail: NotAllowedError: Registration failed - permission denied");

        // This should have caused notifications permission to become denied.
        Assert.assertEquals("\"denied\"", runScriptBlocking("Notification.permission"));

        // Reload page to ensure the block is persisted.
        mNotificationTestRule.loadUrl(mPushTestPage);

        // PushManager.subscribePush() should now fail immediately without showing a permission
        // prompt.
        runScriptAndWaitForTitle(
                "subscribePush()",
                "subscribe fail: NotAllowedError: Registration failed - permission denied");
        Assert.assertFalse(
                "Permission prompt should not be shown",
                PermissionDialogController.getInstance().isDialogShownForTest());

        // Notifications permission should still be denied.
        Assert.assertEquals("\"denied\"", runScriptBlocking("Notification.permission"));
    }

    /** Verifies that PushManager.subscribe() requests permission correctly. */
    @Test
    @MediumTest
    @Feature({"Browser", "PushMessaging"})
    @DisabledTest(message = "Disabled for flakiness, see https://crbug.com/1442707")
    public void testPushPermissionGranted() throws TimeoutException {
        // Notifications permission should initially be prompt.
        Assert.assertEquals("\"default\"", runScriptBlocking("Notification.permission"));

        // PushManager.subscribePush() should show the notifications permission prompt.
        Assert.assertFalse(
                "Permission prompt should not be shown",
                PermissionDialogController.getInstance().isDialogShownForTest());
        runScript("subscribePush()");

        // Accepting the prompt should cause subscribe() to succeed.
        PermissionTestRule.waitForDialog(mNotificationTestRule.getActivity());
        PermissionTestRule.replyToDialog(
                PermissionTestRule.PromptDecision.ALLOW, mNotificationTestRule.getActivity());
        waitForTitle(mNotificationTestRule.getActivity().getActivityTab(), "subscribe ok");

        // This should have caused notifications permission to become granted.
        Assert.assertEquals("\"granted\"", runScriptBlocking("Notification.permission"));
    }

    /**
     * Verifies that a notification can be shown from a push event handler in the service worker.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "PushMessaging"})
    @DisabledTest(message = "https://crbug.com/707528")
    public void testPushAndShowNotification() throws TimeoutException {
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mEmbeddedTestServerRule.getOrigin());
        runScriptAndWaitForTitle("subscribePush()", "subscribe ok");

        Pair<String, String> appIdAndSenderId =
                FakeInstanceIDWithSubtype.getSubtypeAndAuthorizedEntityOfOnlyToken();
        sendPushAndWaitForCallback(appIdAndSenderId);
        NotificationEntry notificationEntry = mNotificationTestRule.waitForNotification();
        Assert.assertEquals(
                "push notification 1",
                notificationEntry.notification.extras.getString(Notification.EXTRA_TITLE));
    }

    /**
     * Verifies that the default notification is shown when no notification is shown from the push
     * event handler while no tab is visible for the origin, and grace has been exceeded.
     */
    @Test
    @LargeTest
    @Feature({"Browser", "PushMessaging"})
    @DisabledTest(message = "https://crbug.com/707528")
    public void testDefaultNotification() throws TimeoutException {
        // Start off using the tab loaded in setUp().
        Assert.assertEquals(1, mNotificationTestRule.getActivity().getCurrentTabModel().getCount());
        Tab tab = mNotificationTestRule.getActivity().getActivityTab();
        Assert.assertEquals(mPushTestPage, tab.getUrl().getSpec());
        Assert.assertFalse(tab.isHidden());

        // Set up the push subscription and capture its details.
        mNotificationTestRule.setNotificationContentSettingForOrigin(
                ContentSettingValues.ALLOW, mEmbeddedTestServerRule.getOrigin());
        runScriptAndWaitForTitle("subscribePush()", "subscribe ok");
        Pair<String, String> appIdAndSenderId =
                FakeInstanceIDWithSubtype.getSubtypeAndAuthorizedEntityOfOnlyToken();

        // Make the tab invisible by opening another one with a different origin.
        mNotificationTestRule.loadUrlInNewTab(ABOUT_BLANK);
        Assert.assertEquals(2, mNotificationTestRule.getActivity().getCurrentTabModel().getCount());
        Assert.assertEquals(
                ABOUT_BLANK,
                mNotificationTestRule.getActivity().getActivityTab().getUrl().getSpec());
        Assert.assertTrue(tab.isHidden());

        // The first time a push event is fired and no notification is shown from the service
        // worker, grace permits it so no default notification is shown.
        runScriptAndWaitForTitle("setNotifyOnPush(false)", "setNotifyOnPush false ok", tab);
        sendPushAndWaitForCallback(appIdAndSenderId);

        // After grace runs out a default notification will be shown.
        sendPushAndWaitForCallback(appIdAndSenderId);
        NotificationEntry notificationEntry = mNotificationTestRule.waitForNotification();
        assertThat(
                notificationEntry.tag, Matchers.containsString("user_visible_auto_notification"));

        // When another push does show a notification, the default notification is automatically
        // dismissed (an additional mutation) so there is only one left in the end.
        runScriptAndWaitForTitle("setNotifyOnPush(true)", "setNotifyOnPush true ok", tab);
        sendPushAndWaitForCallback(appIdAndSenderId);
        mNotificationTestRule.waitForNotificationManagerMutation();
        notificationEntry = mNotificationTestRule.waitForNotification();
        Assert.assertEquals(
                "push notification 1",
                notificationEntry.notification.extras.getString(Notification.EXTRA_TITLE));
    }

    /** Runs {@code script} in the current tab but does not wait for the result. */
    private void runScript(String script) {
        JavaScriptUtils.executeJavaScript(mNotificationTestRule.getWebContents(), script);
    }

    /** Runs {@code script} in the current tab and returns its synchronous result in JSON format. */
    private String runScriptBlocking(String script) throws TimeoutException {
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mNotificationTestRule.getWebContents(), script);
    }

    /**
     * Runs {@code script} in the current tab and waits for the tab title to change to {@code
     * expectedTitle}.
     */
    private void runScriptAndWaitForTitle(String script, String expectedTitle) {
        runScriptAndWaitForTitle(
                script, expectedTitle, mNotificationTestRule.getActivity().getActivityTab());
    }

    /**
     * Runs {@code script} in {@code tab} and waits for the tab title to change to {@code
     * expectedTitle}.
     */
    private void runScriptAndWaitForTitle(String script, String expectedTitle, Tab tab) {
        JavaScriptUtils.executeJavaScript(tab.getWebContents(), script);
        waitForTitle(tab, expectedTitle);
    }

    private void sendPushAndWaitForCallback(Pair<String, String> appIdAndSenderId)
            throws TimeoutException {
        final String appId = appIdAndSenderId.first;
        final String senderId = appIdAndSenderId.second;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Bundle extras = new Bundle();
                    extras.putString("subtype", appId);

                    GCMMessage message = new GCMMessage(senderId, extras);
                    ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
                    GCMDriver.dispatchMessage(message);
                });
        mMessageHandledHelper.waitForCallback(mMessageHandledHelper.getCallCount());
    }

    @SuppressWarnings("MissingFail")
    private void waitForTitle(Tab tab, String expectedTitle) {
        TabTitleObserver titleObserver = new TabTitleObserver(tab, expectedTitle);
        try {
            titleObserver.waitForTitleUpdate(TITLE_UPDATE_TIMEOUT_SECONDS);
        } catch (TimeoutException e) {
            // The title is not as expected, this assertion neatly logs what the difference is.
            Assert.assertEquals(expectedTitle, tab.getTitle());
        }
    }
}
