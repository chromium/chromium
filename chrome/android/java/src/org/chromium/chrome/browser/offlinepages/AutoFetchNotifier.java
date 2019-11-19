// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;
import android.support.v4.app.NotificationCompat;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.CachedMetrics.EnumeratedHistogramSample;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.TabOpenType;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.notifications.ChromeNotification;
import org.chromium.chrome.browser.notifications.ChromeNotificationBuilder;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.NotificationManagerProxy;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;
import org.chromium.chrome.browser.notifications.NotificationMetadata;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.PendingIntentProvider;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.content_public.browser.LoadUrlParams;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Implements notifications when pages are automatically fetched after reaching the net-error page.
 */
@JNINamespace("offline_pages")
public class AutoFetchNotifier {
    private static final String TAG = "AutoFetchNotifier";
    private static final String COMPLETE_NOTIFICATION_TAG = "OfflinePageAutoFetchNotification";
    private static final String IN_PROGRESS_NOTIFICATION_TAG =
            "OfflinePageAutoFetchInProgressNotification";
    private static final String EXTRA_URL = "org.chromium.chrome.browser.offlinepages.URL";
    private static final String EXTRA_ACTION = "notification_action";

    // Name of an application preference variable used to track whether or not the in-progress
    // notification is being shown. This is an alternative to
    // NotificationManager.getActiveNotifications, which isn't available prior to API level 23.
    private static final String PREF_SHOWING_IN_PROGRESS = "offline_auto_fetch_showing_in_progress";
    // The application preference variable which is set to the NotificationAction that triggered the
    // cancellation, when a cancellation is requested by the user.
    private static final String PREF_USER_CANCEL_ACTION_IN_PROGRESS =
            "offline_auto_fetch_user_cancel_action_in_progress";

    @VisibleForTesting
    public static TestHooks mTestHooks;

    /**
     * Interface for testing.
     */
    @VisibleForTesting
    public static interface TestHooks {
        public void inProgressNotificationShown(Intent cancelButtonIntent, Intent deleteIntent);
        public void completeNotificationShown(Intent clickIntent, Intent deleteIntent);
    }

    /*
     * A list of notification actions logged to UMA. To add a value to this list, update
     * OfflinePagesAutoFetchNotificationAction in enums.xml and make sure to keep this list in sync.
     * Additions should be treated as APPEND ONLY to keep the UMA metric semantics the same over
     * time.
     */
    @IntDef({NotificationAction.SHOWN, NotificationAction.COMPLETE,
            NotificationAction.CANCEL_PRESSED, NotificationAction.DISMISSED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface NotificationAction {
        int SHOWN = 0;
        int COMPLETE = 1;
        int CANCEL_PRESSED = 2;
        int DISMISSED = 3;
        int TAPPED = 4;

        int NUM_ENTRIES = 5;
    }

    /**
     * Dismisses the in-progress notification and cancels request, triggered when the notification
     * is swiped away or the cancel button is tapped.
     */
    public static class InProgressCancelReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(final Context context, Intent intent) {
            // Error check the action stored in the intent. Ignore the intent if it looks invalid.
            @NotificationAction
            int action = IntentUtils.safeGetIntExtra(
                    intent, EXTRA_ACTION, NotificationAction.NUM_ENTRIES);
            if (action != NotificationAction.CANCEL_PRESSED
                    && action != NotificationAction.DISMISSED) {
                return;
            }

            // Chrome may or may not be running. Use runNowOrAfterNativeInitialization() to trigger
            // the cancellation if Chrome is running. If Chrome isn't running,
            // runNowOrAfterNativeInitialization() will never call our runnable, so set a pref to
            // remember to cancel on next startup.
            ContextUtils.getAppSharedPreferences()
                    .edit()
                    .putInt(PREF_USER_CANCEL_ACTION_IN_PROGRESS, action)
                    .apply();
            // This will call us back with cancellationComplete().
            ChromeBrowserInitializer.getInstance(context).runNowOrAfterNativeInitialization(
                    AutoFetchNotifier::cancelInProgress);
            // Finally, whether chrome is running or not, remove the notification.
            closeInProgressNotification();
        }
    }

    // Called by native when the number of in-progress requests changes.
    @CalledByNative
    private static void updateInProgressNotificationCountIfShowing(int inProgressCount) {
        if (inProgressCount == 0) {
            Context context = ContextUtils.getApplicationContext();
            // Note: we're not fully trusting the result of isShowingInProgressNotification(). It's
            // possible that the prefs-based value is out of sync with the system notification, in
            // which case we still try to remove the notification even if we think it's not there.
            if (isShowingInProgressNotification()) {
                reportInProgressNotificationAction(NotificationAction.COMPLETE);
            }
            closeInProgressNotification();
            return;
        }

        if (isShowingInProgressNotification()) {
            // Since the notification is already showing, don't increment the notification action
            // UMA.
            showInProgressNotification(inProgressCount);
        }
    }

    @CalledByNative
    private static void showInProgressNotification(int inProgressCount) {
        Context context = ContextUtils.getApplicationContext();

        // Create intents for cancellation, both by pressing 'cancel', and swiping away.
        Intent cancelButtonIntent = new Intent(context, InProgressCancelReceiver.class);
        cancelButtonIntent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        cancelButtonIntent.putExtra(EXTRA_ACTION, NotificationAction.CANCEL_PRESSED);
        cancelButtonIntent.setPackage(context.getPackageName());

        Intent deleteIntent = new Intent(context, InProgressCancelReceiver.class);
        deleteIntent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        deleteIntent.putExtra(EXTRA_ACTION, NotificationAction.DISMISSED);
        deleteIntent.setPackage(context.getPackageName());

        String title = context.getResources().getQuantityString(
                R.plurals.offline_pages_auto_fetch_in_progress_notification_text, inProgressCount);

        // Create the notification.
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory
                        .createChromeNotificationBuilder(
                                true /* preferCompat */, ChannelDefinitions.ChannelId.DOWNLOADS)
                        .setContentTitle(title)
                        .setGroup(COMPLETE_NOTIFICATION_TAG)
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_LOW)
                        .setSmallIcon(R.drawable.ic_chrome)
                        .addAction(0 /* icon */, context.getString(R.string.cancel),
                                PendingIntent.getBroadcast(context, 0 /* requestCode */,
                                        cancelButtonIntent, 0 /* flags */))
                        .setDeleteIntent(PendingIntent.getBroadcast(
                                context, 0 /* requestCode */, deleteIntent, 0 /* flags */));

        NotificationManager manager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        Notification notification = builder.build();
        manager.notify(IN_PROGRESS_NOTIFICATION_TAG, 0, notification);
        NotificationUmaTracker.getInstance().onNotificationShown(
                NotificationUmaTracker.SystemNotificationType.OFFLINE_PAGES, notification);
        reportInProgressNotificationAction(NotificationAction.SHOWN);
        if (mTestHooks != null) {
            mTestHooks.inProgressNotificationShown(cancelButtonIntent, deleteIntent);
        }
    }

