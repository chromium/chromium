// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.notifications;

import android.app.Activity;

import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.permissions.NotificationPermissionController;
import org.chromium.ui.permissions.ContextualNotificationPermissionRequester;

/**
 * Implementation of {@link ContextualNotificationPermissionRequester}. Contains the necessary
 * hookups to request permission using the last focused chrome activity.
 */
public class ContextualNotificationPermissionRequesterImpl
        extends ContextualNotificationPermissionRequester {
    private static final String FIELD_TRIAL_ENABLE_CONTEXTUAL_PERMISSION_REQUESTS =
            "enable_contextual_permission_requests";

    private static class LazyHolder {
        static final ContextualNotificationPermissionRequesterImpl INSTANCE =
                new ContextualNotificationPermissionRequesterImpl();
    }

    /** Called to initialize the singleton instance. */
    public static void initialize() {
        ContextualNotificationPermissionRequester.setInstance(LazyHolder.INSTANCE);
    }

    @Override
    public void requestPermissionIfNeeded() {
        boolean isContextualPermissionRequestEnabled =
                ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.NOTIFICATION_PERMISSION_VARIANT,
                        FIELD_TRIAL_ENABLE_CONTEXTUAL_PERMISSION_REQUESTS,
                        false);
        if (!isContextualPermissionRequestEnabled) return;
        NotificationPermissionController permissionController =
                getNotificationPermissionController();
        if (permissionController == null) return;
        permissionController.requestPermissionIfNeeded(/* contextual= */ true);
    }

    @Override
    public boolean doesAppLevelSettingsAllowSiteNotifications() {
        NotificationPermissionController permissionController =
                getNotificationPermissionController();
        if (permissionController == null) return true;
        return permissionController.doesAppLevelSettingsAllowSiteNotifications();
    }

    private NotificationPermissionController getNotificationPermissionController() {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (!(activity instanceof ChromeTabbedActivity)) return null;

        // TODO(shaktisahu): Maybe split out the contextual permission logic out of
        // NotificationPermissionController entirely to serve non-chrome activities.
        ChromeTabbedActivity chromeTabbedActivity = (ChromeTabbedActivity) activity;
        return NotificationPermissionController.from(chromeTabbedActivity.getWindowAndroid());
    }
}
