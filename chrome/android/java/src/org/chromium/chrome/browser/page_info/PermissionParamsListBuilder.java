// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.support.v4.app.NotificationManagerCompat;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.style.StyleSpan;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.permissiondelegation.TrustedWebActivityPermissionManager;
import org.chromium.chrome.browser.preferences.website.ContentSettingValues;
import org.chromium.chrome.browser.preferences.website.ContentSettingsResources;
import org.chromium.chrome.browser.preferences.website.WebsitePreferenceBridge;
import org.chromium.components.location.LocationUtils;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.base.PermissionCallback;

import java.util.ArrayList;
import java.util.List;

/**
 * This class is a helper for PageInfoController. It contains the logic required to turn a set of
 * permission values into PermissionParams suitable for PageInfoView to display.
 *
 */
class PermissionParamsListBuilder {
    private final List<PageInfoPermissionEntry> mEntries;
    private final String mFullUrl;
    private final Context mContext;
    private final AndroidPermissionDelegate mPermissionDelegate;
    private final SystemSettingsActivityRequiredListener mSettingsActivityRequiredListener;
    private final Callback<List<PageInfoView.PermissionParams>> mDisplayPermissionsCallback;

    /**
     * Creates a new builder of a list of PermissionParams that can be displayed.
     *
     * @param context Context for accessing string resources.
     * @param permissionDelegate Delegate for checking system permissions.
     * @param fullUrl Full URL of the site whose permissions are being displayed.
     * @param systemSettingsActivityRequiredListener Listener for when we need the user to enable
     *                                               a system setting to proceed.
     * @param displayPermissionsCallback Callback to run to display fresh permissions in response to
     *                                   user interaction with a permission entry.
     */
    PermissionParamsListBuilder(Context context, AndroidPermissionDelegate permissionDelegate,
            String fullUrl,
            SystemSettingsActivityRequiredListener systemSettingsActivityRequiredListener,
            Callback<List<PageInfoView.PermissionParams>> displayPermissionsCallback) {
        mContext = context;
        mFullUrl = fullUrl;
        mSettingsActivityRequiredListener = systemSettingsActivityRequiredListener;
        mPermissionDelegate = permissionDelegate;
        mEntries = new ArrayList<>();
        mDisplayPermissionsCallback = displayPermissionsCallback;
    }

    void addPermissionEntry(String name, int type, @ContentSettingValues int value) {
        mEntries.add(new PageInfoPermissionEntry(name, type, value));
    }

    List<PageInfoView.PermissionParams> build() {
        List<PageInfoView.PermissionParams> permissionParams = new ArrayList<>();
        for (PermissionParamsListBuilder.PageInfoPermissionEntry permission : mEntries) {
            permissionParams.add(createPermissionParams(permission));
        }
        return permissionParams;
    }

    private PageInfoView.PermissionParams createPermissionParams(
            PermissionParamsListBuilder.PageInfoPermissionEntry permission) {
        PageInfoView.PermissionParams permissionParams = new PageInfoView.PermissionParams();

        permissionParams.iconResource = getImageResourceForPermission(permission.type);
        if (permission.setting == ContentSettingValues.ALLOW) {
            LocationUtils locationUtils = LocationUtils.getInstance();
            Intent intentOverride = null;
            String[] androidPermissions = null;
            if (permission.type == ContentSettingsType.GEOLOCATION
                    && !locationUtils.isSystemLocationSettingEnabled()) {
                permissionParams.warningTextResource = R.string.page_info_android_location_blocked;
                intentOverride = locationUtils.getSystemLocationSettingsIntent();
            } else if (shouldShowNotificationsDisabledWarning(permission)) {
                permissionParams.warningTextResource =
                        R.string.page_info_android_permission_blocked;
                intentOverride = ApiCompatibilityUtils.getNotificationSettingsIntent();
            } else if (!hasAndroidPermission(permission.type)) {
                permissionParams.warningTextResource =
                        R.string.page_info_android_permission_blocked;
                androidPermissions = WebsitePreferenceBridge.getAndroidPermissionsForContentSetting(
                        permission.type);
            }

            if (permissionParams.warningTextResource != 0) {
                permissionParams.iconResource = R.drawable.exclamation_triangle;
                permissionParams.iconTintColorResource = R.color.default_icon_color_blue;
                permissionParams.clickCallback =
                        createPermissionClickCallback(intentOverride, androidPermissions);
            }
        }

        // The ads permission requires an additional static subtitle.
        if (permission.type == ContentSettingsType.ADS) {
            permissionParams.subtitleTextResource = R.string.page_info_permission_ads_subtitle;
        }

        SpannableStringBuilder builder = new SpannableStringBuilder();
        SpannableString nameString = new SpannableString(permission.name);
        final StyleSpan boldSpan = new StyleSpan(android.graphics.Typeface.BOLD);
        nameString.setSpan(boldSpan, 0, nameString.length(), Spannable.SPAN_INCLUSIVE_EXCLUSIVE);

        builder.append(nameString);
        builder.append(" – "); // en-dash.
        String status_text = "";

        String managedBy = null;
        if (permission.type == ContentSettingsType.NOTIFICATIONS) {
            TrustedWebActivityPermissionManager manager = TrustedWebActivityPermissionManager.get();
            Origin origin = Origin.create(mFullUrl);
            if (origin != null) {
                managedBy = manager.getDelegateAppName(origin);
            }
        }
        if (managedBy != null) {
            status_text = String.format(
                    mContext.getString(R.string.website_notification_managed_by_app), managedBy);
        } else {
            switch (permission.setting) {
                case ContentSettingValues.ALLOW:
                    status_text = mContext.getString(R.string.page_info_permission_allowed);
                    break;
                case ContentSettingValues.BLOCK:
                    status_text = mContext.getString(R.string.page_info_permission_blocked);
                    break;
                default:
                    assert false : "Invalid setting " + permission.setting + " for permission "
                                   + permission.type;
            }
            if (WebsitePreferenceBridge.isPermissionControlledByDSE(
                        permission.type, mFullUrl, false)) {
                status_text = statusTextForDSEPermission(permission.setting);
            }
        }
        builder.append(status_text);
        permissionParams.status = builder;

        return permissionParams;
    }

