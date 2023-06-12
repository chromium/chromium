// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotificationManager;
import org.robolectric.shadows.ShadowPendingIntent;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.components.webapps.WebApkInstallResult;

/**
 * Tests WebAPKs install notifications from {@link WebApkInstallService}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowNotificationManager.class, ShadowPendingIntent.class})
@Features.EnableFeatures({ChromeFeatureList.WEB_APK_INSTALL_FAILURE_NOTIFICATION,
        ChromeFeatureList.WEB_APK_INSTALL_RETRY})
public class WebApkInstallBroadcastReceiverTest {
    private static final String MANIFEST_URL = "https://test.com/manifest.json";
    private static final String SHORT_NAME = "webapk";
    private static final String URL = "https://test.com";
    private final Bitmap mIcon = Bitmap.createBitmap(1, 1, Bitmap.Config.ALPHA_8);
    private final byte[] mSerializedProto = new byte[] {1, 2};

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private UrlFormatter.Natives mUrlFormatterJniMock;

    private Context mContext;
    private ShadowNotificationManager mShadowNotificationManager;

    @Mock
    public WebApkInstallCoordinatorBridge mBridge;
    @Mock
    private Context mContextMock;

    private WebApkInstallBroadcastReceiver mReceiver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UrlFormatterJni.TEST_HOOKS, mUrlFormatterJniMock);
        when(mUrlFormatterJniMock.formatStringUrlForSecurityDisplay(
                     anyString(), eq(SchemeDisplay.OMIT_HTTP_AND_HTTPS)))
                .then(inv -> inv.getArgument(0));

        mContext = spy(RuntimeEnvironment.application);
        ContextUtils.initApplicationContextForTests(mContext);

        mReceiver = new WebApkInstallBroadcastReceiver(mBridge);

        mShadowNotificationManager = shadowOf(
                (NotificationManager) mContext.getSystemService(Context.NOTIFICATION_SERVICE));
        WebApkInstallService.showInstallFailedNotification(MANIFEST_URL, SHORT_NAME, URL, mIcon,
                false /* isIconMaskable */, WebApkInstallResult.FAILURE, mSerializedProto);
    }

    private Intent createActionIntent(String action) {
        PendingIntentProvider provider = WebApkInstallBroadcastReceiver.createPendingIntent(
                mContext, MANIFEST_URL, URL, action, mSerializedProto);
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

    @Test
    public void testRetryInstallAction() {
        Intent intent = createActionIntent(WebApkInstallBroadcastReceiver.ACTION_RETRY_INSTALL);

        mReceiver.onReceive(mContext, intent);

        Assert.assertEquals(0, mShadowNotificationManager.getAllNotifications().size());
        verify(mBridge).retry(any(), any(), any());
    }

    @Test(expected = AssertionError.class)
    public void testRetryInstallActionWithoutProto() {
        Intent intent = createActionIntent(WebApkInstallBroadcastReceiver.ACTION_RETRY_INSTALL);
        intent.putExtra(WebApkInstallBroadcastReceiver.RETRY_PROTO, (byte[]) null);

        mReceiver.onReceive(mContext, intent);

        Assert.assertEquals(0, mShadowNotificationManager.getAllNotifications().size());
        // Verify it opens the startUrl when no valid proto to retry.
        verify(mContext).startActivity(notNull());
        verify(mBridge, never()).retry(any(), any(), any());
    }
}
