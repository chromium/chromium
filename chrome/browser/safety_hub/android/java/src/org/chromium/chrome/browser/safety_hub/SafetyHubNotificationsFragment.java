// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.os.Bundle;
import android.view.MenuItem;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.favicon.LargeIconBridge;

import java.util.List;

/**
 * Safety Hub subpage that displays a list of all to be reviewed notifications alongside their
 * supported actions.
 */
public class SafetyHubNotificationsFragment extends SafetyHubSubpageFragment
        implements NotificationPermissionReviewBridge.Observer,
                SafetyHubNotificationsPreference.MenuClickListener {
    private NotificationPermissionReviewBridge mNotificationPermissionReviewBridge;
    private LargeIconBridge mLargeIconBridge;

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        super.onCreatePreferences(bundle, s);

        mNotificationPermissionReviewBridge =
                NotificationPermissionReviewBridge.getForProfile(getProfile());
        mNotificationPermissionReviewBridge.addObserver(this);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mNotificationPermissionReviewBridge.removeObserver(this);

        if (mBulkActionConfirmed) {
            List<NotificationPermissions> notificationPermissionsList =
                    mNotificationPermissionReviewBridge.getNotificationPermissions();
            mNotificationPermissionReviewBridge.bulkBlockNotificationPermissions();
            showSnackbarOnLastFocusedActivity(
                    getString(
                            R.string.safety_hub_notifications_bulk_block_snackbar,
                            notificationPermissionsList.size()),
                    Snackbar.UMA_SAFETY_HUB_MULTIPLE_SITE_NOTIFICATIONS,
                    new SnackbarManager.SnackbarController() {
                        @Override
                        public void onAction(Object actionData) {
                            mNotificationPermissionReviewBridge.bulkAllowNotificationPermissions(
                                    (List<NotificationPermissions>) actionData);
                        }
                    },
                    notificationPermissionsList);
        }
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
        showSingleSiteSnackbar(
                R.string.safety_hub_notifications_allow_snackbar,
                notificationPermissions.getPrimaryPattern());
    }

    @Override
    public void onBlockClicked(SafetyHubNotificationsPreference preference) {
        NotificationPermissions notificationPermissions =
                ((SafetyHubNotificationsPreference) preference).getNotificationsPermissions();
        mNotificationPermissionReviewBridge.blockNotificationPermissionForOrigin(
                notificationPermissions.getPrimaryPattern());
        showSingleSiteSnackbar(
                R.string.safety_hub_notifications_block_snackbar,
                notificationPermissions.getPrimaryPattern());
    }

    @Override
    public void onAskClicked(SafetyHubNotificationsPreference preference) {
        NotificationPermissions notificationPermissions =
                ((SafetyHubNotificationsPreference) preference).getNotificationsPermissions();
        mNotificationPermissionReviewBridge.resetNotificationPermissionForOrigin(
                notificationPermissions.getPrimaryPattern());
        showSingleSiteSnackbar(
                R.string.safety_hub_notifications_ask_snackbar,
                notificationPermissions.getPrimaryPattern());
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.safety_hub_subpage_menu_item) {
            launchSiteSettingsActivity(SiteSettingsCategory.Type.NOTIFICATIONS);
            return true;
        }
        return false;
    }

    @Override
    protected void updatePreferenceList() {
        if (mLargeIconBridge == null) {
            mLargeIconBridge = new LargeIconBridge(getProfile());
        }
        mPreferenceList.removeAll();

        List<NotificationPermissions> notificationPermissionsList =
                mNotificationPermissionReviewBridge.getNotificationPermissions();
        for (NotificationPermissions notificationPermissions : notificationPermissionsList) {
            SafetyHubNotificationsPreference preference =
                    new SafetyHubNotificationsPreference(
                            getContext(), notificationPermissions, mLargeIconBridge);
            preference.setMenuClickListener(this);
            mPreferenceList.addPreference(preference);
        }
    }

    @Override
    protected @StringRes int getTitleId() {
        return R.string.safety_hub_notifications_page_title;
    }

    @Override
    protected @StringRes int getHeaderId() {
        return R.string.safety_hub_notifications_page_header;
    }

    @Override
    protected @StringRes int getButtonTextId() {
        return R.string.safety_hub_notifications_block_all_button;
    }

    @Override
    protected @StringRes int getMenuItemTextId() {
        return R.string.safety_hub_notifications_page_menu_text;
    }

    private void showSingleSiteSnackbar(int titleResId, String origin) {
        showSnackbar(
                getString(titleResId, origin),
                Snackbar.UMA_SAFETY_HUB_SINGLE_SITE_NOTIFICATIONS,
                new SnackbarManager.SnackbarController() {
                    @Override
                    public void onAction(Object actionData) {
                        mNotificationPermissionReviewBridge.allowNotificationPermissionForOrigin(
                                (String) actionData);
                    }
                },
                origin);
    }
}
