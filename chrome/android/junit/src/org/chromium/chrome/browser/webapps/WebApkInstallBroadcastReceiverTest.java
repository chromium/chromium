// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotificationManager;
import org.robolectric.shadows.ShadowPendingIntent;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.webapps.WebApkInstallResult;

/** Tests WebAPKs install notifications from {@link WebApkInstallService}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowNotificationManager.class, ShadowPendingIntent.class})
@EnableFeatures({ChromeFeatureList.WEB_APK_INSTALL_FAILURE_NOTIFICATION})
public class WebApkInstallBroadcastReceiverTest {
    private static final String MANIFEST_URL = "https://test.com/manifest.json";
    private static final String SHORT_NAME = "webapk";
    private static final String URL = "https://test.com";
    private final Bitmap mIcon = Bitmap.createBitmap(1, 1, Bitmap.Config.ALPHA_8);
    private final byte[] mSerializedProto = new byte[] {1, 2};

    private Context mContext;
    private ShadowNotificationManager mShadowNotificationManager;

    @Mock private Context mContextMock;

    private WebApkInstallBroadcastReceiver mReceiver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mContext = spy(RuntimeEnvironment.application);
        ContextUtils.initApplicationContextForTests(mContext);

        mReceiver = new WebApkInstallBroadcastReceiver();

        mShadowNotificationManager =
                shadowOf(
                        (NotificationManager)
                                mContext.getSystemService(Context.NOTIFICATION_SERVICE));
        WebApkInstallService.showInstallFailedNotification(
                MANIFEST_URL,
                SHORT_NAME,
                URL,
                mIcon,
                /* isIconMaskable= */ false,
                WebApkInstallResult.FAILURE);
    }

    private Intent createActionIntent(String action) {
        PendingIntentProvider provider =
                WebApkInstallBroadcastReceiver.createPendingIntent(
                        mContext, MANIFEST_URL, URL, action);
        ShadowPendingIntent shadow = shadowOf(provider.getPendingIntent());
        Intent intent = shadow.getSavedIntents()[0];
        Assert.assertNotNull(intent);
        return intent;
    }

    @Test
    public void testOpenInChromeAction() {
        Intent intent = createActionIntent(WebApkInstallBroadcastReceiver.ACTION_OPEN_IN_BROWSER);

        mReceiver.onReceive(mContext, intent);

        Assert.assertEquals(0, mShadowNotificationManager.getAllNotifications().size());
        verify(mContext).startActivity(notNull());
    }
}
