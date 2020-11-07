// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.annotation.TargetApi;
import android.app.Instrumentation.ActivityMonitor;
import android.app.Notification;
import android.app.NotificationManager;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.service.notification.StatusBarNotification;
import android.support.test.InstrumentationRegistry;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for a standalone Web App notification governed by {@link WebappActionsNotificationManager}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@MinAndroidSdkLevel(Build.VERSION_CODES.M) // NotificationManager.getActiveNotifications
@TargetApi(Build.VERSION_CODES.M)
public class WebappActionsNotificationTest {
    private static final String WEB_APP_PATH = "/chrome/test/data/banners/manifest_test_page.html";

    @Rule
    public final WebappActivityTestRule mActivityTestRule = new WebappActivityTestRule();

    @Before
    public void startWebapp() {
        mActivityTestRule.startWebappActivity(mActivityTestRule.createIntent().putExtra(
                ShortcutHelper.EXTRA_URL, mActivityTestRule.getTestServer().getURL(WEB_APP_PATH)));
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testNotification_openInChrome() throws Exception {
        Notification notification = getWebappNotification();

        Assert.assertNotNull(notification);
        Assert.assertEquals(
                "webapp short name", notification.extras.getString(Notification.EXTRA_TITLE));
        Assert.assertEquals(
                mActivityTestRule.getActivity().getString(R.string.webapp_tap_to_copy_url),
                notification.extras.getString(Notification.EXTRA_TEXT));
        Assert.assertEquals("Share", notification.actions[0].title);
        Assert.assertEquals("Open in Chrome", notification.actions[1].title);

        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addDataScheme("http");
        final ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(filter, null, false);

        notification.actions[1].actionIntent.send();
        CriteriaHelper.pollInstrumentationThread(() -> {
            return InstrumentationRegistry.getInstrumentation().checkMonitorHit(monitor, 1);
        });

        Assert.assertNull("Notification should no longer be shown", getWebappNotification());
    }

    @Test
    /*
      @SmallTest
      @Feature({"Webapps"})
    */
    @DisabledTest(message = "crbug.com/774491")
    public void testNotification_copyUrl() throws Exception {
        Notification notification = getWebappNotification();
        Assert.assertNotNull(notification);

        notification.contentIntent.send();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ClipboardManager clipboard =
                    (ClipboardManager) mActivityTestRule.getActivity().getSystemService(
                            Context.CLIPBOARD_SERVICE);
            Assert.assertEquals(mActivityTestRule.getTestServer().getURL(WEB_APP_PATH),
                    clipboard.getPrimaryClip().getItemAt(0).getText().toString());
        });
    }

    @Nullable
    private Notification getWebappNotification() {
        NotificationManager nm =
                (NotificationManager) mActivityTestRule.getActivity().getSystemService(
                        Context.NOTIFICATION_SERVICE);
        for (StatusBarNotification sbn : nm.getActiveNotifications()) {
            if (sbn.getId() == NotificationConstants.NOTIFICATION_ID_WEBAPP_ACTIONS) {
                return sbn.getNotification();
            }
        }
        return null;
    }
}
