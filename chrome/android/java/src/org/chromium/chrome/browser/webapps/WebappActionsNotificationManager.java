// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.support.v4.app.NotificationCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.widget.Toast;

/**
 * Manages the notification shown by Chrome when running standalone Web Apps. It accomplishes
 * number of goals:
 * - Presents the current URL.
 * - Exposes 'Share' and 'Open in Chrome' actions.
 * - Messages that Web App runs in Chrome.
 */
class WebappActionsNotificationManager {
    private static final String ACTION_SHARE =
            "org.chromium.chrome.browser.webapps.NOTIFICATION_ACTION_SHARE";
    private static final String ACTION_OPEN_IN_CHROME =
            "org.chromium.chrome.browser.webapps.NOTIFICATION_ACTION_OPEN_IN_CHROME";
    private static final String ACTION_FOCUS =
            "org.chromium.chrome.browser.webapps.NOTIFICATION_ACTION_FOCUS";

    static boolean isEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.PWA_PERSISTENT_NOTIFICATION);
    }

    private final WebappActivity mWebappActivity;

    public WebappActionsNotificationManager(WebappActivity webappActivity) {
        this.mWebappActivity = webappActivity;
    }

    public void maybeShowNotification() {
        if (!isEnabled() || mWebappActivity.getActivityTab() == null) return;

        // All features provided by the notification are also available in the minimal-ui toolbar.
        if (mWebappActivity.getWebappInfo().displayMode() == WebDisplayMode.MINIMAL_UI) {
            return;
        }

        Notification notification = createNotification();
        NotificationManager nm = (NotificationManager) mWebappActivity.getSystemService(
                Context.NOTIFICATION_SERVICE);
        nm.notify(NotificationConstants.NOTIFICATION_ID_WEBAPP_ACTIONS, notification);

        NotificationUmaTracker.getInstance().onNotificationShown(
                NotificationUmaTracker.SystemNotificationType.WEBAPP_ACTIONS, notification);
    }

    private Notification createNotification() {
        int intentFlags = Intent.FLAG_ACTIVITY_NEW_TASK
                | ApiCompatibilityUtils.getActivityNewDocumentFlag()
                | Intent.FLAG_ACTIVITY_CLEAR_TOP;
        int pendingIntentFlags = PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_ONE_SHOT;

        PendingIntent focusIntent = PendingIntent.getActivity(mWebappActivity, 0,
                new Intent(mWebappActivity, mWebappActivity.getClass())
                        .setAction(ACTION_FOCUS)
                        .setFlags(intentFlags),
                pendingIntentFlags);

        PendingIntent openInChromeIntent = PendingIntent.getActivity(mWebappActivity, 0,
                new Intent(mWebappActivity, mWebappActivity.getClass())
                        .setAction(ACTION_OPEN_IN_CHROME)
                        .setFlags(intentFlags),
                pendingIntentFlags);

        PendingIntent shareIntent = PendingIntent.getActivity(mWebappActivity, 0,
                new Intent(mWebappActivity, mWebappActivity.getClass())
                        .setAction(ACTION_SHARE)
                        .setFlags(intentFlags),
                pendingIntentFlags);

        return NotificationBuilderFactory
                .createChromeNotificationBuilder(
                        true /* prefer compat */, ChannelDefinitions.ChannelId.WEBAPP_ACTIONS)
                .setSmallIcon(R.drawable.ic_chrome)
                .setContentTitle(mWebappActivity.getWebappInfo().shortName())
                .setContentText(mWebappActivity.getString(R.string.webapp_tap_to_copy_url))
                .setShowWhen(false)
                .setAutoCancel(false)
                .setOngoing(true)
                .setPriorityBeforeO(NotificationCompat.PRIORITY_MIN)
                .setContentIntent(focusIntent)
                .addAction(R.drawable.ic_share_white_24dp,
                        mWebappActivity.getResources().getString(R.string.share), shareIntent)
                .addAction(R.drawable.ic_exit_to_app_white_24dp,
                        mWebappActivity.getResources().getString(R.string.menu_open_in_chrome),
                        openInChromeIntent)
                .build();
    }

    public void cancelNotification() {
        if (!isEnabled()) return;
        NotificationManager nm = (NotificationManager) mWebappActivity.getSystemService(
                Context.NOTIFICATION_SERVICE);
        nm.cancel(NotificationConstants.NOTIFICATION_ID_WEBAPP_ACTIONS);
    }

    public boolean handleNotificationAction(Intent intent) {
        if (ACTION_SHARE.equals(intent.getAction())) {
            // Not routing through onMenuOrKeyboardAction to control UMA String.
            mWebappActivity.onShareMenuItemSelected(
                    false /* share directly */, mWebappActivity.getCurrentTabModel().isIncognito());
            RecordUserAction.record("Webapp.NotificationShare");
            return true;
        } else if (ACTION_OPEN_IN_CHROME.equals(intent.getAction())) {
            mWebappActivity.onMenuOrKeyboardAction(R.id.open_in_browser_id, false /* fromMenu */);
            return true;
        } else if (ACTION_FOCUS.equals(intent.getAction())) {
            Tab tab = mWebappActivity.getActivityTab();
            if (tab != null) {
                ClipboardManager clipboard = (ClipboardManager) mWebappActivity.getSystemService(
                        Context.CLIPBOARD_SERVICE);
                clipboard.setPrimaryClip(ClipData.newPlainText("url", tab.getOriginalUrl()));
                Toast.makeText(mWebappActivity, R.string.url_copied, Toast.LENGTH_SHORT).show();
            }
            RecordUserAction.record("Webapp.NotificationFocused");
            return true;
        }
        return false;
    }
}
