// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.annotation.SuppressLint;
import android.content.res.Resources;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ContentSettingsType;

import java.util.HashMap;
import java.util.Map;

/**
 * A class with utility functions that get the appropriate string and icon resources for the
 * Android UI that allows managing content settings.
 */
// The Linter suggests using SparseArray<ResourceItem> instead of a HashMap
// because our key is an int but we're changing the key to a string soon so
// suppress the lint warning for now.
@SuppressLint("UseSparseArrays")
public class ContentSettingsResources {
    /**
     * An inner class contains all the resources for a ContentSettingsType
     */
    private static class ResourceItem {
        private final int mIcon;
        private final int mTitle;
        private final int mExplanation;
        private final ContentSetting mDefaultEnabledValue;
        private final ContentSetting mDefaultDisabledValue;
        private final int mEnabledSummary;
        private final int mDisabledSummary;

        ResourceItem(int icon, int title, int explanation, ContentSetting defaultEnabledValue,
                ContentSetting defaultDisabledValue, int enabledSummary, int disabledSummary) {
            mIcon = icon;
            mTitle = title;
            mExplanation = explanation;
            mDefaultEnabledValue = defaultEnabledValue;
            mDefaultDisabledValue = defaultDisabledValue;
            mEnabledSummary = enabledSummary;
            mDisabledSummary = disabledSummary;
        }

        private int getIcon() {
            return mIcon;
        }

        private int getTitle() {
            return mTitle;
        }

        private int getExplanation() {
            return mExplanation;
        }

        private ContentSetting getDefaultEnabledValue() {
            return mDefaultEnabledValue;
        }

        private ContentSetting getDefaultDisabledValue() {
            return mDefaultDisabledValue;
        }

        private int getEnabledSummary() {
            return mEnabledSummary == 0 ? getCategorySummary(mDefaultEnabledValue)
                                        : mEnabledSummary;
        }

        private int getDisabledSummary() {
            return mDisabledSummary == 0 ? getCategorySummary(mDefaultDisabledValue)
                                         : mDisabledSummary;
        }
    }

    // TODO(lshang): use string for the index of HashMap after we change the type of
    // ContentSettingsType from int to string.
    private static Map<Integer, ResourceItem> sResourceInfo;