    private boolean shouldShowNotificationsDisabledWarning(PageInfoPermissionEntry permission) {
        return permission.type == ContentSettingsType.NOTIFICATIONS
                && !NotificationManagerCompat.from(mContext).areNotificationsEnabled()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.APP_NOTIFICATION_STATUS_MESSAGING);
    }

    private boolean hasAndroidPermission(int contentSettingType) {
        String[] androidPermissions =
                WebsitePreferenceBridge.getAndroidPermissionsForContentSetting(contentSettingType);
        if (androidPermissions == null) return true;
        for (int i = 0; i < androidPermissions.length; i++) {
            if (!mPermissionDelegate.hasPermission(androidPermissions[i])) {
                return false;
            }
        }
        return true;
    }

    /**
     * Finds the Image resource of the icon to use for the given permission.
     *
     * @param permission A valid ContentSettingsType that can be displayed in the PageInfo dialog to
     *                   retrieve the image for.
     * @return The resource ID of the icon to use for that permission.
     */
    private int getImageResourceForPermission(int permission) {
        int icon = ContentSettingsResources.getIcon(permission);
        assert icon != 0 : "Icon requested for invalid permission: " + permission;
        return icon;
    }

    private Runnable createPermissionClickCallback(
            Intent intentOverride, String[] androidPermissions) {
        return () -> {
            if (intentOverride == null && mPermissionDelegate != null) {
                // Try and immediately request missing Android permissions where possible.
                for (int i = 0; i < androidPermissions.length; i++) {
                    if (!mPermissionDelegate.canRequestPermission(androidPermissions[i])) continue;

                    // If any permissions can be requested, attempt to request them all.
                    mPermissionDelegate.requestPermissions(
                            androidPermissions, new PermissionCallback() {
                                @Override
                                public void onRequestPermissionsResult(
                                        String[] permissions, int[] grantResults) {
                                    boolean allGranted = true;
                                    for (int i = 0; i < grantResults.length; i++) {
                                        if (grantResults[i] != PackageManager.PERMISSION_GRANTED) {
                                            allGranted = false;
                                            break;
                                        }
                                    }
                                    if (allGranted) mDisplayPermissionsCallback.onResult(build());
                                }
                            });
                    return;
                }
            }

            mSettingsActivityRequiredListener.onSystemSettingsActivityRequired(intentOverride);
        };
    }

    /**
     * Returns the permission string for the Default Search Engine.
     */
    private String statusTextForDSEPermission(@ContentSettingValues int setting) {
        if (setting == ContentSettingValues.ALLOW) {
            return mContext.getString(R.string.page_info_dse_permission_allowed);
        }

        return mContext.getString(R.string.page_info_dse_permission_blocked);
    }

    /**
     * An entry in the settings dropdown for a given permission. There are two options for each
     * permission: Allow and Block.
     */
    private static final class PageInfoPermissionEntry {
        public final String name;
        public final int type;
        public final @ContentSettingValues int setting;

        PageInfoPermissionEntry(String name, int type, @ContentSettingValues int setting) {
            this.name = name;
            this.type = type;
            this.setting = setting;
        }

        @Override
        public String toString() {
            return name;
        }
    }
}
