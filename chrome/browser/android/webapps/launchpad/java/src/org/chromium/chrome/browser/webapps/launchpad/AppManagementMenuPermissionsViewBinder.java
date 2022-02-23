// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.webapps.launchpad;

import android.view.View;
import android.widget.ImageView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Class responsible for binding the model and the AppManagementMenuPermissionsView..
 */
class AppManagementMenuPermissionsViewBinder {
    static void bind(
            PropertyModel model, AppManagementMenuPermissionsView view, PropertyKey propertyKey) {
        if (propertyKey == AppManagementMenuPermissionsProperties.NOTIFICATIONS) {
            @ContentSettingValues
            int setting = model.get(AppManagementMenuPermissionsProperties.NOTIFICATIONS);
            updatePermissionIcon(view, R.id.notifications_button, setting,
                    R.drawable.gm_filled_notifications_24,
                    R.drawable.gm_filled_notifications_off_24);
        } else if (propertyKey == AppManagementMenuPermissionsProperties.MIC) {
            @ContentSettingValues
            int setting = model.get(AppManagementMenuPermissionsProperties.MIC);
            updatePermissionIcon(view, R.id.mic_button, setting, R.drawable.gm_filled_mic_24,
                    R.drawable.gm_filled_mic_off_24);
        } else if (propertyKey == AppManagementMenuPermissionsProperties.CAMERA) {
            @ContentSettingValues
            int setting = model.get(AppManagementMenuPermissionsProperties.CAMERA);
            updatePermissionIcon(view, R.id.camera_button, setting,
                    R.drawable.gm_filled_videocam_24, R.drawable.gm_filled_videocam_off_24);
        } else if (propertyKey == AppManagementMenuPermissionsProperties.LOCATION) {
            @ContentSettingValues
            int setting = model.get(AppManagementMenuPermissionsProperties.LOCATION);
            updatePermissionIcon(view, R.id.location_button, setting,
                    R.drawable.gm_filled_location_on_24, R.drawable.gm_filled_location_off_24);
        } else if (propertyKey == AppManagementMenuPermissionsProperties.ON_CLICK) {
            view.setOnImageButtonClick(model.get(AppManagementMenuPermissionsProperties.ON_CLICK));
        }
    }

    private static void updatePermissionIcon(
            View view, int iconId, int setting, int enabledIcon, int disabledIcon) {
        ImageView icon = (ImageView) view.findViewById(iconId);
        int resId;
        int tintId;

        switch (setting) {
            case ContentSettingValues.ALLOW:
                // Permission is ALLOW, showing the enabled icon with the default tint.
                resId = enabledIcon;
                tintId = R.color.default_icon_color_tint_list;
                break;
            case ContentSettingValues.BLOCK:
                // Permission is BLOCK, showing the disabled icon (crossed-out icon), with the
                // default tint.
                resId = disabledIcon;
                tintId = R.color.default_icon_color_tint_list;
                break;
            case ContentSettingValues.ASK:
            case ContentSettingValues.DEFAULT:
                // When value is ASK or DEFAULT, the permission is not set (never requested),
                // showing the disabled icon and grey-out tint color.
                resId = disabledIcon;
                tintId = R.color.default_icon_color_disabled;
                icon.setEnabled(false);
                break;
            default:
                assert false : "Unexpected ContentSettingValue: " + setting;
                return;
        }
        icon.setImageResource(resId);
        icon.setImageTintList(AppCompatResources.getColorStateList(view.getContext(), tintId));
    }
}
