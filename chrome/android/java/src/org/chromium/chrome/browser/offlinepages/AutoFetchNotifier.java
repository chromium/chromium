// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.app.NotificationManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.core.app.NotificationCompat;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.TabOpenType;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
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

    @VisibleForTesting public static TestHooks mTestHooks;

    /** Interface for testing. */
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
    @IntDef({
        NotificationAction.SHOWN,
        NotificationAction.COMPLETE,
        NotificationAction.CANCEL_PRESSED,
        NotificationAction.DISMISSED
    })
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
            int action =
                    IntentUtils.safeGetIntExtra(
                            intent, EXTRA_ACTION, NotificationAction.NUM_ENTRIES);
            if (action != NotificationAction.CANCEL_PRESSED
                    && action != NotificationAction.DISMISSED) {
                return;
            }

            // Chrome may or may not be running. Use runNowOrAfterFullBrowserStarted() to trigger
            // the cancellation if Chrome is running in full browser. If Chrome isn't running in
            // full browser, runNowOrAfterFullBrowserStarted() will never call our runnable, so set
            // a pref to remember to cancel on next startup.
            ChromeSharedPreferences.getInstance()
                    .writeInt(
                            ChromePreferenceKeys.OFFLINE_AUTO_FETCH_USER_CANCEL_ACTION_IN_PROGRESS,
                            action);
            // This will call us back with cancellationComplete().
            ChromeBrowserInitializer.getInstance()
                    .runNowOrAfterFullBrowserStarted(AutoFetchNotifier::cancelInProgress);
            // Finally, whether chrome is running or not, remove the notification.
            closeInProgressNotification();
        }
    }

    // Called by native when the number of in-progress requests changes.
    @CalledByNative
    private static void updateInProgressNotificationCountIfShowing(int inProgressCount) {
        if (inProgressCount == 0) {
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

        String title =
                context.getResources()
                        .getQuantityString(
                                R.plurals.offline_pages_auto_fetch_in_progress_notification_text,
                                inProgressCount);

        // Create the notification.
        NotificationMetadata metadata =
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.OFFLINE_PAGES,
                        IN_PROGRESS_NOTIFICATION_TAG,
                        /* notificationId= */ 0);
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                                ChromeChannelDefinitions.ChannelId.DOWNLOADS, metadata)
                        .setContentTitle(title)
                        .setGroup(COMPLETE_NOTIFICATION_TAG)
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_LOW)
                        .setSmallIcon(R.drawable.ic_chrome)
                        .addAction(
                                /* icon= */ 0,
                                context.getString(R.string.cancel),
                                PendingIntentProvider.getBroadcast(
                                        context,
                                        /* requestCode= */ 0,
                                        cancelButtonIntent,
                                        /* flags= */ 0),
                                NotificationUmaTracker.ActionType.AUTO_FETCH_CANCEL)
                        .setDeleteIntent(
                                PendingIntentProvider.getBroadcast(
                                        context,
                                        /* requestCode= */ 0,
                                        deleteIntent,
                                        /* flags= */ 0));

        BaseNotificationManagerProxy manager = BaseNotificationManagerProxyFactory.create(context);
        NotificationWrapper notification = builder.buildNotificationWrapper();
        manager.notify(notification);
        NotificationUmaTracker.getInstance()
                .onNotificationShown(
                        NotificationUmaTracker.SystemNotificationType.OFFLINE_PAGES,
                        notification.getNotification());
        if (mTestHooks != null) {
            mTestHooks.inProgressNotificationShown(cancelButtonIntent, deleteIntent);
        }
    }

    public static void closeInProgressNotification() {
        NotificationManager manager =
                (NotificationManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.NOTIFICATION_SERVICE);
        manager.cancel(IN_PROGRESS_NOTIFICATION_TAG, 0);
        setIsShowingInProgressNotification(false);
    }

    // Called by native after all in-flight requests were canceled. This happens in response to the
    // user interacting with the in-progress notification.
    @CalledByNative
    private static void cancellationComplete() {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        @NotificationAction
        int currentAction =
                prefs.readInt(
                        ChromePreferenceKeys.OFFLINE_AUTO_FETCH_USER_CANCEL_ACTION_IN_PROGRESS,
                        NotificationAction.NUM_ENTRIES);
        if (currentAction == NotificationAction.NUM_ENTRIES) {
            return;
        }
        prefs.removeKey(ChromePreferenceKeys.OFFLINE_AUTO_FETCH_USER_CANCEL_ACTION_IN_PROGRESS);
    }

    /**
     * Returns true if all auto-fetch requests should be canceled due to user interaction with the
     * in-progress notification.
     */
    @VisibleForTesting
    @CalledByNative
    public static boolean autoFetchInProgressNotificationCanceled() {
        return ChromeSharedPreferences.getInstance()
                        .readInt(
                                ChromePreferenceKeys
                                        .OFFLINE_AUTO_FETCH_USER_CANCEL_ACTION_IN_PROGRESS,
                                NotificationAction.NUM_ENTRIES)
                != NotificationAction.NUM_ENTRIES;
    }

    /** Handles interaction with the complete notification. */
    public static class CompleteNotificationReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(final Context context, Intent intent) {
            // Error check the action stored in the intent. Ignore the intent if it looks invalid.
            @NotificationAction
            int action =
                    IntentUtils.safeGetIntExtra(
                            intent, EXTRA_ACTION, NotificationAction.NUM_ENTRIES);
            if (action != NotificationAction.TAPPED && action != NotificationAction.DISMISSED) {
                return;
            }
            if (action != NotificationAction.TAPPED) {
                // If action == DISMISSED, the notification is already automatically removed.
                return;
            }

            // Create a new intent that will be handled by |ChromeTabbedActivity| to open the page.
            // This |BroadcastReceiver| is only required for collecting UMA.
            Intent viewIntent =
                    new Intent(
                            Intent.ACTION_VIEW,
                            Uri.parse(IntentUtils.safeGetStringExtra(intent, EXTRA_URL)));
            viewIntent.putExtras(intent);
            viewIntent.setPackage(context.getPackageName());
            viewIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            IntentHandler.startActivityForTrustedIntent(viewIntent);
        }
    }

    /**
     * Creates a system notification that informs the user when an auto-fetched page is ready. If
     * the notification is tapped, it opens the offline page in Chrome.
     *
     * @param pageTitle The title of the page. This is displayed on the notification.
     * @param originalUrl The requested URL before any redirection.
     * @param finalUrl The requested URL after any redirection.
     * @param tabId ID of the tab where the auto-fetch occurred. This tab is used, if available, to
     *     open the offline page when the notification is tapped.
     * @param offlineId The offlineID for the offline page that was just saved.
     */
    @CalledByNative
    private static void showCompleteNotification(
            @JniType("std::u16string") String pageTitle,
            @JniType("std::string") String originalUrl,
            @JniType("std::string") String finalUrl,
            int tabId,
            long offlineId) {
        // Since offline pages are only available in regular mode, any downloaded content should be
        // triggered by regular mode. Hence, it is correct to pass always regular profile.
        OfflinePageUtils.getLoadUrlParamsForOpeningOfflineVersion(
                finalUrl,
                offlineId,
                LaunchLocation.NOTIFICATION,
                (params) -> {
                    showCompleteNotificationWithParams(
                            pageTitle, tabId, offlineId, originalUrl, finalUrl, params);
                },
                ProfileManager.getLastUsedRegularProfile());
    }

    private static void showCompleteNotificationWithParams(
            String pageTitle,
            int tabId,
            long offlineId,
            String originalUrl,
            String finalUrl,
            LoadUrlParams params) {
        Context context = ContextUtils.getApplicationContext();
        // Create an intent to handle tapping the notification.
        Intent clickIntent = new Intent(context, CompleteNotificationReceiver.class);
        // TODO(crbug.com/41444557): We're using the final URL here so that redirects can't break
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

        PendingIntentProvider pendingClickIntent =
                PendingIntentProvider.getBroadcast(
                        context, (int) /* requestCode= */ offlineId, clickIntent, /* flags= */ 0);

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
        NotificationMetadata metadata =
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.OFFLINE_PAGES,
                        COMPLETE_NOTIFICATION_TAG,
                        notificationId);
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                                ChromeChannelDefinitions.ChannelId.DOWNLOADS, metadata)
                        .setAutoCancel(true)
                        .setContentIntent(pendingClickIntent)
                        .setContentTitle(pageTitle)
                        .setContentText(
                                context.getString(
                                        R.string.offline_pages_auto_fetch_ready_notification_text))
                        .setGroup(COMPLETE_NOTIFICATION_TAG)
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_LOW)
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setDeleteIntent(
                                PendingIntentProvider.getBroadcast(
                                        context,
                                        /* requestCode= */ 0,
                                        deleteIntent,
                                        /* flags= */ 0));

        NotificationWrapper notification = builder.buildNotificationWrapper();
        BaseNotificationManagerProxy manager = BaseNotificationManagerProxyFactory.create(context);
        manager.notify(notification);
        NotificationUmaTracker.getInstance()
                .onNotificationShown(
                        NotificationUmaTracker.SystemNotificationType.OFFLINE_PAGES,
                        notification.getNotification());
        if (mTestHooks != null) {
            mTestHooks.completeNotificationShown(clickIntent, deleteIntent);
        }
    }

    private static boolean isShowingInProgressNotification() {
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.OFFLINE_AUTO_FETCH_SHOWING_IN_PROGRESS, false);
    }

    private static void setIsShowingInProgressNotification(boolean showing) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.OFFLINE_AUTO_FETCH_SHOWING_IN_PROGRESS, showing);
    }

    private static void cancelInProgress() {
        // Using regular profile here, since this function is only called in regular mode.
        AutoFetchNotifierJni.get().cancelInProgress(ProfileManager.getLastUsedRegularProfile());
    }

    @NativeMethods
    interface Natives {
        void cancelInProgress(@JniType("Profile*") Profile profile);
    }
}
