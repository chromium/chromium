// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.provider.Settings;

import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator class for fetching and updating app's permissions in Launchpad management menu.
 */
class AppManagementMenuPermissionsMediator {
    private final Context mContext;
    private final String mPackageName;
    private final Origin mOrigin;
    private final Profile mProfile;
    private PropertyModel mModel;

    /**
     * Creates a new AppManagementMenuPermissionsMediator.
     * @param context The associated Context.
     * @param packageName The package name of the WebAPK that is showing in the management menu.
     * @param origin The origin of the WebAPK that is showing in the management menu. Use for
     * getting and setting site permissionss and open notification channel setting.
     */
    AppManagementMenuPermissionsMediator(Context context, String packageName, Origin origin) {
        mContext = context;
        mPackageName = packageName;
        mOrigin = origin;
        mProfile = Profile.getLastUsedRegularProfile();
        if (mOrigin == null) {
            // If the WebAPK does not have valid origin, set the PropertyModel to an empty one so no
            // permission will be updated.
            mModel = new PropertyModel.Builder(AppManagementMenuPermissionsProperties.ALL_KEYS)
                             .build();
        } else {
            mModel = buildModel();
        }
    }

    PropertyModel getModel() {
        return mModel;
    }

    private PropertyModel buildModel() {
        return new PropertyModel.Builder(AppManagementMenuPermissionsProperties.ALL_KEYS)
                .with(AppManagementMenuPermissionsProperties.NOTIFICATIONS,
                        getContentSetting(toContentSettingsType(
                                AppManagementMenuPermissionsProperties.NOTIFICATIONS)))
                .with(AppManagementMenuPermissionsProperties.MIC,
                        getContentSetting(
                                toContentSettingsType(AppManagementMenuPermissionsProperties.MIC)))
                .with(AppManagementMenuPermissionsProperties.CAMERA,
                        getContentSetting(toContentSettingsType(
                                AppManagementMenuPermissionsProperties.CAMERA)))
                .with(AppManagementMenuPermissionsProperties.LOCATION,
                        getContentSetting(toContentSettingsType(
                                AppManagementMenuPermissionsProperties.LOCATION)))
                .with(AppManagementMenuPermissionsProperties.ON_CLICK,
                        (propertyKey) -> onButtonClick(propertyKey))
                .build();
    }

    private void onButtonClick(PropertyModel.WritableIntPropertyKey key) {
        @ContentSettingsType
        int type = toContentSettingsType(key);
        @ContentSettingValues
        int value = getContentSetting(type);
        if (value == ContentSettingValues.ASK || value == ContentSettingValues.DEFAULT) {
            assert false : "button should not be clickable";
            return;
        }

        boolean shouldEnable = (value == ContentSettingValues.BLOCK);
        @ContentSettingValues
        int newPermission = shouldEnable ? ContentSettingValues.ALLOW : ContentSettingValues.BLOCK;

        // Notifications permission on O+ devices are managed by notification channel.
        if (type == ContentSettingsType.NOTIFICATIONS
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            launchNotificationChannelSettings();
            return;
        }

        Profile profile = Profile.getLastUsedRegularProfile();
        PermissionInfo permissionInfo =
                new PermissionInfo(type, mOrigin.toString(), null /* embedder */);
        permissionInfo.setContentSetting(profile, newPermission);

        mModel.set(key, newPermission);
    }

    private void launchNotificationChannelSettings() {
        String channelId =
                SiteChannelsManager.getInstance().getChannelIdForOrigin(mOrigin.toString());
        Intent intent = new Intent(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS);
        intent.putExtra(Settings.EXTRA_CHANNEL_ID, channelId);
        intent.putExtra(Settings.EXTRA_APP_PACKAGE, mContext.getPackageName());
        mContext.startActivity(intent);
    }

    private @ContentSettingValues int getContentSetting(@ContentSettingsType int type) {
        PermissionInfo permissionInfo = new PermissionInfo(type, mOrigin.toString(), null, false);
        return permissionInfo.getContentSetting(mProfile);
    }

    private @ContentSettingsType int toContentSettingsType(PropertyKey key) {
        if (key == AppManagementMenuPermissionsProperties.NOTIFICATIONS) {
            return ContentSettingsType.NOTIFICATIONS;
        } else if (key == AppManagementMenuPermissionsProperties.MIC) {
            return ContentSettingsType.MEDIASTREAM_MIC;
        } else if (key == AppManagementMenuPermissionsProperties.CAMERA) {
            return ContentSettingsType.MEDIASTREAM_CAMERA;
        } else if (key == AppManagementMenuPermissionsProperties.LOCATION) {
            return ContentSettingsType.GEOLOCATION;
        }
        assert false;
        return ContentSettingsType.DEFAULT;
    }
}
