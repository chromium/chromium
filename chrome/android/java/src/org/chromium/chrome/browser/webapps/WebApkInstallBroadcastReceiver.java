// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.net.Uri;
import android.service.notification.StatusBarNotification;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;

/**
 * Triggered when user interaction with WebAPK install notifications.
 *
 * Lifecycle: This BroadcastReceiver is started when an Intent arrives, performs a small amount of
 * work (dismissing a notification and post task to retry the install) and is destroyed.
 *
 * Thread safety: {@link #onReceive} is called on the main thread by the Android framework.
 */
public class WebApkInstallBroadcastReceiver extends BroadcastReceiver {
    private static final String TAG = "webapk";

    static final String ACTION_RETRY_INSTALL = "WebApkInstallNotification.retry";
    static final String ACTION_OPEN_IN_BROWSER = "WebApkInstallNotification.open";

    private static final String NOTIFICATION_ID = "WebApkInstallNotification.notification_id";
    private static final String WEBAPK_START_URL = "WebApkInstallNotification.start_url";
    @VisibleForTesting
    static final String RETRY_PROTO = "WebApkInstallNotification.retry_proto";

    private WebApkInstallCoordinatorBridge mBridge;

    /** Constructor used by the Android framework. */
    public WebApkInstallBroadcastReceiver() {}

    /** Constructor that allows dependency injection for use in tests. */
    @VisibleForTesting
    public WebApkInstallBroadcastReceiver(WebApkInstallCoordinatorBridge bridge) {
        mBridge = bridge;
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        assert intent != null && intent.hasExtra(NOTIFICATION_ID);

        String id = IntentUtils.safeGetStringExtra(intent, NOTIFICATION_ID);
        String startUrl = IntentUtils.safeGetStringExtra(intent, WEBAPK_START_URL);

        byte[] proto = IntentUtils.safeGetByteArrayExtra(intent, RETRY_PROTO);
        Bitmap icon = getCurrentIconFromNotification(context, id);

        WebApkInstallService.cancelNotification(id);

        if (ACTION_RETRY_INSTALL.equals(intent.getAction())) {
            if (icon != null && proto != null && proto.length != 0) {
                retryInstall(id, proto, icon);
            } else {
                assert false : "Invalid intent data " + id;
                openInChrome(context, startUrl);
            }
        } else if (ACTION_OPEN_IN_BROWSER.equals(intent.getAction())) {
            openInChrome(context, startUrl);
        }
    }

    // Get the Bitmap icon from the current install notification, it will be use for the retry
    // install notification.
    @Nullable
    private Bitmap getCurrentIconFromNotification(Context context, String id) {
        NotificationManager nm =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        for (StatusBarNotification sbn : nm.getActiveNotifications()) {
            if ((WebApkInstallService.WEBAPK_INSTALL_NOTIFICATION_TAG_PREFIX + id)
                            .equals(sbn.getTag())) {
                return ((BitmapDrawable) sbn.getNotification().getLargeIcon().loadDrawable(context))
                        .getBitmap();
            }
        }
        return null;
    }

    static PendingIntentProvider createPendingIntent(Context context, String notificationId,
            String url, String action, byte[] serializedProto) {
        Intent intent = new Intent(action);
        intent.setClass(context, WebApkInstallBroadcastReceiver.class);
        intent.putExtra(NOTIFICATION_ID, notificationId);
        intent.putExtra(WEBAPK_START_URL, url);
        intent.putExtra(RETRY_PROTO, serializedProto);

        int requestCode = 0;
        int flags = PendingIntent.FLAG_UPDATE_CURRENT;
        return PendingIntentProvider.getBroadcast(context, requestCode, intent, flags);
    }

    private void openInChrome(Context context, String url) {
        Intent chromeIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        chromeIntent.setPackage(context.getPackageName());
        chromeIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        chromeIntent.putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        context.startActivity(chromeIntent);
    }

    private void retryInstall(String id, byte[] proto, Bitmap icon) {
        if (mBridge == null) {
            ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
            mBridge = new WebApkInstallCoordinatorBridge();
        }
        mBridge.retry(id, proto, icon);
    }
}