    /**
     * Initializes and returns the map. Only initializes it the first time it's needed.
     */
    private static Map<Integer, ResourceItem> getResourceInfo() {
        ThreadUtils.assertOnUiThread();
        if (sResourceInfo == null) {
            Map<Integer, ResourceItem> localMap = new HashMap<Integer, ResourceItem>();
            localMap.put(ContentSettingsType.CONTENT_SETTINGS_TYPE_ADS,
                    new ResourceItem(R.drawable.web_asset, R.string.ads_permission_title,
                            R.string.ads_permission_title, ContentSetting.ALLOW,
                            ContentSetting.BLOCK, 0,
                            R.string.website_settings_category_ads_blocked));
            localMap.put(ContentSettingsType.CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,
                    new ResourceItem(R.drawable.infobar_downloading,
                            R.string.automatic_downloads_permission_title,
                            R.string.automatic_downloads_permission_title, ContentSetting.ASK,
                            ContentSetting.BLOCK, R.string.website_settings_category_ask, 0));
            localMap.put(ContentSettingsType.CONTENT_SETTINGS_TYPE_AUTOPLAY,
                    new ResourceItem(R.drawable.settings_autoplay, R.string.autoplay_title,
                                 R.string.autoplay_title, ContentSetting.ALLOW,
                                 ContentSetting.BLOCK,
                                 R.string.website_settings_category_autoplay_allowed, 0));
            localMap.put(ContentSettingsType.CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC,
                    new ResourceItem(R.drawable.permission_background_sync,
                                 R.string.background_sync_permission_title,
                                 R.string.background_sync_permission_title, ContentSetting.ALLOW,
                                 ContentSetting.BLOCK,
                                 R.string.website_settings_category_allowed_recommended, 0));
            localMap.put(ContentSettingsType.CONTENT_SETTINGS_TYPE_CLIPBOARD_READ,
                    new ResourceItem(R.drawable.ic_content_paste_grey600_24dp,
                            R.string.clipboard_permission_title,
                            R.string.clipboard_permission_title, ContentSetting.ASK,
                            ContentSetting.BLOCK,
                            R.string.website_settings_category_clipboard_ask,
                            R.string.website_settings_category_clipboard_blocked));
            localMap.put(ContentSettingsType.CONTENT_SETTINGS_TYPE_COOKIES,
                    new ResourceItem(R.drawable.permission_cookie, R.string.cookies_title,
                                 R.string.cookies_title, ContentSetting.ALLOW, ContentSetting.BLOCK,
                                 R.string.website_settings_category_cookie_allowed, 0));
            localMap.put(ContentSettingsType.CONTENT_SETTINGS_TYPE_GEOLOCATION,
                    new ResourceItem(R.drawable.permission_location,
                                 R.string.website_settings_device_location,
                                 R.string.geolocation_permission_title, ContentSetting.ASK,
                                 ContentSetting.BLOCK,
                                 R.string.website_settings_category_location_ask, 0));
            localMap.put(ContentSettingsType.CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                    new ResourceItem(R.drawable.permission_javascript,
                                 R.string.javascript_permission_title,
                                 R.string.javascript_permission_title, ContentSetting.ALLOW,
                                 ContentSetting.BLOCK,
                                 R.string.website_settings_category_javascript_allowed, 0));
            localMap.put(ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                    new ResourceItem(R.drawable.ic_videocam_white_24dp,
                            R.string.website_settings_use_camera, R.string.camera_permission_title,
                            ContentSetting.ASK, ContentSetting.BLOCK,
                            R.string.website_settings_category_camera_ask, 0));
            localMap.put(
                    ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                    new ResourceItem(R.drawable.permission_mic, R.string.website_settings_use_mic,
                            R.string.mic_permission_title, ContentSetting.ASK, ContentSetting.BLOCK,
                            R.string.website_settings_category_mic_ask, 0));
            localMap.put(ContentSettingsType.CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
                    new ResourceItem(R.drawable.permission_midi, 0,
                                 R.string.midi_sysex_permission_title, null, null, 0, 0));
            localMap.put(ContentSettingsType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                    new ResourceItem(R.drawable.permission_push_notification,
                                 R.string.push_notifications_permission_title,
                                 R.string.push_notifications_permission_title, ContentSetting.ASK,
                                 ContentSetting.BLOCK,
                                 R.string.website_settings_category_notifications_ask, 0));
            localMap.put(ContentSettingsType.CONTENT_SETTINGS_TYPE_POPUPS,
                    new ResourceItem(R.drawable.permission_popups, R.string.popup_permission_title,
                            R.string.popup_permission_title, ContentSetting.ALLOW,
                            ContentSetting.BLOCK, 0,
                            R.string.website_settings_category_popups_redirects_blocked));
            // PROTECTED_MEDIA_IDENTIFIER uses 3-state preference so some values are not used.
            // If 3-state becomes more common we should update localMaps to support it better.
            localMap.put(ContentSettingsType.CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
                    new ResourceItem(R.drawable.permission_protected_media,
                                 org.chromium.chrome.R.string.protected_content,
                                 org.chromium.chrome.R.string.protected_content,
                                 ContentSetting.ASK, ContentSetting.BLOCK, 0, 0));
            localMap.put(ContentSettingsType.CONTENT_SETTINGS_TYPE_SOUND,
                    new ResourceItem(R.drawable.ic_volume_up_grey600_24dp,
                            R.string.sound_permission_title, R.string.sound_permission_title,
                            ContentSetting.ALLOW, ContentSetting.BLOCK,
                            R.string.website_settings_category_sound_allowed,
                            R.string.website_settings_category_sound_blocked));
            localMap.put(ContentSettingsType.CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA,
                    new ResourceItem(R.drawable.settings_usb, 0, 0, ContentSetting.ASK,
                            ContentSetting.BLOCK, 0, 0));
            localMap.put(ContentSettingsType.CONTENT_SETTINGS_TYPE_USB_GUARD,
                    new ResourceItem(R.drawable.settings_usb, R.string.website_settings_usb,
                            R.string.website_settings_usb, ContentSetting.ASK, ContentSetting.BLOCK,
                            R.string.website_settings_category_usb_ask,
                            R.string.website_settings_category_usb_blocked));
            localMap.put(ContentSettingsType.CONTENT_SETTINGS_TYPE_SENSORS,
                    new ResourceItem(R.drawable.settings_sensors, R.string.sensors_permission_title,
                            R.string.sensors_permission_title, ContentSetting.ALLOW,
                            ContentSetting.BLOCK,
                            R.string.website_settings_category_sensors_allowed,
                            R.string.website_settings_category_sensors_blocked));
            sResourceInfo = localMap;
        }
        return sResourceInfo;
    }

    /**
     * Returns the ResourceItem for a ContentSettingsType.
     */
    private static ResourceItem getResourceItem(int contentType) {
        return getResourceInfo().get(contentType);
    }

    /**
     * Returns the resource id of the icon for a content type.
     */
    public static int getIcon(int contentType) {
        return getResourceItem(contentType).getIcon();
    }

    /**
     * Returns the Drawable object of the icon for a content type with a disabled tint.
     */
    public static Drawable getDisabledIcon(int contentType, Resources resources) {
        Drawable icon = ApiCompatibilityUtils.getDrawable(resources, getIcon(contentType));
        icon.mutate();
        int disabledColor = ApiCompatibilityUtils.getColor(resources,
                R.color.primary_text_disabled_material_light);
        icon.setColorFilter(disabledColor, PorterDuff.Mode.SRC_IN);
        return icon;
    }

