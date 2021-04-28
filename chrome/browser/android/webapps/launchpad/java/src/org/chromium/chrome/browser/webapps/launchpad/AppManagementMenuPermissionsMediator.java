// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator class for fetching and updating app's permissions in Launchpad management menu.
 */
class AppManagementMenuPermissionsMediator {
    private final String mUrl;
    private final Profile mProfile;
    private PropertyModel mModel;

    /**
     * Creates a new AppManagementMenuPermissionsMediator.
     * @param url The launch url of the WebAPK that is showing in the management menu.
     */
    AppManagementMenuPermissionsMediator(String url) {
        mUrl = url;
        mProfile = Profile.getLastUsedRegularProfile();
        mModel = buildModel();
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

    private void onButtonClick(PropertyModel.WritableIntPropertyKey key) {}

    private @ContentSettingValues int getContentSetting(@ContentSettingsType int type) {
        PermissionInfo permissionInfo = new PermissionInfo(type, mUrl, null, false);
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