    public static void closeInProgressNotification() {
        NotificationManager manager =
                (NotificationManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.NOTIFICATION_SERVICE);
        manager.cancel(IN_PROGRESS_NOTIFICATION_TAG, 0);
        setIsShowingInProgressNotification(false);
    }

    // Called by native after all in-flight requests were canceled. This happens in response to the
    // user interacting with the in-progress notification.
    @CalledByNative
    private static void cancellationComplete() {
        @NotificationAction
        int currentAction = ContextUtils.getAppSharedPreferences().getInt(
                PREF_USER_CANCEL_ACTION_IN_PROGRESS, NotificationAction.NUM_ENTRIES);
        if (currentAction == NotificationAction.NUM_ENTRIES) {
            return;
        }
        reportInProgressNotificationAction(currentAction);
        ContextUtils.getAppSharedPreferences()
                .edit()
                .remove(PREF_USER_CANCEL_ACTION_IN_PROGRESS)
                .apply();
    }

    /**
     * Returns true if all auto-fetch requests should be canceled due to user interaction with the
     * in-progress notification.
     */
    @VisibleForTesting
    @CalledByNative
    public static boolean autoFetchInProgressNotificationCanceled() {
        return ContextUtils.getAppSharedPreferences().getInt(
                       PREF_USER_CANCEL_ACTION_IN_PROGRESS, NotificationAction.NUM_ENTRIES)
                != NotificationAction.NUM_ENTRIES;
    }

    /**
     * Handles interaction with the complete notification.
     */
    public static class CompleteNotificationReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(final Context context, Intent intent) {
            // Error check the action stored in the intent. Ignore the intent if it looks invalid.
            @NotificationAction
            int action = IntentUtils.safeGetIntExtra(
                    intent, EXTRA_ACTION, NotificationAction.NUM_ENTRIES);
            if (action != NotificationAction.TAPPED && action != NotificationAction.DISMISSED) {
                return;
            }

            reportCompleteNotificationAction(action);
            if (action != NotificationAction.TAPPED) {
                // If action == DISMISSED, the notification is already automatically removed.
                return;
            }

