// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;

/** Unit tests for NotificationBuilderBase. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NotificationBuilderBaseTest {
    /**
     * Tests the three paths for ensuring that a notification will be shown with a normalized icon:
     * (1) NULL bitmaps should have an auto-generated image. (2) Large bitmaps should be resized to
     * the device's intended size. (3) Smaller bitmaps should be left alone.
     */
    @Test
    @Feature({"Browser", "Notifications"})
    public void testEnsureNormalizedIconBehavior() {
        // Get the dimensions of the notification icon that will be presented to the user.
        Context appContext = RuntimeEnvironment.getApplication();
        Resources resources = appContext.getResources();

        int largeIconWidthPx =
                resources.getDimensionPixelSize(android.R.dimen.notification_large_icon_width);
        int largeIconHeightPx =
                resources.getDimensionPixelSize(android.R.dimen.notification_large_icon_height);

        String origin = "https://example.com";

        NotificationBuilderBase notificationBuilder =
                new NotificationBuilderBase(resources) {
                    @Override
                    public NotificationWrapper build(NotificationMetadata metadata) {
                        return null;
                    }
                };
        notificationBuilder.setChannelId(ChromeChannelDefinitions.ChannelId.BROWSER);
        Bitmap fromNullIcon = notificationBuilder.ensureNormalizedIcon(null, origin);
        Assert.assertNotNull(fromNullIcon);
        Assert.assertEquals(largeIconWidthPx, fromNullIcon.getWidth());
        Assert.assertEquals(largeIconHeightPx, fromNullIcon.getHeight());

        Bitmap largeIcon =
                Bitmap.createBitmap(
                        largeIconWidthPx * 2, largeIconHeightPx * 2, Bitmap.Config.ALPHA_8);

        Bitmap fromLargeIcon = notificationBuilder.ensureNormalizedIcon(largeIcon, origin);
        Assert.assertNotNull(fromLargeIcon);
        Assert.assertEquals(largeIconWidthPx, fromLargeIcon.getWidth());
        Assert.assertEquals(largeIconHeightPx, fromLargeIcon.getHeight());

        Bitmap smallIcon =
                Bitmap.createBitmap(
                        largeIconWidthPx / 2, largeIconHeightPx / 2, Bitmap.Config.ALPHA_8);

        Bitmap fromSmallIcon = notificationBuilder.ensureNormalizedIcon(smallIcon, origin);
        Assert.assertNotNull(fromSmallIcon);
        Assert.assertEquals(smallIcon, fromSmallIcon);
    }
}
