// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.os.Bundle;

import androidx.annotation.Nullable;

import java.util.List;

/**
 * Safety Hub subpage that displays a list of all to be reviewed notifications alongside their
 * supported actions.
 */
public class SafetyHubNotificationsFragment extends SafetyHubSubpageFragment
        implements NotificationPermissionReviewBridge.Observer,
                SafetyHubNotificationsPreference.MenuClickListener {
    private NotificationPermissionReviewBridge mNotificationPermissionReviewBridge;

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        super.onCreatePreferences(bundle, s);

        mNotificationPermissionReviewBridge =
                NotificationPermissionReviewBridge.getForProfile(getProfile());
        mNotificationPermissionReviewBridge.addObserver(this);
    }

    @Override
    public void notificationPermissionsChanged() {
        updatePreferenceList();
    }

    @Override
    public void onAllowClicked(SafetyHubNotificationsPreference preference) {
        NotificationPermissions notificationPermissions =
                ((SafetyHubNotificationsPreference) preference).getNotificationsPermissions();
        mNotificationPermissionReviewBridge.ignoreOriginForNotificationPermissionReview(
                notificationPermissions.getPrimaryPattern());
    }

    @Override
    public void onBlockClicked(SafetyHubNotificationsPreference preference) {
        NotificationPermissions notificationPermissions =
                ((SafetyHubNotificationsPreference) preference).getNotificationsPermissions();
        mNotificationPermissionReviewBridge.blockNotificationPermissionForOrigin(
                notificationPermissions.getPrimaryPattern());
    }

    @Override
    public void onAskClicked(SafetyHubNotificationsPreference preference) {
        NotificationPermissions notificationPermissions =
                ((SafetyHubNotificationsPreference) preference).getNotificationsPermissions();
        mNotificationPermissionReviewBridge.resetNotificationPermissionForOrigin(
                notificationPermissions.getPrimaryPattern());
    }

    @Override
    protected void updatePreferenceList() {
        mPreferenceList.removeAll();

        List<NotificationPermissions> notificationPermissionsList =
                mNotificationPermissionReviewBridge.getNotificationPermissions();
        for (NotificationPermissions notificationPermissions : notificationPermissionsList) {
            SafetyHubNotificationsPreference preference =
                    new SafetyHubNotificationsPreference(getContext(), notificationPermissions);
            preference.setMenuClickListener(this);
            mPreferenceList.addPreference(preference);
        }
    }

    @Override
    protected int getTitleId() {
        return R.string.safety_hub_notifications_page_title;
    }

    @Override
    protected int getHeaderId() {
        return R.string.safety_hub_notifications_page_header;
    }

    @Override
    protected void onBottomButtonClicked() {
        // TODO(crbug.com/324562205): React to the 'Got it' button being clicked.
    }
}
