// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Process;
import android.provider.Settings;
import android.support.v7.preference.Preference;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;

import androidx.annotation.IntDef;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A base class for dealing with website settings categories.
 */
public class SiteSettingsCategory {
    @IntDef({Type.ALL_SITES, Type.ADS, Type.AUTOMATIC_DOWNLOADS, Type.AUTOPLAY,
            Type.BACKGROUND_SYNC, Type.BLUETOOTH_SCANNING, Type.CAMERA, Type.CLIPBOARD,
            Type.COOKIES, Type.DEVICE_LOCATION, Type.JAVASCRIPT, Type.MICROPHONE, Type.NFC,
            Type.NOTIFICATIONS, Type.POPUPS, Type.PROTECTED_MEDIA, Type.SENSORS, Type.SOUND,
            Type.USB, Type.USE_STORAGE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        // Values used to address array index - should be enumerated from 0 and can't have gaps.
        // All updates here must also be reflected in {@link #preferenceKey(int)
        // preferenceKey} and {@link #contentSettingsType(int) contentSettingsType}.
        int ALL_SITES = 0; // Always first as it should appear in the UI at the top.
        int ADS = 1;
        int AUTOMATIC_DOWNLOADS = 2;
        int AUTOPLAY = 3;
        int BACKGROUND_SYNC = 4;
        int BLUETOOTH_SCANNING = 5;
        int CAMERA = 6;
        int CLIPBOARD = 7;
        int COOKIES = 8;
        int DEVICE_LOCATION = 9;
        int JAVASCRIPT = 10;
        int MICROPHONE = 11;
        int NFC = 12;
        int NOTIFICATIONS = 13;
        int POPUPS = 14;
        int PROTECTED_MEDIA = 15;
        int SENSORS = 16;
        int SOUND = 17;
        int USB = 18;
        int USE_STORAGE = 19; // Always last as it should appear in the UI at the bottom.
        /**
         * Number of handled categories used for calculating array sizes.
         */
        int NUM_ENTRIES = 20;
    }

    // The id of this category.
    private @Type int mCategory;

    // The id of a permission in Android M that governs this category. Can be blank if Android has
    // no equivalent permission for the category.
    private String mAndroidPermission;

    /**
     * Construct a SiteSettingsCategory.
     * @param category The string id of the category to construct.
     * @param androidPermission A string containing the id of a toggle-able permission in Android
     *        that this category represents (or blank, if Android does not expose that permission).
     */
    protected SiteSettingsCategory(@Type int category, String androidPermission) {
        mCategory = category;
        mAndroidPermission = androidPermission;
    }

    /**
     * Construct a SiteSettingsCategory from a type.
     */
    public static SiteSettingsCategory createFromType(@Type int type) {
        if (type == Type.DEVICE_LOCATION) return new LocationCategory();
        if (type == Type.NOTIFICATIONS) return new NotificationCategory();

        final String permission;
        if (type == Type.CAMERA) {
            permission = android.Manifest.permission.CAMERA;
        } else if (type == Type.MICROPHONE) {
            permission = android.Manifest.permission.RECORD_AUDIO;
        } else {
            permission = "";
        }
        return new SiteSettingsCategory(type, permission);
    }

    public static SiteSettingsCategory createFromContentSettingsType(
            @ContentSettingsType int contentSettingsType) {
        assert contentSettingsType != -1;
        assert Type.ALL_SITES == 0;
        for (@Type int i = Type.ALL_SITES; i < Type.NUM_ENTRIES; i++) {
            if (contentSettingsType(i) == contentSettingsType) return createFromType(i);
        }
        return null;
    }

    public static SiteSettingsCategory createFromPreferenceKey(String preferenceKey) {
        assert Type.ALL_SITES == 0;
        for (@Type int i = Type.ALL_SITES; i < Type.NUM_ENTRIES; i++) {
            if (preferenceKey(i).equals(preferenceKey)) return createFromType(i);
        }
        return null;
    }

    /**
     * Convert Type into {@link ContentSettingsType}
     */
    public static int contentSettingsType(@Type int type) {
        switch (type) {
            case Type.ADS:
                return ContentSettingsType.ADS;
            case Type.AUTOMATIC_DOWNLOADS:
                return ContentSettingsType.AUTOMATIC_DOWNLOADS;
            case Type.AUTOPLAY:
                return ContentSettingsType.AUTOPLAY;
            case Type.BACKGROUND_SYNC:
                return ContentSettingsType.BACKGROUND_SYNC;
            case Type.BLUETOOTH_SCANNING:
                return ContentSettingsType.BLUETOOTH_SCANNING;
            case Type.CAMERA:
                return ContentSettingsType.MEDIASTREAM_CAMERA;
            case Type.CLIPBOARD:
                return ContentSettingsType.CLIPBOARD_READ;
            case Type.COOKIES:
                return ContentSettingsType.COOKIES;
            case Type.DEVICE_LOCATION:
                return ContentSettingsType.GEOLOCATION;
            case Type.JAVASCRIPT:
                return ContentSettingsType.JAVASCRIPT;
            case Type.MICROPHONE:
                return ContentSettingsType.MEDIASTREAM_MIC;
            case Type.NFC:
                return ContentSettingsType.NFC;
            case Type.NOTIFICATIONS:
                return ContentSettingsType.NOTIFICATIONS;
            case Type.POPUPS:
                return ContentSettingsType.POPUPS;
            case Type.PROTECTED_MEDIA:
                return ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER;
            case Type.SENSORS:
                return ContentSettingsType.SENSORS;
            case Type.SOUND:
                return ContentSettingsType.SOUND;
            case Type.USB:
                return ContentSettingsType.USB_GUARD;
            // case Type.ALL_SITES
            // case Type.USE_STORAGE
            default:
                return -1; // Conversion unavailable.
        }
    }

    /**
     * Get the chooser data type {@link ContentSettingsType} corresponding to the given
     * {@link ContentSettingsType}.
     */
    public static int objectChooserDataTypeFromGuard(@ContentSettingsType int type) {
        switch (type) {
            case ContentSettingsType.USB_GUARD:
                return ContentSettingsType.USB_CHOOSER_DATA;
            default:
                return -1; // Conversion unavailable.
        }
    }

    /**
     * Convert Type into preference String
     */
    public static String preferenceKey(@Type int type) {
        switch (type) {
            case Type.ADS:
                return "ads";
            case Type.ALL_SITES:
                return "all_sites";
            case Type.AUTOMATIC_DOWNLOADS:
                return "automatic_downloads";
            case Type.AUTOPLAY:
                return "autoplay";
            case Type.BACKGROUND_SYNC:
                return "background_sync";
            case Type.BLUETOOTH_SCANNING:
                return "bluetooth_scanning";
            case Type.CAMERA:
                return "camera";
            case Type.CLIPBOARD:
                return "clipboard";
            case Type.COOKIES:
                return "cookies";
            case Type.DEVICE_LOCATION:
                return "device_location";
            case Type.JAVASCRIPT:
                return "javascript";
            case Type.MICROPHONE:
                return "microphone";
            case Type.NFC:
                return "nfc";
            case Type.NOTIFICATIONS:
                return "notifications";
            case Type.POPUPS:
                return "popups";
            case Type.PROTECTED_MEDIA:
                return "protected_content";
            case Type.SENSORS:
                return "sensors";
            case Type.SOUND:
                return "sound";
            case Type.USB:
                return "usb";
            case Type.USE_STORAGE:
                return "use_storage";
            default:
                assert false;
                return "";
        }
    }

    /**
     * Returns the {@link ContentSettingsType} for this category, or -1 if no such type exists.
     */
    public @ContentSettingsType int getContentSettingsType() {
        return contentSettingsType(mCategory);
    }

    /**
     * Returns the {@link ContentSettingsType} representing the chooser data type for this category,
     * or -1 if this category does not have a chooser data type.
     */
    public @ContentSettingsType int getObjectChooserDataType() {
        return objectChooserDataTypeFromGuard(contentSettingsType(mCategory));
    }

    /**
     * Returns whether this category is the specified type.
     */
    public boolean showSites(@Type int type) {
        return type == mCategory;
    }

    /**
     * Returns whether the Ads category is enabled via an experiment flag.
     */
    public static boolean adsCategoryEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.SUBRESOURCE_FILTER);
    }

    /**
     * Returns whether the current category is managed either by enterprise policy or by the
     * custodian of a supervised account.
     */
    public boolean isManaged() {
        if (showSites(Type.AUTOMATIC_DOWNLOADS)) {
            return WebsitePreferenceBridge.isAutomaticDownloadsManaged();
        } else if (showSites(Type.BACKGROUND_SYNC)) {
            return WebsitePreferenceBridge.isBackgroundSyncManaged();
        } else if (showSites(Type.COOKIES)) {
            return !WebsitePreferenceBridge.isAcceptCookiesUserModifiable();
        } else if (showSites(Type.DEVICE_LOCATION)) {
            return !WebsitePreferenceBridge.isAllowLocationUserModifiable();
        } else if (showSites(Type.JAVASCRIPT)) {
            return WebsitePreferenceBridge.javaScriptManaged();
        } else if (showSites(Type.CAMERA)) {
            return !WebsitePreferenceBridge.isCameraUserModifiable();
        } else if (showSites(Type.MICROPHONE)) {
            return !WebsitePreferenceBridge.isMicUserModifiable();
        } else if (showSites(Type.POPUPS)) {
            return WebsitePreferenceBridge.isPopupsManaged();
        }
        return false;
    }

    /**
     * Returns whether the current category is managed by the custodian (e.g. parent, not an
     * enterprise admin) of the account if the account is supervised.
     */
    public boolean isManagedByCustodian() {
        if (showSites(Type.COOKIES)) {
            return WebsitePreferenceBridge.isAcceptCookiesManagedByCustodian();
        } else if (showSites(Type.DEVICE_LOCATION)) {
            return WebsitePreferenceBridge.isAllowLocationManagedByCustodian();
        } else if (showSites(Type.CAMERA)) {
            return WebsitePreferenceBridge.isCameraManagedByCustodian();
        } else if (showSites(Type.MICROPHONE)) {
            return WebsitePreferenceBridge.isMicManagedByCustodian();
        }
        return false;
    }

    /**
     * Configure a preference to show when when the Android permission for this category is
     * disabled.
     * @param osWarning A preference to hold the first permission warning. After calling this
     *                  method, if osWarning has no title, the preference should not be added to the
     *                  preference screen.
     * @param osWarningExtra A preference to hold any additional permission warning (if any). After
     *                       calling this method, if osWarningExtra has no title, the preference
     *                       should not be added to the preference screen.
     * @param activity The current activity.
     * @param specificCategory Whether the warnings refer to a single category or is an aggregate
     *                         for many permissions.
     */
    public void configurePermissionIsOffPreferences(Preference osWarning, Preference osWarningExtra,
            Activity activity, boolean specificCategory) {
        Intent perAppIntent = getIntentToEnableOsPerAppPermission(activity);
        Intent globalIntent = getIntentToEnableOsGlobalPermission(activity);
        String perAppMessage = getMessageForEnablingOsPerAppPermission(activity, !specificCategory);
        String globalMessage = getMessageForEnablingOsGlobalPermission(activity);

        Resources resources = activity.getResources();
        int color = ApiCompatibilityUtils.getColor(resources, R.color.pref_accent_color);
        ForegroundColorSpan linkSpan = new ForegroundColorSpan(color);

        if (perAppIntent != null) {
            SpannableString messageWithLink = SpanApplier.applySpans(
                    perAppMessage, new SpanInfo("<link>", "</link>", linkSpan));
            osWarning.setTitle(messageWithLink);
            osWarning.setIntent(perAppIntent);

            if (!specificCategory) {
                osWarning.setIcon(getDisabledInAndroidIcon(activity));
            }
        }

        if (globalIntent != null) {
            SpannableString messageWithLink = SpanApplier.applySpans(
                    globalMessage, new SpanInfo("<link>", "</link>", linkSpan));
            osWarningExtra.setTitle(messageWithLink);
            osWarningExtra.setIntent(globalIntent);

            if (!specificCategory) {
                if (perAppIntent == null) {
                    osWarningExtra.setIcon(getDisabledInAndroidIcon(activity));
                } else {
                    Drawable transparent = new ColorDrawable(Color.TRANSPARENT);
                    osWarningExtra.setIcon(transparent);
                }
            }
        }
    }

    /**
     * Returns the icon for permissions that have been disabled by Android.
     */
    Drawable getDisabledInAndroidIcon(Activity activity) {
        Drawable icon = ApiCompatibilityUtils.getDrawable(activity.getResources(),
                R.drawable.exclamation_triangle);
        icon.mutate();
        int disabledColor = ApiCompatibilityUtils.getColor(activity.getResources(),
                R.color.pref_accent_color);
        icon.setColorFilter(disabledColor, PorterDuff.Mode.SRC_IN);
        return icon;
    }

    /**
     * Returns whether the permission is enabled in Android, both globally and per-app. If the
     * permission does not have a per-app setting or a global setting, true is assumed for either
     * that is missing (or both).
     */
    boolean enabledInAndroid(Context context) {
        return enabledGlobally() && enabledForChrome(context);
    }

    /**
     * Returns whether a permission is enabled across Android. Not all permissions can be disabled
     * globally, so the default is true, but can be overwritten in sub-classes.
     */
    protected boolean enabledGlobally() {
        return true;
    }

    /**
     * Returns whether a permission is enabled for Chrome specifically.
     */
    protected boolean enabledForChrome(Context context) {
        if (mAndroidPermission.isEmpty()) return true;
        return permissionOnInAndroid(mAndroidPermission, context);
    }

    /**
     * Returns whether to show the 'permission blocked' message. Majority of the time, that is
     * warranted when the permission is either blocked per app or globally. But there are exceptions
     * to this, so the sub-classes can overwrite.
     */
    boolean showPermissionBlockedMessage(Context context) {
        return !enabledForChrome(context) || !enabledGlobally();
    }

    /**
     * Returns the OS Intent to use to enable a per-app permission, or null if the permission is
     * already enabled. Android M and above provides two ways of doing this for some permissions,
     * most notably Location, one that is per-app and another that is global.
     */
    private Intent getIntentToEnableOsPerAppPermission(Context context) {
        if (enabledForChrome(context)) return null;
        return getAppInfoIntent(context);
    }

    /**
     * Returns the OS Intent to use to enable a permission globally, or null if there is no global
     * permission. Android M and above provides two ways of doing this for some permissions, most
     * notably Location, one that is per-app and another that is global.
     */
    protected Intent getIntentToEnableOsGlobalPermission(Context context) {
        return null;
    }

    /**
     * Returns the message to display when per-app permission is blocked.
     * @param plural Whether it applies to one per-app permission or multiple.
     */
    protected String getMessageForEnablingOsPerAppPermission(Activity activity, boolean plural) {
        @ContentSettingsType
        int type = this.getContentSettingsType();
        int permission_string = R.string.android_permission_off;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_SITE_SETTINGS_UI_REFRESH)) {
            if (type == ContentSettingsType.GEOLOCATION) {
                permission_string = R.string.android_location_permission_off;
            } else if (type == ContentSettingsType.MEDIASTREAM_CAMERA) {
                permission_string = R.string.android_camera_permission_off;
            } else if (type == ContentSettingsType.MEDIASTREAM_MIC) {
                permission_string = R.string.android_microphone_permission_off;
            } else if (type == ContentSettingsType.NOTIFICATIONS) {
                permission_string = R.string.android_notifications_permission_off;
            }
            return activity.getResources().getString(
                    plural ? R.string.android_permission_off_plural : permission_string);
        } else {
            return activity.getResources().getString(plural ? R.string.android_permission_off_plural
                                                            : R.string.android_permission_off);
        }
    }

    /**
     * Returns the message to display when per-app permission is blocked.
     */
    protected String getMessageForEnablingOsGlobalPermission(Activity activity) {
        return null;
    }

    /**
     * Returns an Intent to show the App Info page for the current app.
     */
    private Intent getAppInfoIntent(Context context) {
        Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
        intent.setData(
                new Uri.Builder().scheme("package").opaquePart(context.getPackageName()).build());
        return intent;
    }

    /**
     * Returns whether a per-app permission is enabled.
     * @param permission The string of the permission to check.
     */
    private boolean permissionOnInAndroid(String permission, Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return true;

        return PackageManager.PERMISSION_GRANTED == ApiCompatibilityUtils.checkPermission(
                context, permission, Process.myPid(), Process.myUid());
    }
}
