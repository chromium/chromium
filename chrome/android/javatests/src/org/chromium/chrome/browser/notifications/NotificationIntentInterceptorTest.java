// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.Until;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

/**
 * Test to verify intercepting notification pending intents with broadcast receiver.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class NotificationIntentInterceptorTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEST_NOTIFICATION_TITLE = "Test notification title.";
    private static final long WAIT_FOR_NOTIFICATION_SHOWN_TIMEOUT_MILLISECONDS = 3000;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    // Builds a simple notification used in tests.
    private Notification buildSimpleNotification(String title) {
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory.createChromeNotificationBuilder(true /* preferCompat */,
                        ChannelDefinitions.ChannelId.DOWNLOADS, null /* remoteAppPackageName */,
                        NotificationTestUtil.getTestNotificationMetadata());

        // Set content intent. UI automator may tap the notification and expand the action buttons,
        // in order to reduce flakiness, don't add action button.
        Context context = ContextUtils.getApplicationContext();
        Intent contentIntent = new Intent(context, ChromeTabbedActivity.class);
        Uri uri = Uri.parse("www.example.com");
        contentIntent.setData(uri);
        contentIntent.setAction(Intent.ACTION_VIEW);
        PendingIntentProvider contentPendingIntent =
                PendingIntentProvider.getActivity(context, 0, contentIntent, 0);
        assert contentPendingIntent != null;
        builder.setContentIntent(contentPendingIntent);
        builder.setContentTitle(title);
        builder.setSmallIcon(R.drawable.offline_pin);
        return builder.build();
    }

    /**
     * Clicks the notification with UI automator. Notice the notification bar is not part of the
     * app, so we have to use UI automator.
     * @param text The text of notification UI.
     */
    private void clickNotification(String text) {
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        device.openNotification();
        device.wait(Until.hasObject(By.textContains(text)),
                WAIT_FOR_NOTIFICATION_SHOWN_TIMEOUT_MILLISECONDS);
        UiObject2 textObject = device.findObject(By.textContains(text));
        Assert.assertEquals(text, textObject.getText());
        textObject.click();
    }

    /**
     * Verifies {@link Notification#contentIntent} can be intercepted by broadcast receiver.
     * Action button and dismiss have no test coverage due to difficulty in simulation with UI
     * automator. On different Android version, the way to dismiss or find the action button can be
     * different.
     */
    @Test
    @DisabledTest(message = "https://crbug.com/910870")
    @MediumTest
    @RetryOnFailure
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    public void testContentIntentInterception() {
        // Send notification.
        NotificationManager notificationManager =
                (NotificationManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.NOTIFICATION_SERVICE);
        notificationManager.cancelAll();
        notificationManager.notify(0, buildSimpleNotification(TEST_NOTIFICATION_TITLE));

        // Click notification body.
        clickNotification(TEST_NOTIFICATION_TITLE);

        // Wait for another tab to load.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mActivityTestRule.tabsCount(false) > 1;
            }
        });
        notificationManager.cancelAll();
    }
}
