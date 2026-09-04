// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Application;
import android.app.Notification;
import android.app.Notification.Action;
import android.app.NotificationManager;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotificationManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.AsyncTabParams;
import org.chromium.components.browser_ui.notifications.NotificationFeatureMap;
import org.chromium.components.webapps.WebApkInstallResult;
import org.chromium.content_public.browser.WebContents;

/** Tests WebAPKs install notifications from {@link WebApkInstallService}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({NotificationFeatureMap.CACHE_NOTIIFICATIONS_ENABLED})
@Config(shadows = {ShadowNotificationManager.class})
public class WebApkInstallNotificationTest {
    private static final String PACKAGE_NAME = "org.chromium.webapk.for.testing";
    private static final String MANIFEST_URL = "https://test.com/manifest.json";
    private static final String SHORT_NAME = "webapk";
    private static final String URL = "https://test.com";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private final Bitmap mIcon = Bitmap.createBitmap(1, 1, Bitmap.Config.ALPHA_8);
    private Context mContext;
    private ShadowNotificationManager mShadowNotificationManager;

    @Before
    public void setUp() {
        DeviceInfo.setIsDesktopForTesting(false);
        mContext = ApplicationProvider.getApplicationContext();
        ContextUtils.initApplicationContextForTests(mContext);
        mShadowNotificationManager =
                shadowOf(
                        (NotificationManager)
                                mContext.getSystemService(Context.NOTIFICATION_SERVICE));
    }

    @After
    public void tearDown() {
        IntentHandler.setTestIntentsEnabled(false);
        DeviceInfo.setIsDesktopForTesting(false);
        ContextUtils.initApplicationContextForTests(ApplicationProvider.getApplicationContext());
        WebApkReparentingHandler.getInstance().clear();
    }

    @Test
    public void testInProgressNotification() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebApkInstallService.showInstallInProgressNotification(
                            MANIFEST_URL, SHORT_NAME, URL, mIcon, /* isIconMaskable= */ false);
                });

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

        Assert.assertEquals(R.drawable.ic_chrome, notification.getSmallIcon().getResId());
    }

    @Test
    public void testCompleteNotification() {
        WebApkInstallService.showInstalledNotificationAndMaybeLaunch(
                /* originatingTab= */ null,
                PACKAGE_NAME,
                MANIFEST_URL,
                SHORT_NAME,
                URL,
                mIcon,
                /* isIconMaskable= */ false);

        Notification notification = mShadowNotificationManager.getAllNotifications().get(0);

        Assert.assertNotNull(notification);
        Assert.assertEquals(SHORT_NAME, notification.extras.getString(Notification.EXTRA_TITLE));
        Assert.assertEquals(
                ChromeChannelDefinitions.ChannelId.WEBAPPS, notification.getChannelId());
        Assert.assertEquals(
                mContext.getString(R.string.notification_webapk_installed),
                notification.extras.getString(Notification.EXTRA_TEXT));

        Bitmap largeIcon =
                ((BitmapDrawable) notification.getLargeIcon().loadDrawable(mContext)).getBitmap();
        Assert.assertTrue(mIcon.sameAs(largeIcon));

        Assert.assertEquals(R.drawable.ic_chrome, notification.getSmallIcon().getResId());

        Assert.assertNotNull(notification.contentIntent);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.WEB_APK_INSTALL_FAILURE_NOTIFICATION})
    public void testFailureNotification() {
        WebApkInstallService.showInstallFailedNotification(
                MANIFEST_URL,
                SHORT_NAME,
                URL,
                mIcon,
                /* isIconMaskable= */ false,
                WebApkInstallResult.FAILURE);

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

        Assert.assertEquals(R.drawable.ic_chrome, notification.getSmallIcon().getResId());

        Assert.assertNotNull(notification.contentIntent);

        Action[] actions = notification.actions;
        Assert.assertEquals(1, actions.length);
        Assert.assertEquals(
                mContext.getString(R.string.webapk_install_failed_action_open), actions[0].title);
        Assert.assertNotNull(actions[0].actionIntent);
    }

    @Test
    public void testCompleteNotification_desktopAutoLaunch() {
        DeviceInfo.setIsDesktopForTesting(true);

        WebApkInstallService.showInstalledNotificationAndMaybeLaunch(
                /* originatingTab= */ null,
                PACKAGE_NAME,
                MANIFEST_URL,
                SHORT_NAME,
                URL,
                mIcon,
                /* isIconMaskable= */ false);

        Notification notification = mShadowNotificationManager.getAllNotifications().get(0);
        Assert.assertNotNull(notification);
    }

    @Test
    public void testCompleteNotification_desktopAutoLaunch_withReparenting() {
        DeviceInfo.setIsDesktopForTesting(true);
        IntentHandler.setTestIntentsEnabled(true);

        final int tabId = 10;
        Tab mockTab = mock(Tab.class);
        UserDataHost userDataHost = new UserDataHost();
        when(mockTab.getUserDataHost()).thenReturn(userDataHost);
        when(mockTab.getId()).thenReturn(tabId);

        WebContents mockWebContents = mock(WebContents.class);
        when(mockTab.getWebContents()).thenReturn(mockWebContents);

        WebApkInstallService.showInstalledNotificationAndMaybeLaunch(
                mockTab,
                PACKAGE_NAME,
                MANIFEST_URL,
                SHORT_NAME,
                URL,
                mIcon,
                /* isIconMaskable= */ false);

        // Verify the WebAPK is launched via an Intent
        Intent launchedIntent = shadowOf((Application) mContext).getNextStartedActivity();
        Assert.assertNotNull(launchedIntent);

        // Verify that the launch Intent has the reparent token attached
        Assert.assertTrue(
                launchedIntent.hasExtra(WebApkReparentingHandler.EXTRA_WEBAPK_REPARENT_TOKEN));
        Assert.assertEquals(
                tabId,
                WebApkReparentingHandler.getInstance()
                        .detachAndRegisterTabAndClear(launchedIntent));

        AsyncTabParams params = AsyncTabParamsManagerSingleton.getInstance().remove(tabId);
        Assert.assertNotNull(params);
        Assert.assertEquals(mockTab, params.getTabToReparent());

        // Verify that the tab was detached from the window for reparenting
        verify(mockWebContents).setTopLevelNativeWindow(null);
        verify(mockTab).updateAttachment(null, null);
    }

    @Test
    public void testCompleteNotification_desktopAutoLaunch_startActivityFails_doesNotDetachTab() {
        DeviceInfo.setIsDesktopForTesting(true);
        IntentHandler.setTestIntentsEnabled(true);

        final int tabId = 10;
        Tab mockTab = mock(Tab.class);
        UserDataHost userDataHost = new UserDataHost();
        lenient().when(mockTab.getUserDataHost()).thenReturn(userDataHost);
        when(mockTab.getId()).thenReturn(tabId);

        WebContents mockWebContents = mock(WebContents.class);
        lenient().when(mockTab.getWebContents()).thenReturn(mockWebContents);

        Context spyContext = spy(mContext);
        doThrow(new ActivityNotFoundException("Activity not found"))
                .when(spyContext)
                .startActivity(any());
        ContextUtils.initApplicationContextForTests(spyContext);

        WebApkInstallService.showInstalledNotificationAndMaybeLaunch(
                mockTab,
                PACKAGE_NAME,
                MANIFEST_URL,
                SHORT_NAME,
                URL,
                mIcon,
                /* isIconMaskable= */ false);

        // Verify that the notification is still shown even if auto-launch fails
        Notification notification = mShadowNotificationManager.getAllNotifications().get(0);
        Assert.assertNotNull(notification);

        // Verify that the tab was NOT stored in AsyncTabParamsManager
        Assert.assertFalse(AsyncTabParamsManagerSingleton.getInstance().hasParamsForTabId(tabId));

        // Verify that the tab was NEVER detached
        verify(mockWebContents, never()).setTopLevelNativeWindow(any());
        verify(mockTab, never()).updateAttachment(any(), any());
    }
}
