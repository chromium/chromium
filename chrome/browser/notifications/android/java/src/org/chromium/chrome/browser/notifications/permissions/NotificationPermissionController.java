// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.permissions;

import android.Manifest;
import android.os.Build;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.core.app.NotificationManagerCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.UnownedUserData;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.NotificationPermissionState;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.ui.permissions.PermissionPrefs;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;

/**
 * Central class containing the logic for when to trigger notification permission request optionally
 * with a rationale.
 */
public class NotificationPermissionController implements UnownedUserData {
    /** Field trial param controlling rationale behavior. */
    public static final String FIELD_TRIAL_ALWAYS_SHOW_RATIONALE_BEFORE_REQUESTING_PERMISSION =
            "always_show_rationale_before_requesting_permission";

    /** Field trial param controlling number of days between permission requests. */
    public static final String FIELD_TRIAL_PERMISSION_REQUEST_INTERVAL_DAYS =
            "permission_request_interval_days";

    /**
     * Field trial param controlling whether site notification requests are allowed when permissions
     * are blocked.
     */
    public static final String FIELD_TRIAL_ALLOW_SITE_NOTIFICATION_REQUESTS =
            "permission_request_allow_site_notification_requests";

    /**
     * Field trial param controlling how many times notification permission request should be shown.
     */
    public static final String FIELD_TRIAL_PERMISSION_REQUEST_MAX_COUNT =
            "permission_request_max_count";

