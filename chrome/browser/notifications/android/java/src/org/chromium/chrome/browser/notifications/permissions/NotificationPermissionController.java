// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.permissions;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.ui.permissions.PermissionConstants;
import org.chromium.ui.permissions.PermissionPrefs;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;

/**
 * Central class containing the logic for when to trigger notification permission request optionally
 * with a rationale.
 */
public class NotificationPermissionController {
    /** Refers to what type of permission UI should be shown. */
    @IntDef({PermissionRequestMode.DO_NOT_REQUEST, PermissionRequestMode.REQUEST_ANDROID_PERMISSION,
            PermissionRequestMode.REQUEST_PERMISSION_WITH_RATIONALE})
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

    /** A delegate to show an in-app UI demonstrating rationale behind the permission request. */
    interface RationaleDelegate {
        /**
         * Called to show the in-app UI.
         * @param callback The callback to be invoked as part of the user action on the dialog UI.
         */
        void showRationaleUi(Callback<Boolean> callback);
    }

    private static final long PERMISSION_REQUEST_RETRIGGER_INTERVAL = TimeUnit.DAYS.toMillis(7);

    private final AndroidPermissionDelegate mAndroidPermissionDelegate;
    private final RationaleDelegate mRationaleDelegate;

    /**
     * Constructor.
     * @param androidPermissionDelegate The delegate to request Android permissions.
     * @param rationaleDelegate The delegate to show the rationale UI.
     */
    public NotificationPermissionController(AndroidPermissionDelegate androidPermissionDelegate,
            RationaleDelegate rationaleDelegate) {
        mAndroidPermissionDelegate = androidPermissionDelegate;
        mRationaleDelegate = rationaleDelegate;
    }

    /**
     * Called on startup to request notification permission. Internally handles the logic for when
     * to make permission request and when to show a rationale.
     */
    public void requestPermissionIfNeeded() {
        @PermissionRequestMode
        int requestMode = shouldRequestPermission();
        if (requestMode == PermissionRequestMode.DO_NOT_REQUEST) return;

        if (requestMode == PermissionRequestMode.REQUEST_ANDROID_PERMISSION) {
            requestAndroidPermission();
        } else if (requestMode == PermissionRequestMode.REQUEST_PERMISSION_WITH_RATIONALE) {
            SharedPreferencesManager.getInstance().writeLong(
                    ChromePreferenceKeys.NOTIFICATION_PERMISSION_RATIONALE_TIMESTAMP_KEY,
                    System.currentTimeMillis());
            mRationaleDelegate.showRationaleUi(accept -> {
                if (accept) {
                    requestAndroidPermission();
                }
            });
        }
    }

    @VisibleForTesting
    @PermissionRequestMode
    int shouldRequestPermission() {
        if (!BuildInfo.isAtLeastT()) return PermissionRequestMode.DO_NOT_REQUEST;
        if (mAndroidPermissionDelegate.hasPermission(PermissionConstants.NOTIFICATION_PERMISSION)) {
            return PermissionRequestMode.DO_NOT_REQUEST;
        }
        if (!mAndroidPermissionDelegate.canRequestPermission(
                    PermissionConstants.NOTIFICATION_PERMISSION)) {
            return PermissionRequestMode.DO_NOT_REQUEST;
        }
        if (mAndroidPermissionDelegate.shouldShowRequestPermissionRationale(
                    PermissionConstants.NOTIFICATION_PERMISSION)) {
            // Also check if we have already shown rationale.
            boolean wasRationaleShown =
                    SharedPreferencesManager.getInstance().readLong(
                            ChromePreferenceKeys.NOTIFICATION_PERMISSION_RATIONALE_TIMESTAMP_KEY, 0)
                    != 0;
            boolean canRequestPermission =
                    !wasRationaleShown && hasEnoughTimeExpiredForRetriggerSinceLastDenial();
            return canRequestPermission ? PermissionRequestMode.REQUEST_PERMISSION_WITH_RATIONALE
                                        : PermissionRequestMode.DO_NOT_REQUEST;
        } else {
            boolean wasAndroidPermssionShown =
                    PermissionPrefs.getAndroidNotificationPermissionRequestTimestamp() != 0;
            if (wasAndroidPermssionShown) {
                return hasEnoughTimeExpiredForRetriggerSinceLastDenial()
                        ? PermissionRequestMode.REQUEST_ANDROID_PERMISSION
                        : PermissionRequestMode.DO_NOT_REQUEST;
            }

            return PermissionRequestMode.REQUEST_ANDROID_PERMISSION;
        }
    }

    private void requestAndroidPermission() {
        String[] permissionsToRequest = {PermissionConstants.NOTIFICATION_PERMISSION};
        mAndroidPermissionDelegate.requestPermissions(
                permissionsToRequest, (permissions, grantResults) -> {});
    }

    /** Some heuristic based re-triggering logic. */
    private static boolean hasEnoughTimeExpiredForRetriggerSinceLastDenial() {
        long lastAndroidPermissionRequestTimestamp =
                PermissionPrefs.getAndroidNotificationPermissionRequestTimestamp();
        long lastRationaleTimestamp = SharedPreferencesManager.getInstance().readLong(
                ChromePreferenceKeys.NOTIFICATION_PERMISSION_RATIONALE_TIMESTAMP_KEY, 0);
        long lastRequestTimestamp =
                Math.max(lastRationaleTimestamp, lastAndroidPermissionRequestTimestamp);

        // If the pref wasn't there to begin with, we don't need to retrigger the UI.
        if (lastRequestTimestamp == 0) return false;

        long elapsedTime = System.currentTimeMillis() - lastRequestTimestamp;
        return elapsedTime > PERMISSION_REQUEST_RETRIGGER_INTERVAL;
    }
}