            // Create a new intent that will be handled by |ChromeTabbedActivity| to open the page.
            // This |BroadcastReceiver| is only required for collecting UMA.
            Intent viewIntent = new Intent(Intent.ACTION_VIEW,
                    Uri.parse(IntentUtils.safeGetStringExtra(intent, EXTRA_URL)));
            viewIntent.putExtras(intent);
            viewIntent.setPackage(context.getPackageName());
            viewIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            IntentHandler.startActivityForTrustedIntent(viewIntent);
        }
    }

    /**
     * Creates a system notification that informs the user when an auto-fetched page is ready.
     * If the notification is tapped, it opens the offline page in Chrome.
     *
     * @param pageTitle     The title of the page. This is displayed on the notification.
     * @param originalUrl   The requested URL before any redirection.
     * @param finalUrl      The requested URL after any redirection.
     * @param tabId         ID of the tab where the auto-fetch occurred. This tab is used, if
     *                      available, to open the offline page when the notification is tapped.
     * @param offlineId     The offlineID for the offline page that was just saved.
     */
    @CalledByNative
    private static void showCompleteNotification(
            String pageTitle, String originalUrl, String finalUrl, int tabId, long offlineId) {
        Context context = ContextUtils.getApplicationContext();
        OfflinePageUtils.getLoadUrlParamsForOpeningOfflineVersion(
                finalUrl, offlineId, LaunchLocation.NOTIFICATION, (params) -> {
                    showCompleteNotificationWithParams(
                            pageTitle, tabId, offlineId, originalUrl, finalUrl, params);
                });
    }

    private static void showCompleteNotificationWithParams(String pageTitle, int tabId,
            long offlineId, String originalUrl, String finalUrl, LoadUrlParams params) {
        Context context = ContextUtils.getApplicationContext();
        // Create an intent to handle tapping the notification.
        Intent clickIntent = new Intent(context, CompleteNotificationReceiver.class);
        // TODO(crbug.com/937581): We're using the final URL here so that redirects can't break
        // the page load. This will result in opening a new tab if there was a redirect (because
        // the URL doesn't match the old dino page), which is not ideal.
        clickIntent.putExtra(EXTRA_URL, finalUrl);
        clickIntent.putExtra(TabOpenType.REUSE_TAB_ORIGINAL_URL_STRING, originalUrl);
        IntentHandler.setIntentExtraHeaders(params.getExtraHeaders(), clickIntent);
        clickIntent.putExtra(TabOpenType.REUSE_TAB_MATCHING_ID_STRING, tabId);
        clickIntent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        clickIntent.putExtra(EXTRA_ACTION, NotificationAction.TAPPED);
        IntentHandler.setTabLaunchType(clickIntent, TabLaunchType.FROM_CHROME_UI);

        clickIntent.setPackage(context.getPackageName());

        PendingIntentProvider pendingClickIntent = PendingIntentProvider.getBroadcast(
                context, (int) offlineId /* requestCode */, clickIntent, 0 /* flags */);

        // Intent for swiping away.
        Intent deleteIntent = new Intent(context, CompleteNotificationReceiver.class);
        deleteIntent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        deleteIntent.putExtra(EXTRA_ACTION, NotificationAction.DISMISSED);
        deleteIntent.setPackage(context.getPackageName());

        // Create the notification.
        // Use the offline ID for a unique notification ID. Offline ID is a random
        // 64-bit integer. Truncating to 32 bits isn't ideal, but chances of collision
        // is still very low, and users should have few of these notifications
        // anyway.
        int notificationId = (int) offlineId;
        NotificationMetadata metadata = new NotificationMetadata(
                NotificationUmaTracker.SystemNotificationType.OFFLINE_PAGES,
                COMPLETE_NOTIFICATION_TAG, notificationId);
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory
                        .createChromeNotificationBuilder(true /* preferCompat */,
                                ChannelDefinitions.ChannelId.DOWNLOADS,
                                null /* remoteAppPackageName */, metadata)
                        .setAutoCancel(true)
                        .setContentIntent(pendingClickIntent)
                        .setContentTitle(pageTitle)
                        .setContentText(context.getString(
                                R.string.offline_pages_auto_fetch_ready_notification_text))
                        .setGroup(COMPLETE_NOTIFICATION_TAG)
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_LOW)
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setDeleteIntent(PendingIntentProvider.getBroadcast(
                                context, 0 /* requestCode */, deleteIntent, 0 /* flags */));

        ChromeNotification notification = builder.buildChromeNotification();
        NotificationManagerProxy manager = new NotificationManagerProxyImpl(context);
        manager.notify(notification);
        NotificationUmaTracker.getInstance().onNotificationShown(
                NotificationUmaTracker.SystemNotificationType.OFFLINE_PAGES,
                notification.getNotification());
        reportCompleteNotificationAction(NotificationAction.SHOWN);
        if (mTestHooks != null) {
            mTestHooks.completeNotificationShown(clickIntent, deleteIntent);
        }
    }

    private static void reportInProgressNotificationAction(@NotificationAction int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "OfflinePages.AutoFetch.InProgressNotificationAction", action,
                NotificationAction.NUM_ENTRIES);
    }

    private static void reportCompleteNotificationAction(@NotificationAction int action) {
        // Native may or may not be running, so use CachedMetrics.EnumeratedHistogramSample.
        EnumeratedHistogramSample sample =
                new EnumeratedHistogramSample("OfflinePages.AutoFetch.CompleteNotificationAction",
                        NotificationAction.NUM_ENTRIES);
        sample.record(action);
    }

    private static boolean isShowingInProgressNotification() {
        return ContextUtils.getAppSharedPreferences().getBoolean(PREF_SHOWING_IN_PROGRESS, false);
    }

    private static void setIsShowingInProgressNotification(boolean showing) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(PREF_SHOWING_IN_PROGRESS, showing)
                .apply();
    }

    private static void cancelInProgress() {
        AutoFetchNotifierJni.get().cancelInProgress(Profile.getLastUsedProfile());
    }

    @NativeMethods
    interface Natives {
        void cancelInProgress(Profile profile);
    }
}
