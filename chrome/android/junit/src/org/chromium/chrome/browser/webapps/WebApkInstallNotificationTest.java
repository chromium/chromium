// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Notification;
import android.app.Notification.Action;
import android.app.NotificationManager;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotificationManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.components.webapps.WebApkInstallResult;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests WebAPKs install notifications from {@link WebApkInstallService}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowNotificationManager.class})
public class WebApkInstallNotificationTest {
    private static final String PACKAGE_NAME = "org.chromium.webapk.for.testing";
    private static final String MANIFEST_URL = "https://test.com/manifest.json";
    private static final String SHORT_NAME = "webapk";
    private static final String URL = "https://test.com";

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private UrlFormatter.Natives mUrlFormatterJniMock;

    private final Bitmap mIcon = Bitmap.createBitmap(1, 1, Bitmap.Config.ALPHA_8);
    private Context mContext;
    private ShadowNotificationManager mShadowNotificationManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UrlFormatterJni.TEST_HOOKS, mUrlFormatterJniMock);
        when(mUrlFormatterJniMock.formatStringUrlForSecurityDisplay(
                     anyString(), eq(SchemeDisplay.OMIT_HTTP_AND_HTTPS)))
                .then(inv -> inv.getArgument(0));

        mContext = ApplicationProvider.getApplicationContext();
        ContextUtils.initApplicationContextForTests(mContext);
        mShadowNotificationManager = shadowOf(
                (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE));
    }

    @Test
    public void testInProgressNotification() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WebApkInstallService.showInstallInProgressNotification(
                    MANIFEST_URL, SHORT_NAME, URL, mIcon, false /* isIconMaskable */);
        });

        String notificationId = WebApkInstallService.getInstallNotificationTag(MANIFEST_URL);
        Notification notification = mShadowNotificationManager.getAllNotifications().get(0);

        Assert.assertNotNull(notification);
        Assert.assertEquals(SHORT_NAME, notification.extras.getString(Notification.EXTRA_TITLE));
        Assert.assertEquals(
                ChromeChannelDefinitions.ChannelId.BROWSER, notification.getChannelId());
        Assert.assertEquals(
                mContext.getString(R.string.notification_webapk_install_in_progress, SHORT_NAME),
                notification.extras.getString(Notification.EXTRA_TEXT));

        Bitmap largeIcon =
                ((BitmapDrawable) notification.getLargeIcon().loadDrawable(mContext)).getBitmap();
        Assert.assertTrue(mIcon.sameAs(largeIcon));
        Bitmap expectedSmallIcon =
                BitmapFactory.decodeResource(mContext.getResources(), R.drawable.ic_chrome);
        Bitmap smallIcon =
                ((BitmapDrawable) notification.getSmallIcon().loadDrawable(mContext)).getBitmap();
        Assert.assertTrue(expectedSmallIcon.sameAs(smallIcon));
    }

    @Test
    public void testCompleteNotification() {
        WebApkInstallService.showInstalledNotification(
                PACKAGE_NAME, MANIFEST_URL, SHORT_NAME, URL, mIcon, false /* isIconMaskable */);

        Notification notification = mShadowNotificationManager.getAllNotifications().get(0);

        Assert.assertNotNull(notification);
        Assert.assertEquals(SHORT_NAME, notification.extras.getString(Notification.EXTRA_TITLE));
        Assert.assertEquals(
                ChromeChannelDefinitions.ChannelId.WEBAPPS, notification.getChannelId());
        Assert.assertEquals(mContext.getString(R.string.notification_webapk_installed),
                notification.extras.getString(Notification.EXTRA_TEXT));

        Bitmap largeIcon =
                ((BitmapDrawable) notification.getLargeIcon().loadDrawable(mContext)).getBitmap();
        Assert.assertTrue(mIcon.sameAs(largeIcon));
        Bitmap expectedSmallIcon =
                BitmapFactory.decodeResource(mContext.getResources(), R.drawable.ic_chrome);
        Bitmap smallIcon =
                ((BitmapDrawable) notification.getSmallIcon().loadDrawable(mContext)).getBitmap();
        Assert.assertTrue(expectedSmallIcon.sameAs(smallIcon));

        Assert.assertNotNull(notification.contentIntent);
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.WEB_APK_INSTALL_FAILURE_NOTIFICATION,
            ChromeFeatureList.WEB_APK_INSTALL_RETRY})
    public void
    testFailureNotification() {
        WebApkInstallService.showInstallFailedNotification(MANIFEST_URL, SHORT_NAME, URL, mIcon,
                false /* isIconMaskable */, WebApkInstallResult.FAILURE, null);

        Notification notification = mShadowNotificationManager.getAllNotifications().get(0);

        Assert.assertNotNull(notification);
        Assert.assertEquals(
                mContext.getString(R.string.notification_webapk_install_failed, SHORT_NAME),
                notification.extras.getString(Notification.EXTRA_TITLE));
        Assert.assertEquals(
                ChromeChannelDefinitions.ChannelId.WEBAPPS, notification.getChannelId());
        Assert.assertEquals(
                mContext.getString(
                        R.string.notification_webapk_install_failed_contents_general, SHORT_NAME),
                notification.extras.getString(Notification.EXTRA_TEXT));

        Bitmap largeIcon =
                ((BitmapDrawable) notification.getLargeIcon().loadDrawable(mContext)).getBitmap();
        Assert.assertTrue(mIcon.sameAs(largeIcon));
        Bitmap expectedSmallIcon =
                BitmapFactory.decodeResource(mContext.getResources(), R.drawable.ic_chrome);
        Bitmap smallIcon =
                ((BitmapDrawable) notification.getSmallIcon().loadDrawable(mContext)).getBitmap();
        Assert.assertTrue(expectedSmallIcon.sameAs(smallIcon));

        Assert.assertNotNull(notification.contentIntent);

        Action[] actions = notification.actions;
        Assert.assertEquals(1, actions.length);
        Assert.assertEquals(
                mContext.getString(R.string.webapk_install_failed_action_open), actions[0].title);
        Assert.assertNotNull(actions[0].actionIntent);
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.WEB_APK_INSTALL_FAILURE_NOTIFICATION,
            ChromeFeatureList.WEB_APK_INSTALL_RETRY})
    public void
    testFailureNotificationWithRetryAction() {
        byte[] serializedProto = new byte[] {1, 2};
        WebApkInstallService.showInstallFailedNotification(MANIFEST_URL, SHORT_NAME, URL, mIcon,
                false /* isIconMaskable */, WebApkInstallResult.FAILURE, serializedProto);

        Notification notification = mShadowNotificationManager.getAllNotifications().get(0);

        Assert.assertNotNull(notification);
        Assert.assertEquals(
                mContext.getString(R.string.notification_webapk_install_failed, SHORT_NAME),
                notification.extras.getString(Notification.EXTRA_TITLE));
        Assert.assertEquals(
                ChromeChannelDefinitions.ChannelId.WEBAPPS, notification.getChannelId());
        Assert.assertEquals(
                mContext.getString(
                        R.string.notification_webapk_install_failed_contents_general, SHORT_NAME),
                notification.extras.getString(Notification.EXTRA_TEXT));

        Bitmap largeIcon =
                ((BitmapDrawable) notification.getLargeIcon().loadDrawable(mContext)).getBitmap();
        Assert.assertTrue(mIcon.sameAs(largeIcon));
        Bitmap expectedSmallIcon =
                BitmapFactory.decodeResource(mContext.getResources(), R.drawable.ic_chrome);
        Bitmap smallIcon =
                ((BitmapDrawable) notification.getSmallIcon().loadDrawable(mContext)).getBitmap();
        Assert.assertTrue(expectedSmallIcon.sameAs(smallIcon));

        Assert.assertNotNull(notification.contentIntent);

        Action[] actions = notification.actions;
        Assert.assertEquals(2, actions.length);
        Assert.assertEquals(
                mContext.getString(R.string.webapk_install_failed_action_retry), actions[0].title);
        Assert.assertNotNull(actions[0].actionIntent);
        Assert.assertEquals(
                mContext.getString(R.string.webapk_install_failed_action_open), actions[1].title);
        Assert.assertNotNull(actions[1].actionIntent);
    }
}
