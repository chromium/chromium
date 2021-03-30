// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Build;

import androidx.core.app.NotificationCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadOpenSource;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.settings.NotificationSettings;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.BrowserStartupController.StartupCallback;

/**
 * Helper that can notify about prefetched pages and also receive the click events.
 */
public class PrefetchedPagesNotifier {
    private static final String NOTIFICATION_TAG = "OfflineContentSuggestionsNotification";

    // UMA histogram values tracking user actions on the prefetch histogram. Because this enum is
    // used to back an UMA histogram, it should be treated as append-only.
    public static final int NOTIFICATION_ACTION_MAY_SHOW = 0;
    public static final int NOTIFICATION_ACTION_SHOWN = 1;
    public static final int NOTIFICATION_ACTION_CLICKED = 2;
    public static final int NOTIFICATION_ACTION_SETTINGS_CLICKED = 3;

    public static final int NOTIFICATION_ACTION_COUNT = 4;

    private static PrefetchedPagesNotifier sInstance;

    public static PrefetchedPagesNotifier getInstance() {
        if (sInstance == null) sInstance = new PrefetchedPagesNotifier();
        return sInstance;
    }

    static void setInstanceForTest(PrefetchedPagesNotifier notifier) {
        sInstance = notifier;
    }

    /**
     * Opens the Downloads Home when the notification is tapped.
     */
    public static class ClickReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(final Context context, Intent intent) {
            PrefetchPrefs.setIgnoredNotificationCounter(0);
            recordNotificationActionWhenChromeLoadsNative(NOTIFICATION_ACTION_CLICKED);

            // TODO(dewittj): Handle the case where we somehow get this broadcast but the Chrome
            // download manager is unavailable.  Today, if this happens then the Android download
            // manager will be launched, and that will not contain any prefetched content.
            DownloadUtils.showDownloadManager(null, null, null,
                    DownloadOpenSource.OFFLINE_CONTENT_NOTIFICATION,
                    true /*showPrefetchedContent*/);
        }
    }

    /**
     * Opens notification settings when the settings button is tapped.  This should never be called
     * on O+ since notification settings are provided by the OS.
     */
    public static class SettingsReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(final Context context, Intent intent) {
            recordNotificationActionWhenChromeLoadsNative(NOTIFICATION_ACTION_SETTINGS_CLICKED);
            SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
            Intent settingsIntent = settingsLauncher.createSettingsActivityIntent(
                    context, NotificationSettings.class.getName());
            settingsIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK);
            context.startActivity(settingsIntent);
        }
    }

    /**
     * Shows the prefetching notification.
     *
     * This requires the native library to have already been loaded. Depending on the invocation,
     * this can be called from Java or Native.
     *
     * @param origin A string representing the origin of a relevant prefetched page.
     */
    @CalledByNative
    static void showDebuggingNotification(String origin) {
        getInstance().showNotification(origin);
    }

    void showNotification(String origin) {
        Context context = ContextUtils.getApplicationContext();

        // TODO(dewittj): Use unique notification IDs, allowing multiple to appear in the
        // notification center.
        // TODO(xingliu): Unify the notification logic in here and AutoFetchNotifier.
        int notificationId = 1;
        PendingIntentProvider clickIntent = getPendingBroadcastFor(context, ClickReceiver.class);
        String title =
                String.format(context.getString(R.string.offline_pages_prefetch_notification_title),
                        context.getString(R.string.app_name));
        String text = String.format(
                context.getString(R.string.offline_pages_prefetch_notification_text), origin);

        NotificationMetadata metadata = new NotificationMetadata(
                NotificationUmaTracker.SystemNotificationType.OFFLINE_CONTENT_SUGGESTION,
                NOTIFICATION_TAG, notificationId);
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory
                        .createNotificationWrapperBuilder(true /* preferCompat */,
                                ChromeChannelDefinitions.ChannelId.CONTENT_SUGGESTIONS,
                                null /* remoteAppPackageName */, metadata)
                        .setAutoCancel(true)
                        .setContentIntent(clickIntent)
                        .setContentTitle(title)
                        .setContentText(text)
                        .setGroup(NOTIFICATION_TAG)
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_LOW)
                        .setSmallIcon(R.drawable.ic_chrome);
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            PendingIntentProvider settingsIntent =
                    getPendingBroadcastFor(context, SettingsReceiver.class);
            builder.addAction(R.drawable.settings_cog, context.getString(R.string.settings),
                    settingsIntent,
                    NotificationUmaTracker.ActionType.OFFLINE_CONTENT_SUGGESTION_SETTINGS);
        }

        NotificationWrapper notification = builder.buildNotificationWrapper();

        NotificationManagerProxy manager = new NotificationManagerProxyImpl(context);
        manager.notify(notification);

        // Increment ignored notification counter.  This will be reset on click.
        PrefetchPrefs.incrementIgnoredNotificationCounter();

        // Metrics tracking
        recordNotificationAction(NOTIFICATION_ACTION_SHOWN);
        NotificationUmaTracker.getInstance().onNotificationShown(
                NotificationUmaTracker.SystemNotificationType.OFFLINE_CONTENT_SUGGESTION,
                notification.getNotification());
    }

    private static PendingIntentProvider getPendingBroadcastFor(Context context, Class clazz) {
        return PendingIntentProvider.getBroadcast(
                context, 0 /* requestCode */, new Intent(context, clazz), 0 /* flags */);
    }

    /**
     * Records a histogram when Chrome loads native.
     * Note: Does not itself load native so this will do nothing if nothing else does.
     */
    private static void recordNotificationActionWhenChromeLoadsNative(final int action) {
        runWhenChromeLoadsNative(() -> getInstance().recordNotificationAction(action));
    }

    /**
     * Records a prefetching notification action histogram now.
     * @param action One of the PrefetchedPagesNotifier.NOTIFICATION_ACTION_* action values.
     */
    void recordNotificationAction(int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "OfflinePages.Prefetching.NotificationAction", action, NOTIFICATION_ACTION_COUNT);
    }

    /**
     * Executes a Runnable when Chrome's native library loads.
     * Does not itself load native.
     */
    private static void runWhenChromeLoadsNative(final Runnable r) {
        BrowserStartupController browserStartup = BrowserStartupController.getInstance();
        if (!browserStartup.isFullBrowserStarted()) {
            browserStartup.addStartupCompletedObserver(new StartupCallback() {
                @Override
                public void onSuccess() {
                    r.run();
                }
                @Override
                public void onFailure() {}
            });
            return;
        }

        r.run();
    }
}