    /**
     * Returns whether the bottom sheet rationale UI should be used.
     *
     * @return true if the bottom sheet UI should be used, false if the dialog UI should be used.
     */
    public static boolean shouldUseBottomSheetRationaleUi() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.NOTIFICATION_PERMISSION_BOTTOM_SHEET);
    }

    /** Refers to what type of permission UI should be shown. */
    @IntDef({
        PermissionRequestMode.DO_NOT_REQUEST,
        PermissionRequestMode.REQUEST_ANDROID_PERMISSION,
        PermissionRequestMode.REQUEST_PERMISSION_WITH_RATIONALE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PermissionRequestMode {
        /** Do not start any permission request. */
        int DO_NOT_REQUEST = 0;

        /** Show android permission dialog. */
        int REQUEST_ANDROID_PERMISSION = 1;

        /**
         * Show the in-app permission dialog. Depending on user action it might lead to the android
         * permission dialog.
         */
        int REQUEST_PERMISSION_WITH_RATIONALE = 2;
    }

    /** Refers to the result of trying to show the rationale UI. */
    @IntDef({RationaleUiResult.ACCEPTED, RationaleUiResult.REJECTED, RationaleUiResult.NOT_SHOWN})
    public @interface RationaleUiResult {
        /** Rationale UI was shown and user accepted. */
        int ACCEPTED = 0;

        /** Rationale UI was shown and user rejected or dismissed. */
        int REJECTED = 1;

        /** Rationale UI couldn't be shown. */
        int NOT_SHOWN = 2;
    }

    /** A delegate to show an in-app UI demonstrating rationale behind the permission request. */
    public interface RationaleDelegate {
        /**
         * Called to show the in-app UI.
         *
         * @param callback The callback to be invoked as part of the user action on the dialog UI.
         *     Its argument is a value from {@code RationaleUiResult}.
         */
        void showRationaleUi(Callback<Integer> callback);
    }

    private static final UnownedUserDataKey<NotificationPermissionController> KEY =
            new UnownedUserDataKey<>(NotificationPermissionController.class);

    private final AndroidPermissionDelegate mAndroidPermissionDelegate;
    private final Supplier<RationaleDelegate> mRationaleDelegateSupplier;

    /**
     * Constructor. Should only be called by {@link ChromeTabbedActivity}. Features looking to
     * request this permission in context should instead use {@link
     * ContextualNotificationPermissionRequester}.
     *
     * @param androidPermissionDelegate The delegate to request Android permissions.
     * @param rationaleDelegateSupplier The delegate to show the rationale UI.
     */
    public NotificationPermissionController(
            AndroidPermissionDelegate androidPermissionDelegate,
            Supplier<RationaleDelegate> rationaleDelegateSupplier) {
        mAndroidPermissionDelegate = androidPermissionDelegate;
        mRationaleDelegateSupplier = rationaleDelegateSupplier;
    }

    /**
     * Get the activity's {@link NotificationPermissionController} from the provided {@link
     * WindowAndroid}.
     *
     * @param window The window to get the manager from.
     * @return The {@link NotificationPermissionController} associated with the activity.
     */
    public static @Nullable NotificationPermissionController from(WindowAndroid window) {
        if (window == null) return null;
        return KEY.retrieveDataFromHost(window.getUnownedUserDataHost());
    }

    /**
     * Make this instance of NotificationPermissionController available through the activity's
     * {@link WindowAndroid} for ease of access.
     *
     * @param window A {@link WindowAndroid} to attach to.
     * @param controller The {@link NotificationPermissionController} to attach.
     */
    public static void attach(WindowAndroid window, NotificationPermissionController controller) {
        KEY.attachToHost(window.getUnownedUserDataHost(), controller);
    }

    /**
     * Detach the provided NotificationPermissionController from any {@link WindowAndroid} it is
     * attached with.
     *
     * @param controller The {@link NotificationPermissionController} to detach.
     */
    public static void detach(NotificationPermissionController controller) {
        KEY.detachFromAllHosts(controller);
    }

    /** Called on startup to request permission. See next method for more details. */
    public void requestPermissionIfNeeded() {
        requestPermissionIfNeeded(false);
    }

    /**
     * Called to request notification permission if not granted. Called on startup and contextually
     * by some features using notifications. Internally handles the logic for when to make
     * permission request directly and when to show a rationale beforehand.
     *
     * @param contextual Whether this request is made in context. True for requesting from features
     *     using notifications, false for invoking on startup.
     * @return True if any UI was shown (either rationale dialog or OS prompt), false otherwise.
     */
    public boolean requestPermissionIfNeeded(boolean contextual) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU
                || !BuildInfo.targetsAtLeastT()
                || ApiCompatibilityUtils.isDemoUser()) {
            return false;
        }

        // Record the state of the notification permission before trying to ask but after verifying
        // we are running on Android T.
        recordCurrentNotificationPermissionStatus();

        @PermissionRequestMode int requestMode = shouldRequestPermission();
        if (requestMode == PermissionRequestMode.DO_NOT_REQUEST) return false;

        if (requestMode == PermissionRequestMode.REQUEST_ANDROID_PERMISSION) {
            requestAndroidPermission();
            recordOsPromptShown();
        } else if (requestMode == PermissionRequestMode.REQUEST_PERMISSION_WITH_RATIONALE) {
            mRationaleDelegateSupplier
                    .get()
                    .showRationaleUi(
                            rationaleResult -> {
                                if (rationaleResult != RationaleUiResult.NOT_SHOWN) {
                                    recordRationaleUiShown();
                                }
                                if (rationaleResult == RationaleUiResult.ACCEPTED) {
                                    requestAndroidPermission();
                                }
                            });
        }
        return true;
    }

    private void recordOsPromptShown() {
        ChromeSharedPreferences.getInstance()
                .incrementInt(ChromePreferenceKeys.NOTIFICATION_PERMISSION_REQUEST_COUNT);
        NotificationUmaTracker.getInstance().onNotificationPermissionRequested();
    }

    private void recordRationaleUiShown() {
        ChromeSharedPreferences.getInstance()
                .incrementInt(ChromePreferenceKeys.NOTIFICATION_PERMISSION_REQUEST_COUNT);
        NotificationUmaTracker.getInstance().onNotificationPermissionRequested();
        ChromeSharedPreferences.getInstance()
                .writeLong(
                        ChromePreferenceKeys.NOTIFICATION_PERMISSION_RATIONALE_TIMESTAMP_KEY,
                        TimeUtils.currentTimeMillis());
    }

    @PermissionRequestMode
    int shouldRequestPermission() {
        // Notifications only require permission starting at Android T. And apps targeting < T can't
        // request permission as the OS prompts the user automatically.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU || !BuildInfo.targetsAtLeastT()) {
            return PermissionRequestMode.DO_NOT_REQUEST;
        }

        if (mAndroidPermissionDelegate.hasPermission(Manifest.permission.POST_NOTIFICATIONS)) {
            return PermissionRequestMode.DO_NOT_REQUEST;
        }
        if (!mAndroidPermissionDelegate.canRequestPermission(
                Manifest.permission.POST_NOTIFICATIONS)) {
            return PermissionRequestMode.DO_NOT_REQUEST;
        }

        // Check if it is too soon to request permission again.
        if (wasPermissionRequestShown() && !hasEnoughTimeExpiredForRetriggerSinceLastDenial()) {
            return PermissionRequestMode.DO_NOT_REQUEST;
        }

        // Check if we have already exhausted the max number of times we can request permission.
        // If we have already declined OS prompt twice, we would have bailed out earlier above.
        int previousAttemptCount =
                ChromeSharedPreferences.getInstance()
                        .readInt(ChromePreferenceKeys.NOTIFICATION_PERMISSION_REQUEST_COUNT);
        int maxPermissionRequestCount =
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.NOTIFICATION_PERMISSION_VARIANT,
                        FIELD_TRIAL_PERMISSION_REQUEST_MAX_COUNT,
                        2);
        if (previousAttemptCount >= maxPermissionRequestCount) {
            return PermissionRequestMode.DO_NOT_REQUEST;
        }

        // Decide whether to show the rationale or just the system prompt.
        boolean meetsAndroidRationaleAPI =
                mAndroidPermissionDelegate.shouldShowRequestPermissionRationale(
                        Manifest.permission.POST_NOTIFICATIONS);
        boolean shouldShowRationale = shouldAlwaysShowRationaleFirst() || meetsAndroidRationaleAPI;
        return shouldShowRationale
                ? PermissionRequestMode.REQUEST_PERMISSION_WITH_RATIONALE
                : PermissionRequestMode.REQUEST_ANDROID_PERMISSION;
    }

    /**
     * See {@link
     * ContextualNotificationPermissionRequester#doesAppLevelSettingsAllowSiteNotifications()} for
     * more details.
     *
     * @return Whether or not the site should is allowed to request the notification permission.
     */
    // TODO(shaktisahu): Determine the rules for showing site notification permission.
    public boolean doesAppLevelSettingsAllowSiteNotifications() {
        NotificationManagerCompat manager =
                NotificationManagerCompat.from(ContextUtils.getApplicationContext());
        boolean notificationsEnabledAtAppLevel = manager.areNotificationsEnabled();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            return notificationsEnabledAtAppLevel;
        }

        if (mAndroidPermissionDelegate.hasPermission(Manifest.permission.POST_NOTIFICATIONS)) {
            return true;
        }

        boolean allowRequestingPermissionsForSiteNotifications =
                ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.NOTIFICATION_PERMISSION_VARIANT,
                        FIELD_TRIAL_ALLOW_SITE_NOTIFICATION_REQUESTS,
                        true);
        boolean canRequestPermission =
                mAndroidPermissionDelegate.canRequestPermission(
                        Manifest.permission.POST_NOTIFICATIONS);
        return allowRequestingPermissionsForSiteNotifications && canRequestPermission;
    }

    /**
     * Records the current status of the notification permission (Allowed/Denied) and if denied we
     * include how many times we've asked or if the permission is denied by policy.
     */
    private void recordCurrentNotificationPermissionStatus() {
        if (mAndroidPermissionDelegate.hasPermission(Manifest.permission.POST_NOTIFICATIONS)) {
            NotificationUmaTracker.getInstance()
                    .recordNotificationPermissionState(NotificationPermissionState.ALLOWED);
            return;
        }

        if (mAndroidPermissionDelegate.isPermissionRevokedByPolicy(
                Manifest.permission.POST_NOTIFICATIONS)) {
            NotificationUmaTracker.getInstance()
                    .recordNotificationPermissionState(
                            NotificationPermissionState.DENIED_BY_DEVICE_POLICY);
            return;
        }

        // Get number of times we've requested for notification permission at startup.
        // This count is updated on NotificationUmaTracker.onNotificationPermissionRequested.
        int promptCount =
                ChromeSharedPreferences.getInstance()
                        .readInt(ChromePreferenceKeys.NOTIFICATION_PERMISSION_REQUEST_COUNT, 0);

        switch (promptCount) {
            case 0:
                NotificationUmaTracker.getInstance()
                        .recordNotificationPermissionState(
                                NotificationPermissionState.DENIED_NEVER_ASKED);
                break;
            case 1:
                NotificationUmaTracker.getInstance()
                        .recordNotificationPermissionState(
                                NotificationPermissionState.DENIED_ASKED_ONCE);
                break;
            case 2:
                NotificationUmaTracker.getInstance()
                        .recordNotificationPermissionState(
                                NotificationPermissionState.DENIED_ASKED_TWICE);
                break;
            default:
                NotificationUmaTracker.getInstance()
                        .recordNotificationPermissionState(
                                NotificationPermissionState.DENIED_ASKED_MORE_THAN_TWICE);
                break;
        }
    }

    private void requestAndroidPermission() {
        String[] permissionsToRequest = {Manifest.permission.POST_NOTIFICATIONS};
        mAndroidPermissionDelegate.requestPermissions(
                permissionsToRequest,
                (permissions, grantResults) ->
                        NotificationUmaTracker.getInstance()
                                .onNotificationPermissionRequestResult(permissions, grantResults));
    }

    /** Some heuristic based re-triggering logic. */
    private static boolean hasEnoughTimeExpiredForRetriggerSinceLastDenial() {
        long lastAndroidPermissionRequestTimestamp =
                PermissionPrefs.getAndroidNotificationPermissionRequestTimestamp();
        long lastRationaleTimestamp =
                ChromeSharedPreferences.getInstance()
                        .readLong(
                                ChromePreferenceKeys
                                        .NOTIFICATION_PERMISSION_RATIONALE_TIMESTAMP_KEY,
                                0);
        long lastRequestTimestamp =
                Math.max(lastRationaleTimestamp, lastAndroidPermissionRequestTimestamp);

        // If the pref wasn't there to begin with, we don't need to retrigger the UI.
        if (lastRequestTimestamp == 0) return false;

        long elapsedTime = TimeUtils.currentTimeMillis() - lastRequestTimestamp;
        return elapsedTime > getPermissionRequestRetriggerIntervalMs();
    }

    /** Gets the amount of time to wait between permission requests in milliseconds. */
    private static long getPermissionRequestRetriggerIntervalMs() {
        // Get number of days from param, or use 7 days as default.
        int retriggerIntervalDays =
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.NOTIFICATION_PERMISSION_VARIANT,
                        FIELD_TRIAL_PERMISSION_REQUEST_INTERVAL_DAYS,
                        /* defaultValue= */ 7);

        return TimeUnit.DAYS.toMillis(retriggerIntervalDays);
    }

    private static boolean shouldAlwaysShowRationaleFirst() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.NOTIFICATION_PERMISSION_VARIANT,
                FIELD_TRIAL_ALWAYS_SHOW_RATIONALE_BEFORE_REQUESTING_PERMISSION,
                true);
    }

    private boolean wasPermissionRequestShown() {
        boolean wasAndroidPermissionShown =
                PermissionPrefs.getAndroidNotificationPermissionRequestTimestamp() != 0;
        boolean wasRationaleShown =
                ChromeSharedPreferences.getInstance()
                                .readLong(
                                        ChromePreferenceKeys
                                                .NOTIFICATION_PERMISSION_RATIONALE_TIMESTAMP_KEY,
                                        0)
                        != 0;
        return wasAndroidPermissionShown || wasRationaleShown;
    }
}
