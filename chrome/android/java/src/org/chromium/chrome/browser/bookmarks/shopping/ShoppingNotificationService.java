// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.shopping;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.provider.Browser;
import android.text.TextUtils;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.url.GURL;

import androidx.annotation.Nullable;

public class ShoppingNotificationService {

    public static final long NOTIFICATION_INTERVAL = 1000 * 60 * 60 * 24;

    private static final String ACTION_OPEN_TAB = "shopping.open_tab";
    private static final String ACTION_DISMISS_NOTIFICATION = "shopping.dismiss_notification";

    private static final String EXTRA_GUID = "shopping.guid";
    public static final String EXTRA_PRODUCT_BOOKMARK_ID = "shopping.product.bookmark_id";

    public static class TapReceiver extends BroadcastReceiver {

        @Override
        public void onReceive(Context context, Intent intent) {
            handleIntent(intent);
        }
    }

    public static class ShoppingSendNotificationTask implements BackgroundTask {

        public ShoppingSendNotificationTask() {
        }

        @Override
        public boolean onStartTask(Context context, TaskParameters taskParameters,
                TaskFinishedCallback callback) {
            try {
                JSONObject notificationJson =
                        new JSONObject(SharedPreferencesManager.getInstance().readString(
                                ChromePreferenceKeys.SHOPPING_CURRENT_NOTIFICATION, ""));
                ShoppingNotificationService.getInstance().showNotification(
                        notificationJson.getInt("id"), notificationJson.getString("title"),
                        notificationJson.getString("subtitle"), notificationJson.getString("url"),
                        notificationJson.getString("bookmark_id"));
                SharedPreferencesManager.getInstance().writeString(
                        ChromePreferenceKeys.SHOPPING_CURRENT_NOTIFICATION, null);
            } catch (JSONException e) {
                // noop
            }
            return false;
        }

        @Override
        public boolean onStopTask(Context context, TaskParameters taskParameters) {
            return false;
        }

        @Override
        public void reschedule(Context context) {
        }
    }

    private static class LazyHolder {
        private static final ShoppingNotificationService INSTANCE
                = new ShoppingNotificationService();
    }

    private NotificationManagerProxy mNotificationManager;

    private ShoppingNotificationService() {
        mNotificationManager =
                new NotificationManagerProxyImpl(ContextUtils.getApplicationContext());
    }

    public static ShoppingNotificationService getInstance() {
        return LazyHolder.INSTANCE;
    }

    public void showNotification(
            int id, String title, String subtitle, String url, String bookmarkIdString) {
        Context context = ContextUtils.getApplicationContext();
        LargeIconBridge iconBridge = new LargeIconBridge(Profile.getLastUsedRegularProfile());

        GURL gurl = new GURL(url);
        iconBridge.getLargeIconForUrl(gurl, context.getResources().getDimensionPixelSize(
                R.dimen.shopping_notification_icon_size), new LargeIconBridge.LargeIconCallback() {
            @Override
            public void onLargeIconAvailable(@Nullable Bitmap icon, int fallbackColor,
                    boolean isFallbackColorDefault, int iconType) {
                showNotificationInternal(
                        context, id, title, subtitle, gurl, icon, bookmarkIdString);
            }
        });
    }

    private void showNotificationInternal(Context context, int id, String title, String subtitle,
            GURL url, Bitmap icon, String bookmarkIdString) {
        String channelId = ChromeChannelDefinitions.ChannelId.SHOPPING;
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(true, channelId,
                        null, new NotificationMetadata(
                                NotificationUmaTracker.SystemNotificationType.SHOPPING, null, id));
        builder.setLocalOnly(true);
        builder.setGroup(NotificationConstants.GROUP_SHOPPING);
        builder.setOngoing(false);

        builder.setLargeIcon(icon);
        builder.setSmallIcon(R.drawable.ic_shopping_bag_24dp);

        Intent contentIntent = new Intent(context, TapReceiver.class);
        contentIntent.setAction(ACTION_OPEN_TAB);
        contentIntent.setData(Uri.parse(url.getSpec()));
        contentIntent.putExtra(EXTRA_GUID, id);
        contentIntent.putExtra(EXTRA_PRODUCT_BOOKMARK_ID, bookmarkIdString);
        builder.setContentIntent(PendingIntentProvider.getBroadcast(
                context, 0, contentIntent, PendingIntent.FLAG_ONE_SHOT));

        Intent deleteIntent = new Intent(context, TapReceiver.class);
        deleteIntent.setAction(ACTION_DISMISS_NOTIFICATION);
        deleteIntent.setData(Uri.parse(url.getSpec()));
        deleteIntent.putExtra(EXTRA_GUID, id);
        builder.setDeleteIntent(PendingIntentProvider.getBroadcast(
                context, 0, deleteIntent, PendingIntent.FLAG_ONE_SHOT));

        mNotificationManager.notify(builder.buildWithBigTextStyle(title + "\n\n" + subtitle));
    }

    public static boolean hasPendingNotification() {
        try {
            String prefString = SharedPreferencesManager.getInstance().readString(
                    ChromePreferenceKeys.SHOPPING_CURRENT_NOTIFICATION, null);
            if (prefString == null) return false;
            JSONObject json = new JSONObject(prefString);
            return System.currentTimeMillis() - json.getLong("creationTime")
                    < NOTIFICATION_INTERVAL;

        } catch (JSONException e) {
            android.util.Log.w("mdjones", "PARSE ERROR: " + e.getMessage());
            return false;
        }
    }

    public static void handleIntent(Intent intent) {
        if (TextUtils.equals(ACTION_OPEN_TAB, intent.getAction())) {
            BookmarkId productId = null;
            if (intent.getStringExtra(EXTRA_PRODUCT_BOOKMARK_ID) != null) {
                productId = BookmarkId.getBookmarkIdFromString(
                        intent.getStringExtra(EXTRA_PRODUCT_BOOKMARK_ID));
            }
            BookmarkUtils.openToShoppingFolder(productId);
            //openUrl(intent.getDataString());
            hideNotification(intent.getExtras().getInt(EXTRA_GUID));
        } else if (TextUtils.equals(ACTION_DISMISS_NOTIFICATION, intent.getAction())) {
            android.util.Log.w("mdjones", "Shopping notification dismissed");
        }
    }

    private static void openUrl(String url) {
        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent()
                .setAction(Intent.ACTION_VIEW)
                .setData(Uri.parse(url))
                .setClass(context, ChromeLauncherActivity.class)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                .putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName())
                .putExtra(ShortcutHelper.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        IntentHandler.addTrustedIntentExtras(intent);
        context.startActivity(intent);
    }

    private static void hideNotification(int id) {
        Context context = ContextUtils.getApplicationContext();
        NotificationManagerProxy manager = new NotificationManagerProxyImpl(context);
        manager.cancel(id);
    }
}