    /**
     * Returns the resource id of the title (short version), shown on the Site Settings page
     * and in the global toggle at the top of a Website Settings page for a content type.
     */
    public static int getTitle(int contentType) {
        return getResourceItem(contentType).getTitle();
    }

    /**
     * Returns the resource id of the title explanation, shown on the Website Details page for
     * a content type.
     */
    public static int getExplanation(int contentType) {
        return getResourceItem(contentType).getExplanation();
    }

    /**
     * Returns which ContentSetting the global default is set to, when enabled.
     * Either Ask/Allow. Not required unless this entry describes a settings
     * that appears on the Site Settings page and has a global toggle.
     */
    public static ContentSetting getDefaultEnabledValue(int contentType) {
        return getResourceItem(contentType).getDefaultEnabledValue();
    }

    /**
     * Returns which ContentSetting the global default is set to, when disabled.
     * Usually Block. Not required unless this entry describes a settings
     * that appears on the Site Settings page and has a global toggle.
     */
    public static ContentSetting getDefaultDisabledValue(int contentType) {
        return getResourceItem(contentType).getDefaultDisabledValue();
    }

    /**
     * Returns the string resource id for a given ContentSetting to show with a permission category.
     * @param value The ContentSetting for which we want the resource.
     */
    public static int getCategorySummary(ContentSetting value) {
        switch (value) {
            case ALLOW:
                return R.string.website_settings_category_allowed;
            case BLOCK:
                return R.string.website_settings_category_blocked;
            case ASK:
                return R.string.website_settings_category_ask;
            default:
                return 0;
        }
    }

    /**
     * Returns the string resource id for a content type to show with a permission category.
     * @param enabled Whether the content type is enabled.
     */
    public static int getCategorySummary(int contentType, boolean enabled) {
        return getCategorySummary(enabled ? getDefaultEnabledValue(contentType)
                                          : getDefaultDisabledValue(contentType));
    }

    /**
     * Returns the string resource id for a given ContentSetting to show
     * with a particular website.
     * @param value The ContentSetting for which we want the resource.
     */
    public static int getSiteSummary(ContentSetting value) {
        switch (value) {
            case ALLOW:
                return R.string.website_settings_permissions_allow;
            case BLOCK:
                return R.string.website_settings_permissions_block;
            default:
                return 0; // We never show Ask as an option on individual permissions.
        }
    }

    /**
     * Returns the summary (resource id) to show when the content type is enabled.
     */
    public static int getEnabledSummary(int contentType) {
        return getResourceItem(contentType).getEnabledSummary();
    }

    /**
     * Returns the summary (resource id) to show when the content type is disabled.
     */
    public static int getDisabledSummary(int contentType) {
        return getResourceItem(contentType).getDisabledSummary();
    }

    /**
     * Returns the summary for Geolocation content settings when it is set to 'Allow' (by policy).
     */
    public static int getGeolocationAllowedSummary() {
        return R.string.website_settings_category_allowed;
    }

    /**
     * Returns the summary for Cookie content settings when it is allowed
     * except for those from third party sources.
     */
    public static int getCookieAllowedExceptThirdPartySummary() {
        return R.string.website_settings_category_allowed_except_third_party;
    }

    /**
     * Returns the summary for Autoplay content settings when it is disabled because of Data Saver
     * being enabled.
     */
    public static int getAutoplayDisabledByDataSaverSummary() {
        return R.string.website_settings_category_autoplay_disabled_data_saver;
    }

    /**
     * Returns the blocked summary for the ads permission which should be used for display in the
     * site settings list only.
     */
    public static int getAdsBlockedListSummary() {
        return R.string.website_settings_category_ads_blocked_list;
    }

    /**
     * Returns the blocked summary for the clipboard permission which should be used for display in
     * the site settings list only.
     */
    public static int getClipboardBlockedListSummary() {
        return R.string.website_settings_category_clipboard_blocked_list;
    }

    /**
     * Returns the blocked summary for the sound permission which should be used for display in the
     * site settings list only.
     */
    public static int getSoundBlockedListSummary() {
        return R.string.website_settings_category_sound_blocked_list;
    }

    /**
     * Returns the resources IDs for descriptions for Allowed, Ask and Blocked states, in that
     * order, on a tri-state setting.
     *
     * @return An array of 3 resource IDs for descriptions for Allowed, Ask and
     *         Blocked states, in that order.
     */
    public static int[] getTriStateSettingDescriptionIDs(int contentType) {
        if (contentType == ContentSettingsType.CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER) {
            int[] descriptionIDs = {R.string.website_settings_category_protected_content_allowed,
                    R.string.website_settings_category_protected_content_ask,
                    R.string.website_settings_category_protected_content_blocked};
            return descriptionIDs;
        }

        assert false;
        return null;
    }
}
