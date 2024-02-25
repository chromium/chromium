// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
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

    static final String ACTION_OPEN_IN_BROWSER = "WebApkInstallNotification.open";

    private static final String NOTIFICATION_ID = "WebApkInstallNotification.notification_id";
    private static final String WEBAPK_START_URL = "WebApkInstallNotification.start_url";

    /** Constructor used by the Android framework. */
    public WebApkInstallBroadcastReceiver() {}

    @Override
    public void onReceive(Context context, Intent intent) {
        assert intent != null && intent.hasExtra(NOTIFICATION_ID);

        String id = IntentUtils.safeGetStringExtra(intent, NOTIFICATION_ID);
        String startUrl = IntentUtils.safeGetStringExtra(intent, WEBAPK_START_URL);

        WebApkInstallService.cancelNotification(id);

        if (ACTION_OPEN_IN_BROWSER.equals(intent.getAction())) {
            openInChrome(context, startUrl);
        }
    }

    static PendingIntentProvider createPendingIntent(
            Context context, String notificationId, String url, String action) {
        Intent intent = new Intent(action);
        intent.setClass(context, WebApkInstallBroadcastReceiver.class);
        intent.putExtra(NOTIFICATION_ID, notificationId);
        intent.putExtra(WEBAPK_START_URL, url);

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
}
