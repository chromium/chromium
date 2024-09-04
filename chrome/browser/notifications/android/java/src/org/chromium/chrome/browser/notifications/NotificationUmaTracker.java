// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.Manifest;
import android.app.Notification;
import android.content.pm.PackageManager;
import android.os.Build;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.core.app.NotificationChannelCompat;
import androidx.core.app.NotificationManagerCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.MathUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Helper class to make tracking notification UMA stats easier for various features. Having a single
 * entry point here to make more complex tracking easier to add in the future.
 */
public class NotificationUmaTracker {
    /*
     * A list of notification types.  To add a type to this list please update
     * SystemNotificationType in enums.xml and make sure to keep this list in sync.  Additions
     * should be treated as APPEND ONLY to keep the UMA metric semantics the same over time.
     *
     * A SystemNotificationType value can also be saved in shared preferences.
     */
    @IntDef({
        SystemNotificationType.UNKNOWN,
        SystemNotificationType.DOWNLOAD_FILES,
        SystemNotificationType.DOWNLOAD_PAGES,
        SystemNotificationType.CLOSE_INCOGNITO,
        SystemNotificationType.CONTENT_SUGGESTION,
        SystemNotificationType.MEDIA_CAPTURE,
        SystemNotificationType.PHYSICAL_WEB,
        SystemNotificationType.MEDIA,
        SystemNotificationType.SITES,
        SystemNotificationType.SYNC,
        SystemNotificationType.WEBAPK,
        SystemNotificationType.BROWSER_ACTIONS,
        SystemNotificationType.WEBAPP_ACTIONS,
        SystemNotificationType.OFFLINE_CONTENT_SUGGESTION,
        SystemNotificationType.TRUSTED_WEB_ACTIVITY_SITES,
        SystemNotificationType.OFFLINE_PAGES,
        SystemNotificationType.SEND_TAB_TO_SELF,
        SystemNotificationType.UPDATES,
        SystemNotificationType.CLICK_TO_CALL,
        SystemNotificationType.SHARED_CLIPBOARD,
        SystemNotificationType.SMS_FETCHER,
        SystemNotificationType.PERMISSION_REQUESTS,
        SystemNotificationType.PERMISSION_REQUESTS_HIGH,
        SystemNotificationType.ANNOUNCEMENT,
        SystemNotificationType.SHARE_SAVE_IMAGE,
        SystemNotificationType.TWA_DISCLOSURE_INITIAL,
        SystemNotificationType.TWA_DISCLOSURE_SUBSEQUENT,
        SystemNotificationType.CHROME_REENGAGEMENT_1,
        SystemNotificationType.CHROME_REENGAGEMENT_2,
        SystemNotificationType.CHROME_REENGAGEMENT_3,
        SystemNotificationType.PRICE_DROP_ALERTS,
        SystemNotificationType.WEBAPK_INSTALL_IN_PROGRESS,
        SystemNotificationType.WEBAPK_INSTALL_COMPLETE,
        SystemNotificationType.PRICE_DROP_ALERTS_CHROME_MANAGED,
        SystemNotificationType.PRICE_DROP_ALERTS_USER_MANAGED,
        SystemNotificationType.CHROME_TIPS,
        SystemNotificationType.BLUETOOTH,
        SystemNotificationType.USB,
        SystemNotificationType.UPM_ERROR,
        SystemNotificationType.WEBAPK_INSTALL_FAILED,
        SystemNotificationType.DATA_SHARING,
        SystemNotificationType.UPM_ACCESS_LOSS_WARNING
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface SystemNotificationType {
        int UNKNOWN = -1;
        int DOWNLOAD_FILES = 0;
        int DOWNLOAD_PAGES = 1;
        int CLOSE_INCOGNITO = 2;
        int CONTENT_SUGGESTION = 3;
        int MEDIA_CAPTURE = 4;
        int PHYSICAL_WEB = 5;
        int MEDIA = 6;
        int SITES = 7;
        int SYNC = 8;
        int WEBAPK = 9;
        int BROWSER_ACTIONS = 10;
        int WEBAPP_ACTIONS = 11;
        int OFFLINE_CONTENT_SUGGESTION = 12;
        int TRUSTED_WEB_ACTIVITY_SITES = 13;
        int OFFLINE_PAGES = 14;
        int SEND_TAB_TO_SELF = 15;
        int UPDATES = 16;
        int CLICK_TO_CALL = 17;
        int SHARED_CLIPBOARD = 18;
        int PERMISSION_REQUESTS = 19;
        int PERMISSION_REQUESTS_HIGH = 20;
        int ANNOUNCEMENT = 21;
        int SHARE_SAVE_IMAGE = 22;
        int TWA_DISCLOSURE_INITIAL = 23;
        int TWA_DISCLOSURE_SUBSEQUENT = 24;
        int CHROME_REENGAGEMENT_1 = 25;
        int CHROME_REENGAGEMENT_2 = 26;
        int CHROME_REENGAGEMENT_3 = 27;
        int PRICE_DROP_ALERTS = 28;
        int SMS_FETCHER = 29;
        int WEBAPK_INSTALL_IN_PROGRESS = 30;
        int WEBAPK_INSTALL_COMPLETE = 31;
        int PRICE_DROP_ALERTS_CHROME_MANAGED = 32;
        int PRICE_DROP_ALERTS_USER_MANAGED = 33;
        int CHROME_TIPS = 34;
        int BLUETOOTH = 35;
        int USB = 36;
        int UPM_ERROR = 37;
        int WEBAPK_INSTALL_FAILED = 38;
        int DATA_SHARING = 39;
        int UPM_ACCESS_LOSS_WARNING = 40;

        int NUM_ENTRIES = 41;
    }

    /*
     * A list of notification action types, each maps to a notification button.
     * To add a type to this list please update SystemNotificationActionType in enums.xml and make
     * sure to keep this list in sync.  Additions should be treated as APPEND ONLY to keep the UMA
     * metric semantics the same over time.
     */
    @IntDef({
        ActionType.UNKNOWN,
        ActionType.DOWNLOAD_PAUSE,
        ActionType.DOWNLOAD_RESUME,
        ActionType.DOWNLOAD_CANCEL,
        ActionType.DOWNLOAD_PAGE_PAUSE,
        ActionType.DOWNLOAD_PAGE_RESUME,
        ActionType.DOWNLOAD_PAGE_CANCEL,
        ActionType.CONTENT_SUGGESTION_SETTINGS,
        ActionType.WEB_APP_ACTION_SHARE,
        ActionType.WEB_APP_ACTION_OPEN_IN_CHROME,
        ActionType.OFFLINE_CONTENT_SUGGESTION_SETTINGS,
        ActionType.SHARING_TRY_AGAIN,
        ActionType.SETTINGS,
        ActionType.ANNOUNCEMENT_ACK,
        ActionType.ANNOUNCEMENT_OPEN,
        ActionType.PRICE_DROP_VISIT_SITE,
        ActionType.PRICE_DROP_TURN_OFF_ALERT,
        ActionType.WEB_APK_ACTION_BACK_TO_SITE,
        ActionType.WEB_APK_ACTION_RETRY,
        ActionType.PRE_UNSUBSCRIBE,
        ActionType.UNDO_UNSUBSCRIBE,
        ActionType.COMMIT_UNSUBSCRIBE_IMPLICIT,
        ActionType.COMMIT_UNSUBSCRIBE_EXPLICIT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActionType {
        int UNKNOWN = -1;
        // Pause button on user download notification.
        int DOWNLOAD_PAUSE = 0;
        // Resume button on user download notification.
        int DOWNLOAD_RESUME = 1;
        // Cancel button on user download notification.
        int DOWNLOAD_CANCEL = 2;
        // Pause button on page download notification.
        int DOWNLOAD_PAGE_PAUSE = 3;
        // Resume button on page download notification.
        int DOWNLOAD_PAGE_RESUME = 4;
        // Cancel button on page download notification.
        int DOWNLOAD_PAGE_CANCEL = 5;
        // Setting button on content suggestion notification.
        int CONTENT_SUGGESTION_SETTINGS = 6;
        // Share button on web app action notification.
        int WEB_APP_ACTION_SHARE = 7;
        // Open in Chrome button on web app action notification.
        int WEB_APP_ACTION_OPEN_IN_CHROME = 8;
        // Setting button in offline content suggestion notification.
        int OFFLINE_CONTENT_SUGGESTION_SETTINGS = 9;
        // Dismiss button on sharing notification.
        // int SHARING_DISMISS = 10; deprecated
        // Try again button on sharing error notification.
        int SHARING_TRY_AGAIN = 11;
        // Settings button for notifications.
        int SETTINGS = 12;
        // Ack button on announcement notification.
        int ANNOUNCEMENT_ACK = 13;
        // Open button on announcement notification.
        int ANNOUNCEMENT_OPEN = 14;
        // "Got it" button on the TWA "Running in Chrome" notification.
        int TWA_NOTIFICATION_ACCEPTANCE = 15;
        // "Cancel" button in auto fetch offline page notification.
        int AUTO_FETCH_CANCEL = 16;

        // Media notification buttons.
        int MEDIA_ACTION_PLAY = 17;
        int MEDIA_ACTION_PAUSE = 18;
        int MEDIA_ACTION_STOP = 19;
        int MEDIA_ACTION_PREVIOUS_TRACK = 20;
        int MEDIA_ACTION_NEXT_TRACK = 21;
        int MEDIA_ACTION_SEEK_FORWARD = 22;
        int MEDIA_ACTION_SEEK_BACKWARD = 23;

        // Price drop notification buttons.
        int PRICE_DROP_VISIT_SITE = 24;
        int PRICE_DROP_TURN_OFF_ALERT = 25;

        // Confirm button on sharing notification.
        int SHARING_CONFIRM = 26;
        // Cancel button on sharing notification.
        int SHARING_CANCEL = 27;

        // Back to site button on WebAPK install error notification.
        int WEB_APK_ACTION_BACK_TO_SITE = 28;
        // Retry button on WebAPK install error notification.
        int WEB_APK_ACTION_RETRY = 29;

        // The one-tap "Unsubscribe" button, used only for persistent web notifications in lieu of
        // the `SETTINGS` button.
        int PRE_UNSUBSCRIBE = 30;

        // The "Undo" button to revert `PRE_UNSUBSCRIBE`.
        int UNDO_UNSUBSCRIBE = 31;

        // The "Okay" button to affirmatively commit `PRE_UNSUBSCRIBE`.
        int COMMIT_UNSUBSCRIBE_EXPLICIT = 32;

        // The "Provisionally Unsubscribed" service notification is dismissed or times out, leading
        // to implicitly committing `PRE_UNSUBSCRIBE`.
        int COMMIT_UNSUBSCRIBE_IMPLICIT = 33;

        // Number of real entries, excluding `UNKNWON`.
        int NUM_ENTRIES = 34;
    }

    /**
     * A list of results from showing the notification permission rationale dialog, defined in
     * enums.xml These values are persisted to logs. Entries should not be renumbered and numeric
     * values should never be reused.
     */
    @IntDef({
        NotificationRationaleResult.POSITIVE_BUTTON_CLICKED,
        NotificationRationaleResult.NEGATIVE_BUTTON_CLICKED,
        NotificationRationaleResult.NAVIGATE_BACK_OR_TOUCH_OUTSIDE,
        NotificationRationaleResult.NOT_ATTACHED_TO_WINDOW,
        NotificationRationaleResult.ACTIVITY_DESTROYED,
        NotificationRationaleResult.BOTTOM_SHEET_BACK_PRESS,
        NotificationRationaleResult.BOTTOM_SHEET_SWIPE,
        NotificationRationaleResult.BOTTOM_SHEET_TAP_SCRIM,
        NotificationRationaleResult.BOTTOM_SHEET_FAILED_TO_OPEN,
        NotificationRationaleResult.BOTTOM_SHEET_DESTROYED,
        NotificationRationaleResult.BOTTOM_SHEET_CLOSED_UNKNOWN,
        NotificationRationaleResult.BOTTOM_SHEET_NEVER_OPENED,
        NotificationRationaleResult.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface NotificationRationaleResult {
        int POSITIVE_BUTTON_CLICKED = 0;
        int NEGATIVE_BUTTON_CLICKED = 1;
        int NAVIGATE_BACK_OR_TOUCH_OUTSIDE = 2;
        int ACTIVITY_DESTROYED = 3;
        int NOT_ATTACHED_TO_WINDOW = 4;
        int BOTTOM_SHEET_BACK_PRESS = 5;
        int BOTTOM_SHEET_SWIPE = 6;
        int BOTTOM_SHEET_TAP_SCRIM = 7;
        int BOTTOM_SHEET_FAILED_TO_OPEN = 8;
        int BOTTOM_SHEET_DESTROYED = 9;
        int BOTTOM_SHEET_CLOSED_UNKNOWN = 10;
        int BOTTOM_SHEET_NEVER_OPENED = 11;

        int NUM_ENTRIES = 12;
    }

    /**
     * A list of possible states of the notification permission, to be recorded on startup. Defined
     * in enums.xml These values are persisted to logs. Entries should not be renumbered and numeric
     * values should never be reused.
     */
    @IntDef({
        NotificationPermissionState.ALLOWED,
        NotificationPermissionState.DENIED_BY_DEVICE_POLICY,
        NotificationPermissionState.DENIED_NEVER_ASKED,
        NotificationPermissionState.DENIED_ASKED_ONCE,
        NotificationPermissionState.DENIED_ASKED_TWICE,
        NotificationPermissionState.DENIED_ASKED_MORE_THAN_TWICE,
        NotificationPermissionState.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface NotificationPermissionState {
        int ALLOWED = 0;
        int DENIED_BY_DEVICE_POLICY = 1;
        int DENIED_NEVER_ASKED = 2;
        int DENIED_ASKED_ONCE = 3;
        int DENIED_ASKED_TWICE = 4;
        int DENIED_ASKED_MORE_THAN_TWICE = 5;

        int NUM_ENTRIES = 6;
    }

    /** The stages of the job handling a notification intent. */
    @IntDef({
        IntentHandlerJobStage.SCHEDULE_JOB,
        IntentHandlerJobStage.SCHEDULE_JOB_FAILED,
        IntentHandlerJobStage.ON_START_JOB,
        IntentHandlerJobStage.ON_STOP_JOB,
        IntentHandlerJobStage.DISPATCH_EVENT,
        IntentHandlerJobStage.NATIVE_STARTUP
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface IntentHandlerJobStage {
        int SCHEDULE_JOB = 0;
        int SCHEDULE_JOB_FAILED = 1;
        int ON_START_JOB = 2;
        int ON_STOP_JOB = 3;
        int NATIVE_STARTUP = 4;
        int DISPATCH_EVENT = 5;

        int NUM_ENTRIES = 6;
    }

    /** The action during which the `WasGlobalStatePreserved` histogram is recorded. */
    @IntDef({GlobalStatePreservedActionSuffix.UNDO, GlobalStatePreservedActionSuffix.COMMIT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface GlobalStatePreservedActionSuffix {
        int UNDO = 0;
        int COMMIT = 1;

        int NUM_ENTRIES = 2;
    }

    private static class LazyHolder {
        private static final NotificationUmaTracker INSTANCE = new NotificationUmaTracker();
    }

    // Cached objects.
    private final SharedPreferencesManager mSharedPreferences;
    private final NotificationManagerCompat mNotificationManager;

    public static NotificationUmaTracker getInstance() {
        return LazyHolder.INSTANCE;
    }

    private NotificationUmaTracker() {
        mSharedPreferences = ChromeSharedPreferences.getInstance();
        mNotificationManager = NotificationManagerCompat.from(ContextUtils.getApplicationContext());
    }

    /**
     * Logs {@link android.app.Notification} usage, categorized into {@link SystemNotificationType}
     * types. Splits the logs by the global enabled state of notifications and also logs the last
     * notification shown prior to the global notifications state being disabled by the user.
     *
     * @param type The type of notification that was shown.
     * @param notification The notification that was shown.
     * @see SystemNotificationType
     */
    public void onNotificationShown(
            @SystemNotificationType int type, @Nullable Notification notification) {
        if (type == SystemNotificationType.UNKNOWN || notification == null) return;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            logNotificationShown(type, notification.getChannelId());
        } else {
            logNotificationShown(type, null);
        }
    }

    /**
     * Logs notification click event when the user taps on the notification body.
     *
     * @param type Type of the notification.
     * @param createTime The notification creation timestamp.
     */
    public void onNotificationContentClick(@SystemNotificationType int type, long createTime) {
        if (type == SystemNotificationType.UNKNOWN) return;

        RecordHistogram.recordEnumeratedHistogram(
                "Mobile.SystemNotification.Content.Click",
                type,
                SystemNotificationType.NUM_ENTRIES);
        if (type == SystemNotificationType.DOWNLOAD_FILES) {
            RecordUserAction.record("Mobile.SystemNotification.Content.Click.Downloads_Files");
        }
        recordNotificationAgeHistogram("Mobile.SystemNotification.Content.Click.Age", createTime);

        switch (type) {
            case SystemNotificationType.SEND_TAB_TO_SELF:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Content.Click.Age.SendTabToSelf", createTime);
                break;
            case SystemNotificationType.CLICK_TO_CALL:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Content.Click.Age.ClickToCall", createTime);
                break;
            case SystemNotificationType.SHARED_CLIPBOARD:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Content.Click.Age.SharedClipboard", createTime);
                break;
            case SystemNotificationType.SMS_FETCHER:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Content.Click.Age.SmsFetcher", createTime);
                break;
            case SystemNotificationType.PRICE_DROP_ALERTS_CHROME_MANAGED:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Content.Click.Age.PriceDropChromeManaged",
                        createTime);
                break;
            case SystemNotificationType.PRICE_DROP_ALERTS_USER_MANAGED:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Content.Click.Age.PriceDropUserManaged",
                        createTime);
                break;
        }
    }

    /**
     * Logs notification dismiss event the user swipes away the notification.
     *
     * @param type Type of the notification.
     * @param createTime The notification creation timestamp.
     */
    public void onNotificationDismiss(@SystemNotificationType int type, long createTime) {
        if (type == SystemNotificationType.UNKNOWN) return;

        // TODO(xingliu): This may not work if Android kill Chrome before native library is loaded.
        // Cache data in Android shared preference and flush them to native when available.
        RecordHistogram.recordEnumeratedHistogram(
                "Mobile.SystemNotification.Dismiss", type, SystemNotificationType.NUM_ENTRIES);
        recordNotificationAgeHistogram("Mobile.SystemNotification.Dismiss.Age", createTime);

        switch (type) {
            case SystemNotificationType.SEND_TAB_TO_SELF:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Dismiss.Age.SendTabToSelf", createTime);
                break;
            case SystemNotificationType.CLICK_TO_CALL:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Dismiss.Age.ClickToCall", createTime);
                break;
            case SystemNotificationType.SHARED_CLIPBOARD:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Dismiss.Age.SharedClipboard", createTime);
                break;
            case SystemNotificationType.SMS_FETCHER:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Dismiss.Age.SmsFetcher", createTime);
                break;
            case SystemNotificationType.PRICE_DROP_ALERTS_CHROME_MANAGED:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Dismiss.Age.PriceDropChromeManaged", createTime);
                break;
            case SystemNotificationType.PRICE_DROP_ALERTS_USER_MANAGED:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Dismiss.Age.PriceDropUserManaged", createTime);
                break;
        }
    }

    /**
     * Logs notification button click event.
     *
     * @param actionType Type of the notification action button.
     * @param notificationType Type of the notification.
     * @param createTime The notification creation timestamp.
     */
    public void onNotificationActionClick(
            @ActionType int actionType,
            @SystemNotificationType int notificationType,
            long createTime) {
        if (actionType == ActionType.UNKNOWN) return;

        // TODO(xingliu): This may not work if Android kill Chrome before native library is loaded.
        // Cache data in Android shared preference and flush them to native when available.
        RecordHistogram.recordEnumeratedHistogram(
                "Mobile.SystemNotification.Action.Click", actionType, ActionType.NUM_ENTRIES);
        recordNotificationAgeHistogram("Mobile.SystemNotification.Action.Click.Age", createTime);

        switch (notificationType) {
            case SystemNotificationType.SEND_TAB_TO_SELF:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Action.Click.Age.SendTabToSelf", createTime);
                break;
            case SystemNotificationType.CLICK_TO_CALL:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Action.Click.Age.ClickToCall", createTime);
                break;
            case SystemNotificationType.SHARED_CLIPBOARD:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Action.Click.Age.SharedClipboard", createTime);
                break;
            case SystemNotificationType.SMS_FETCHER:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Action.Click.Age.SmsFetcher", createTime);
                break;
            case SystemNotificationType.PRICE_DROP_ALERTS_CHROME_MANAGED:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Action.Click.Age.PriceDropChromeManaged",
                        createTime);
                break;
            case SystemNotificationType.PRICE_DROP_ALERTS_USER_MANAGED:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Action.Click.Age.PriceDropUserManaged",
                        createTime);
                break;
        }
    }

    /**
     * Records the count of requests for notification permission, this includes either showing the
     * OS prompt or Chrome's permission rationale.
     */
    public void onNotificationPermissionRequested() {
        int requestCount =
                mSharedPreferences.readInt(
                        ChromePreferenceKeys.NOTIFICATION_PERMISSION_REQUEST_COUNT);
        RecordHistogram.recordExactLinearHistogram(
                "Mobile.SystemNotification.Permission.StartupRequestCount", requestCount, 50);
    }

    /**
     * Records the result of an OS prompt for notification permissions.
     *
     * @param permissions List of permissions requested, the only element should be the notification
     *     permission.
     * @param grantResults List of grant results.
     */
    public void onNotificationPermissionRequestResult(String[] permissions, int[] grantResults) {
        if (permissions == null
                || permissions.length != 1
                || grantResults.length != 1
                || !permissions[0].equals(Manifest.permission.POST_NOTIFICATIONS)) {
            assert permissions != null : "Parameter permissions should not be null";
            assert permissions.length == 1 : "A single permission should have been requested";
            assert grantResults.length == 1 : "A single result should have been returned";
            assert permissions[0].equals(Manifest.permission.POST_NOTIFICATIONS)
                    : "The requested permission should be for notifications";
            return;
        }

        boolean isPermissionGranted = grantResults[0] == PackageManager.PERMISSION_GRANTED;

        RecordHistogram.recordBooleanHistogram(
                "Mobile.SystemNotification.Permission.OSPromptResult", isPermissionGranted);
    }

    /**
     * Called when the app's notifications are blocked or allowed through Android settings or when
     * allowed through the OS prompt.
     *
     * @param blockedState If true all notifications are blocked.
     */
    public void onNotificationPermissionSettingChange(boolean blockedState) {
        boolean isPermissionGranted = !blockedState;

        RecordHistogram.recordBooleanHistogram(
                "Mobile.SystemNotification.Permission.Change", isPermissionGranted);
    }

    /** Records the result of showing the notification permission rationale dialog. */
    public void onNotificationPermissionRationaleResult(@NotificationRationaleResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                "Mobile.SystemNotification.Permission.RationaleResult",
                result,
                NotificationRationaleResult.NUM_ENTRIES);
    }

    /** Records a metric indicating the state of notification permissions on startup. */
    public void recordNotificationPermissionState(@NotificationPermissionState int state) {
        RecordHistogram.recordEnumeratedHistogram(
                "Mobile.SystemNotification.Permission.StartupState",
                state,
                NotificationPermissionState.NUM_ENTRIES);
    }

    /**
     * Records whether the origin was already in the provisionally unsubscribed state when
     * processing a tap on the `PRE_UNSUBSCRIBE` action button.
     */
    public void recordIsDuplicatePreUnsubscribe(boolean isDuplicate) {
        RecordHistogram.recordBooleanHistogram(
                "Mobile.SystemNotification.Permission.OneTapUnsubscribe.IsDuplicatePreUnsubscribe",
                isDuplicate);
    }

    /**
     * Records how long the pre-native processing for the `PRE_UNSUBSCRIBE` action button took in
     * real time, which includes time spent in power-saving modes and/or display being dark.
     */
    public void recordPreUnsubscribeRealDuration(long durationMillis) {
        RecordHistogram.recordMediumTimesHistogram(
                "Mobile.SystemNotification.Permission.OneTapUnsubscribe."
                        + "PreUnsubscribePreNativeRealDuration",
                durationMillis);
    }

    /**
     * Records how long the pre-native processing for the `PRE_UNSUBSCRIBE` action button took in
     * `uptimeMillis`, which stops the clock when in power-saving modes and/or display being dark.
     */
    public void recordPreUnsubscribeDuration(long durationMillis) {
        RecordHistogram.recordMediumTimesHistogram(
                "Mobile.SystemNotification.Permission.OneTapUnsubscribe."
                        + "PreUnsubscribePreNativeDuration",
                durationMillis);
    }

    /**
     * Records the time, as perceived by the user, that has elapsed between the most recent
     * non-duplicate `PRE_UNSUBSCRIBE` intent and the current, duplicate `PRE_UNSUBSCRIBE` intent,
     * including time spent in power-saving modes and/or display being dark.
     */
    public void recordDuplicatePreUnsubscribeRealDelay(long delayMillis) {
        RecordHistogram.recordMediumTimesHistogram(
                "Mobile.SystemNotification.Permission.OneTapUnsubscribe."
                        + "DuplicatePreUnsubscribeRealDelay",
                delayMillis);
    }

    /**
     * Records whether the Java global state was preserved between `PRE_UNSUBSCRIBE` and the
     * `UNDO_UNSUBSCRIBE`/`COMMIT_UNSUBSCRIBE_*` events.
     */
    public void recordWasGlobalStatePreserved(
            @GlobalStatePreservedActionSuffix int action, boolean wasPreserved) {
        RecordHistogram.recordBooleanHistogram(
                "Mobile.SystemNotification.Permission.OneTapUnsubscribe.WasGlobalStatePreserved."
                        + (action == GlobalStatePreservedActionSuffix.UNDO ? "Undo" : "Commit"),
                wasPreserved);
    }

    /**
     * Records a sample to indicate that the job to handle a notification intent has reached a given
     * stage.
     *
     * @param stage The stage reached.
     * @param intentAction The action of the intent being processed.
     */
    public void recordIntentHandlerJobStage(@IntentHandlerJobStage int stage, String intentAction) {
        RecordHistogram.recordSparseHistogram("Notifications.Android.JobStage", stage);
        if (NotificationConstants.ACTION_PRE_UNSUBSCRIBE.equals(intentAction)) {
            RecordHistogram.recordSparseHistogram(
                    "Notifications.Android.JobStage.PreUnsubscribe", stage);
        }
    }

    /**
     * Records the number of notifications that were suspended every time the user hits the
     * `PRE_UNSUBSCRIBE` action button.
     *
     * @param count The number of notifications suspended, including the clicked notification.
     */
    public void recordSuspendedNotificationCountOnUnsubscribe(int count) {
        RecordHistogram.recordCount100Histogram(
                "Mobile.SystemNotification.Permission.OneTapUnsubscribe.SuspendedNotificationCount",
                count);
    }

    private void logNotificationShown(
            @SystemNotificationType int type,
            @ChromeChannelDefinitions.ChannelId String channelId) {
        if (!mNotificationManager.areNotificationsEnabled()) {
            logPotentialBlockedCause();
            recordHistogram("Mobile.SystemNotification.Blocked", type);
            return;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                && channelId != null
                && isChannelBlocked(channelId)) {
            recordHistogram("Mobile.SystemNotification.ChannelBlocked", type);
            return;
        }
        saveLastShownNotification(type);
        recordHistogram("Mobile.SystemNotification.Shown", type);
    }

    @RequiresApi(26)
    private boolean isChannelBlocked(@ChromeChannelDefinitions.ChannelId String channelId) {
        NotificationChannelCompat channel =
                mNotificationManager.getNotificationChannelCompat(channelId);
        return channel != null
                && channel.getImportance() == NotificationManagerCompat.IMPORTANCE_NONE;
    }

    private void saveLastShownNotification(@SystemNotificationType int type) {
        mSharedPreferences.writeInt(
                ChromePreferenceKeys.NOTIFICATIONS_LAST_SHOWN_NOTIFICATION_TYPE, type);
    }

    private void logPotentialBlockedCause() {
        int lastType =
                mSharedPreferences.readInt(
                        ChromePreferenceKeys.NOTIFICATIONS_LAST_SHOWN_NOTIFICATION_TYPE,
                        SystemNotificationType.UNKNOWN);
        if (lastType == -1) return;
        mSharedPreferences.removeKey(
                ChromePreferenceKeys.NOTIFICATIONS_LAST_SHOWN_NOTIFICATION_TYPE);

        recordHistogram("Mobile.SystemNotification.BlockedAfterShown", lastType);
    }

    private static void recordHistogram(String name, @SystemNotificationType int type) {
        if (type == SystemNotificationType.UNKNOWN) return;

        RecordHistogram.recordEnumeratedHistogram(name, type, SystemNotificationType.NUM_ENTRIES);
    }

    /**
     * Records the notification age, defined as the duration from the notification shown to the time
     * when an user interaction happens.
     *
     * @param name The histogram name.
     * @param createTime The creation timestamp of the notification, generated by {@link
     *     System#currentTimeMillis()}.
     */
    private static void recordNotificationAgeHistogram(String name, long createTime) {
        // If we didn't get shared preference data, do nothing.
        if (createTime == NotificationIntentInterceptor.INVALID_CREATE_TIME) return;

        int ageSample =
                (int)
                        MathUtils.clamp(
                                (System.currentTimeMillis() - createTime)
                                        / DateUtils.MINUTE_IN_MILLIS,
                                0,
                                Integer.MAX_VALUE);
        RecordHistogram.recordCustomCountHistogram(
                name,
                ageSample,
                1,
                (int) (DateUtils.WEEK_IN_MILLIS / DateUtils.MINUTE_IN_MILLIS),
                50);
    }
}
