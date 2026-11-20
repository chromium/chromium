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

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
import org.chromium.chrome.browser.profiles.ProfileTokenUtils;
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
@NullMarked
public class AutoFetchNotifier {
    private static final String TAG = "AutoFetchNotifier";
    private static final String COMPLETE_NOTIFICATION_TAG = "OfflinePageAutoFetchNotification";
    private static final String IN_PROGRESS_NOTIFICATION_TAG =
            "OfflinePageAutoFetchInProgressNotification";
    private static final String EXTRA_URL = "org.chromium.chrome.browser.offlinepages.URL";
    private static final String EXTRA_ACTION = "notification_action";

    private static @Nullable TestHooks sTestHooks;

    /** Interface for testing. */
    @VisibleForTesting
    public interface TestHooks {
        void inProgressNotificationShown(Intent cancelButtonIntent, Intent deleteIntent);

        void completeNotificationShown(Intent clickIntent, Intent deleteIntent);
    }

    /** Sets the test hooks and registers a {@link ResettersForTesting} callback to clear them. */
    public static void setTestHooksForTesting(TestHooks hooks) {
        sTestHooks = hooks;
        ResettersForTesting.register(() -> sTestHooks = null);
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
     * Dismisses the in-progress notification and cancels the requests of its profile, triggered
     * when the notification is swiped away or the cancel button is tapped.
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
            // TODO(crbug.com/587417706): Scope this cancellation pref per profile. Currently it is
            // app-wide and only consumed by the initial profile on next startup.
            ChromeSharedPreferences.getInstance()
                    .writeInt(
                            ChromePreferenceKeys.OFFLINE_AUTO_FETCH_USER_CANCEL_ACTION_IN_PROGRESS,
                            action);
            // This will call us back with cancellationComplete().
            ChromeBrowserInitializer.getInstance()
                    .runNowOrAfterFullBrowserStarted(() -> cancelInProgress(intent));
            // Finally, whether chrome is running or not, remove the notification.
            @Nullable String tag =
                    ProfileTokenUtils.getNotificationTagFromIntent(
                            IN_PROGRESS_NOTIFICATION_TAG, intent);
            // TODO(crbug.com/585922618): Remove this fallback in M163 once notifications posted
            // before notifications were per-profile have expired.
            closeInProgressNotification(tag != null ? tag : IN_PROGRESS_NOTIFICATION_TAG);
        }
    }

    private static String getInProgressNotificationTag(Profile profile) {
        return ProfileTokenUtils.getNotificationTag(IN_PROGRESS_NOTIFICATION_TAG, profile);
    }

    // Called by native when the number of in-progress requests of a profile changes.
    @VisibleForTesting
    @CalledByNative
    static void updateInProgressNotificationCountIfShowing(
            @JniType("Profile*") Profile profile, int inProgressCount) {
        if (inProgressCount == 0) {
            closeInProgressNotification(getInProgressNotificationTag(profile));
            if (profile.isInitialProfile()) {
                // TODO(crbug.com/585922618): Remove in M163 once notifications posted before
                // in-progress notifications were per-profile have expired. Only the initial
                // profile could have posted one.
                closeInProgressNotification(IN_PROGRESS_NOTIFICATION_TAG);
            }
            return;
        }

        if (isShowingInProgressNotification()) {
            // Since the notification is already showing, don't increment the notification action
            // UMA.
            showInProgressNotification(profile, inProgressCount);
        }
    }

    /**
     * Shows the in-progress notification of a profile. Each profile has its own notification,
     * tagged with {@link ProfileTokenUtils#getNotificationTag}.
     *
     * @param profile The profile whose auto-fetch requests are in progress.
     * @param inProgressCount The number of in-progress requests of {@code profile}.
     */
    @VisibleForTesting
    @CalledByNative
    static void showInProgressNotification(
            @JniType("Profile*") Profile profile, int inProgressCount) {
        Context context = ContextUtils.getApplicationContext();

        // Create intents for cancellation, both by pressing 'cancel', and swiping away.
        Intent cancelButtonIntent = new Intent(context, InProgressCancelReceiver.class);
        cancelButtonIntent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        cancelButtonIntent.putExtra(EXTRA_ACTION, NotificationAction.CANCEL_PRESSED);
        cancelButtonIntent.setPackage(context.getPackageName());
        ProfileTokenUtils.addProfileToIntent(profile, cancelButtonIntent);

        Intent deleteIntent = new Intent(context, InProgressCancelReceiver.class);
        deleteIntent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        deleteIntent.putExtra(EXTRA_ACTION, NotificationAction.DISMISSED);
        deleteIntent.setPackage(context.getPackageName());
        ProfileTokenUtils.addProfileToIntent(profile, deleteIntent);

        String title =
                context.getResources()
                        .getQuantityString(
                                R.plurals.offline_pages_auto_fetch_in_progress_notification_text,
                                inProgressCount);

        // Create the notification.
        NotificationMetadata metadata =
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.OFFLINE_PAGES,
                        getInProgressNotificationTag(profile),
                        /* notificationId= */ 0);
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                                ChromeChannelDefinitions.ChannelId.DOWNLOADS, metadata)
                        .setContentTitle(title)
                        .setGroup(COMPLETE_NOTIFICATION_TAG)
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

        BaseNotificationManagerProxy manager = BaseNotificationManagerProxyFactory.create();
        NotificationWrapper notification = builder.buildNotificationWrapper();
        manager.notify(notification);
        NotificationUmaTracker.getInstance()
                .onNotificationShown(
                        NotificationUmaTracker.SystemNotificationType.OFFLINE_PAGES,
                        notification.getNotification());
        if (sTestHooks != null) {
            sTestHooks.inProgressNotificationShown(cancelButtonIntent, deleteIntent);
        }
    }

    private static void closeInProgressNotification(String tag) {
        NotificationManager manager =
                (NotificationManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.NOTIFICATION_SERVICE);
        manager.cancel(tag, 0);
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
     * Returns true if all auto-fetch requests of {@code profile} should be canceled due to user
     * interaction with the in-progress notification while Chrome wasn't running.
     *
     * <p>The cancellation pref is app-wide. Before notifications were per-profile, only the initial
     * profile could post one, so only it consumes the pref.
     */
    // TODO(crbug.com/587417706): Honor cancellations of non-initial profiles made while Chrome
    // isn't running, via per-profile SharedPreferences.
    @VisibleForTesting
    @CalledByNative
    public static boolean autoFetchInProgressNotificationCanceled(
            @JniType("Profile*") Profile profile) {
        return profile.isInitialProfile()
                && ChromeSharedPreferences.getInstance()
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
     * @param profile The profile that auto-fetched the page.
     * @param pageTitle The title of the page. This is displayed on the notification.
     * @param originalUrl The requested URL before any redirection.
     * @param finalUrl The requested URL after any redirection.
     * @param tabId ID of the tab where the auto-fetch occurred. This tab is used, if available, to
     *     open the offline page when the notification is tapped.
     * @param offlineId The offlineID for the offline page that was just saved.
     */
    @VisibleForTesting
    @CalledByNative
    static void showCompleteNotification(
            @JniType("Profile*") Profile profile,
            @JniType("std::u16string") String pageTitle,
            @JniType("std::string") String originalUrl,
            @JniType("std::string") String finalUrl,
            int tabId,
            long offlineId) {
        OfflinePageUtils.getLoadUrlParamsForOpeningOfflineVersion(
                finalUrl,
                offlineId,
                LaunchLocation.NOTIFICATION,
                (params) -> {
                    assert params != null;
                    if (profile.shutdownStarted()) return;
                    showCompleteNotificationWithParams(
                            profile, pageTitle, tabId, offlineId, originalUrl, finalUrl, params);
                },
                profile);
    }

    private static void showCompleteNotificationWithParams(
            Profile profile,
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
        // TODO(crbug.com/40254448): Honor the profile token when IntentHandler routes ACTION_VIEW
        // intents.
        ProfileTokenUtils.addProfileToIntent(profile, clickIntent);

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
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setDeleteIntent(
                                PendingIntentProvider.getBroadcast(
                                        context,
                                        /* requestCode= */ 0,
                                        deleteIntent,
                                        /* flags= */ 0));

        NotificationWrapper notification = builder.buildNotificationWrapper();
        BaseNotificationManagerProxy manager = BaseNotificationManagerProxyFactory.create();
        manager.notify(notification);
        NotificationUmaTracker.getInstance()
                .onNotificationShown(
                        NotificationUmaTracker.SystemNotificationType.OFFLINE_PAGES,
                        notification.getNotification());
        if (sTestHooks != null) {
            sTestHooks.completeNotificationShown(clickIntent, deleteIntent);
        }
    }

    // TODO(crbug.com/587416933): Scope this pref per profile (and update the notification count).
    private static boolean isShowingInProgressNotification() {
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.OFFLINE_AUTO_FETCH_SHOWING_IN_PROGRESS, false);
    }

    private static void setIsShowingInProgressNotification(boolean showing) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.OFFLINE_AUTO_FETCH_SHOWING_IN_PROGRESS, showing);
    }

    /**
     * Cancels the requests of the profile of an in-progress notification. Requires native.
     *
     * @param intent The intent of the notification's cancel button or delete action.
     */
    private static void cancelInProgress(Intent intent) {
        if (!ProfileTokenUtils.hasProfileToken(intent)) {
            // TODO(crbug.com/585922618): Remove this fallback in M163 once notifications posted
            // before profile tokens were added have expired.
            AutoFetchNotifierJni.get().cancelInProgress(ProfileManager.getLastUsedRegularProfile());
            return;
        }
        ProfileTokenUtils.retrieveProfileFromIntent(
                intent,
                (profile) -> {
                    if (profile == null) {
                        // The profile no longer exists, so there is nothing to cancel. Clear the
                        // pref so that no other profile cancels its requests on next startup.
                        cancellationComplete();
                        return;
                    }
                    AutoFetchNotifierJni.get().cancelInProgress(profile);
                });
    }

    @NativeMethods
    interface Natives {
        void cancelInProgress(@JniType("Profile*") Profile profile);
    }
}
