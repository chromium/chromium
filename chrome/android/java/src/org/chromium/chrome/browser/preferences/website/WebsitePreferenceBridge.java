// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ContentSettingsType;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;

/**
 * Utility class that interacts with native to retrieve and set website settings.
 */
public class WebsitePreferenceBridge {
    /** The android permissions associated with requesting location. */
    private static final String[] LOCATION_PERMISSIONS = {
            android.Manifest.permission.ACCESS_FINE_LOCATION,
            android.Manifest.permission.ACCESS_COARSE_LOCATION};
    /** The android permissions associated with requesting access to the camera. */
    private static final String[] CAMERA_PERMISSIONS = {android.Manifest.permission.CAMERA};
    /** The android permissions associated with requesting access to the microphone. */
    private static final String[] MICROPHONE_PERMISSIONS = {
            android.Manifest.permission.RECORD_AUDIO};
    /** Signifies there are no permissions associated. */
    private static final String[] EMPTY_PERMISSIONS = {};

    /**
     * Interface for an object that listens to storage info is cleared callback.
     */
    public interface StorageInfoClearedCallback {
        @CalledByNative("StorageInfoClearedCallback")
        public void onStorageInfoCleared();
    }

    /**
     * @return the list of all origins that have permissions in non-incognito mode.
     */
    @SuppressWarnings("unchecked")
    public List<PermissionInfo> getPermissionInfo(@PermissionInfo.Type int type) {
        ArrayList<PermissionInfo> list = new ArrayList<PermissionInfo>();
        // Camera, Location & Microphone can be managed by the custodian
        // of a supervised account or by enterprise policy.
        if (type == PermissionInfo.Type.CAMERA) {
            boolean managedOnly = !isCameraUserModifiable();
            WebsitePreferenceBridgeJni.get().getCameraOrigins(list, managedOnly);
        } else if (type == PermissionInfo.Type.CLIPBOARD) {
            WebsitePreferenceBridgeJni.get().getClipboardOrigins(list);
        } else if (type == PermissionInfo.Type.GEOLOCATION) {
            boolean managedOnly = !isAllowLocationUserModifiable();
            WebsitePreferenceBridgeJni.get().getGeolocationOrigins(list, managedOnly);
        } else if (type == PermissionInfo.Type.MICROPHONE) {
            boolean managedOnly = !isMicUserModifiable();
            WebsitePreferenceBridgeJni.get().getMicrophoneOrigins(list, managedOnly);
        } else if (type == PermissionInfo.Type.MIDI) {
            WebsitePreferenceBridgeJni.get().getMidiOrigins(list);
        } else if (type == PermissionInfo.Type.NFC) {
            WebsitePreferenceBridgeJni.get().getNfcOrigins(list);
        } else if (type == PermissionInfo.Type.NOTIFICATION) {
            WebsitePreferenceBridgeJni.get().getNotificationOrigins(list);
        } else if (type == PermissionInfo.Type.PROTECTED_MEDIA_IDENTIFIER) {
            WebsitePreferenceBridgeJni.get().getProtectedMediaIdentifierOrigins(list);
        } else if (type == PermissionInfo.Type.SENSORS) {
            WebsitePreferenceBridgeJni.get().getSensorsOrigins(list);
        } else {
            assert false;
        }
        return list;
    }

    private static void insertInfoIntoList(@PermissionInfo.Type int type,
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        if (type == PermissionInfo.Type.CAMERA || type == PermissionInfo.Type.MICROPHONE) {
            for (PermissionInfo info : list) {
                if (info.getOrigin().equals(origin) && info.getEmbedder().equals(embedder)) {
                    return;
                }
            }
        }
        list.add(new PermissionInfo(type, origin, embedder, false));
    }

    @CalledByNative
    private static void insertCameraInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.CAMERA, list, origin, embedder);
    }

    @CalledByNative
    private static void insertClipboardInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.CLIPBOARD, list, origin, embedder);
    }

    @CalledByNative
    private static void insertGeolocationInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.GEOLOCATION, list, origin, embedder);
    }

    @CalledByNative
    private static void insertMicrophoneInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.MICROPHONE, list, origin, embedder);
    }

    @CalledByNative
    private static void insertMidiInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.MIDI, list, origin, embedder);
    }

    @CalledByNative
    private static void insertNfcInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.NFC, list, origin, embedder);
    }

    @CalledByNative
    private static void insertNotificationIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.NOTIFICATION, list, origin, embedder);
    }

    @CalledByNative
    private static void insertProtectedMediaIdentifierInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.PROTECTED_MEDIA_IDENTIFIER, list, origin, embedder);
    }

    @CalledByNative
    private static void insertSensorsInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.SENSORS, list, origin, embedder);
    }

    @CalledByNative
    private static void insertStorageInfoIntoList(
            ArrayList<StorageInfo> list, String host, int type, long size) {
        list.add(new StorageInfo(host, type, size));
    }

    @CalledByNative
    private static Object createStorageInfoList() {
        return new ArrayList<StorageInfo>();
    }

    @CalledByNative
    private static Object createLocalStorageInfoMap() {
        return new HashMap<String, LocalStorageInfo>();
    }

    @SuppressWarnings("unchecked")
    @CalledByNative
    private static void insertLocalStorageInfoIntoMap(
            HashMap map, String origin, long size, boolean important) {
        ((HashMap<String, LocalStorageInfo>) map)
                .put(origin, new LocalStorageInfo(origin, size, important));
    }

    public List<ContentSettingException> getContentSettingsExceptions(
            @ContentSettingsType int contentSettingsType) {
        List<ContentSettingException> exceptions = new ArrayList<>();
        WebsitePreferenceBridgeJni.get().getContentSettingsExceptions(
                contentSettingsType, exceptions);
        if (!isContentSettingManaged(contentSettingsType)) {
            return exceptions;
        }

        List<ContentSettingException> managedExceptions =
                new ArrayList<ContentSettingException>();
        for (ContentSettingException exception : exceptions) {
            if (exception.getSource().equals("policy")) {
                managedExceptions.add(exception);
            }
        }
        return managedExceptions;
    }

    public void fetchLocalStorageInfo(Callback<HashMap> callback, boolean fetchImportant) {
        WebsitePreferenceBridgeJni.get().fetchLocalStorageInfo(callback, fetchImportant);
    }

    public void fetchStorageInfo(Callback<ArrayList> callback) {
        WebsitePreferenceBridgeJni.get().fetchStorageInfo(callback);
    }

    /**
     * Returns the list of all chosen object permissions for the given ContentSettingsType.
     *
     * There will be one ChosenObjectInfo instance for each granted permission. That means that if
     * two origin/embedder pairs have permission for the same object there will be two
     * ChosenObjectInfo instances.
     */
    public List<ChosenObjectInfo> getChosenObjectInfo(
            @ContentSettingsType int contentSettingsType) {
        ArrayList<ChosenObjectInfo> list = new ArrayList<ChosenObjectInfo>();
        WebsitePreferenceBridgeJni.get().getChosenObjects(contentSettingsType, list);
        return list;
    }

    /**
     * Inserts a ChosenObjectInfo into a list.
     */
    @CalledByNative
    private static void insertChosenObjectInfoIntoList(ArrayList<ChosenObjectInfo> list,
            int contentSettingsType, String origin, String embedder, String name, String object,
            boolean isManaged) {
        list.add(new ChosenObjectInfo(
                contentSettingsType, origin, embedder, name, object, isManaged));
    }

    /**
     * Returns whether the DSE (Default Search Engine) controls the given permission the given
     * origin.
     */
    public static boolean isPermissionControlledByDSE(
            @ContentSettingsType int contentSettingsType, String origin, boolean isIncognito) {
        return WebsitePreferenceBridgeJni.get().isPermissionControlledByDSE(
                contentSettingsType, origin, isIncognito);
    }

    /**
     * Returns whether this origin is activated for ad blocking, and will have resources blocked
     * unless they are explicitly allowed via a permission.
     */
    public static boolean getAdBlockingActivated(String origin) {
        return WebsitePreferenceBridgeJni.get().getAdBlockingActivated(origin);
    }

    /**
     * Return the list of android permission strings for a given {@link ContentSettingsType}.  If
     * there is no permissions associated with the content setting, then an empty array is returned.
     *
     * @param contentSettingType The content setting to get the android permission for.
     * @return The android permissions for the given content setting.
     */
    @CalledByNative
    public static String[] getAndroidPermissionsForContentSetting(int contentSettingType) {
        switch (contentSettingType) {
            case ContentSettingsType.GEOLOCATION:
                return Arrays.copyOf(LOCATION_PERMISSIONS, LOCATION_PERMISSIONS.length);
            case ContentSettingsType.MEDIASTREAM_MIC:
                return Arrays.copyOf(MICROPHONE_PERMISSIONS, MICROPHONE_PERMISSIONS.length);
            case ContentSettingsType.MEDIASTREAM_CAMERA:
                return Arrays.copyOf(CAMERA_PERMISSIONS, CAMERA_PERMISSIONS.length);
            default:
                return EMPTY_PERMISSIONS;
        }
    }

    @CalledByNative
    private static void addContentSettingExceptionToList(ArrayList<ContentSettingException> list,
            int contentSettingsType, String pattern, int contentSetting, String source) {
        ContentSettingException exception =
                new ContentSettingException(contentSettingsType, pattern, contentSetting, source);
        list.add(exception);
    }

    /**
     * Returns whether a particular content setting type is enabled.
     * @param contentSettingsType The content setting type to check.
     */
    public static boolean isContentSettingEnabled(int contentSettingsType) {
        return WebsitePreferenceBridgeJni.get().isContentSettingEnabled(contentSettingsType);
    }

    /**
     * @return Whether a particular content setting type is managed by policy.
     * @param contentSettingsType The content setting type to check.
     */
    public static boolean isContentSettingManaged(int contentSettingsType) {
        return WebsitePreferenceBridgeJni.get().isContentSettingManaged(contentSettingsType);
    }

    /**
     * Sets a default value for content setting type.
     * @param contentSettingsType The content setting type to check.
     * @param enabled Whether the default value should be disabled or enabled.
     */
    public static void setContentSettingEnabled(int contentSettingsType, boolean enabled) {
        WebsitePreferenceBridgeJni.get().setContentSettingEnabled(contentSettingsType, enabled);
    }

    /**
     * @return Whether JavaScript is managed by policy.
     */
    public static boolean javaScriptManaged() {
        return isContentSettingManaged(ContentSettingsType.JAVASCRIPT);
    }

    /**
     * @return true if background sync is managed by policy.
     */
    public static boolean isBackgroundSyncManaged() {
        return isContentSettingManaged(ContentSettingsType.BACKGROUND_SYNC);
    }

    /**
     * @return true if automatic downloads is managed by policy.
     */
    public static boolean isAutomaticDownloadsManaged() {
        return isContentSettingManaged(ContentSettingsType.AUTOMATIC_DOWNLOADS);
    }

    /**
     * @return Whether the setting to allow popups is configured by policy
     */
    public static boolean isPopupsManaged() {
        return isContentSettingManaged(ContentSettingsType.POPUPS);
    }

    /**
     * Whether the setting type requires tri-state (Allowed/Ask/Blocked) setting.
     */
    public static boolean requiresTriStateContentSetting(int contentSettingsType) {
        switch (contentSettingsType) {
            case ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER:
                return true;
            default:
                return false;
        }
    }

    /**
     * Sets the preferences on whether to enable/disable given setting.
     */
    public static void setCategoryEnabled(int contentSettingsType, boolean allow) {
        assert !requiresTriStateContentSetting(contentSettingsType);

        switch (contentSettingsType) {
            case ContentSettingsType.ADS:
            case ContentSettingsType.BLUETOOTH_SCANNING:
            case ContentSettingsType.JAVASCRIPT:
            case ContentSettingsType.POPUPS:
            case ContentSettingsType.USB_GUARD:
                setContentSettingEnabled(contentSettingsType, allow);
                break;
            case ContentSettingsType.AUTOMATIC_DOWNLOADS:
                WebsitePreferenceBridgeJni.get().setAutomaticDownloadsEnabled(allow);
                break;
            case ContentSettingsType.AUTOPLAY:
                WebsitePreferenceBridgeJni.get().setAutoplayEnabled(allow);
                break;
            case ContentSettingsType.BACKGROUND_SYNC:
                WebsitePreferenceBridgeJni.get().setBackgroundSyncEnabled(allow);
                break;
            case ContentSettingsType.CLIPBOARD_READ:
                WebsitePreferenceBridgeJni.get().setClipboardEnabled(allow);
                break;
            case ContentSettingsType.COOKIES:
                WebsitePreferenceBridgeJni.get().setAllowCookiesEnabled(allow);
                break;
            case ContentSettingsType.GEOLOCATION:
                WebsitePreferenceBridgeJni.get().setAllowLocationEnabled(allow);
                break;
            case ContentSettingsType.MEDIASTREAM_CAMERA:
                WebsitePreferenceBridgeJni.get().setCameraEnabled(allow);
                break;
            case ContentSettingsType.MEDIASTREAM_MIC:
                WebsitePreferenceBridgeJni.get().setMicEnabled(allow);
                break;
            case ContentSettingsType.NFC:
                WebsitePreferenceBridgeJni.get().setNfcEnabled(allow);
                break;
            case ContentSettingsType.NOTIFICATIONS:
                WebsitePreferenceBridgeJni.get().setNotificationsEnabled(allow);
                break;
            case ContentSettingsType.SENSORS:
                WebsitePreferenceBridgeJni.get().setSensorsEnabled(allow);
                break;
            case ContentSettingsType.SOUND:
                WebsitePreferenceBridgeJni.get().setSoundEnabled(allow);
                break;
            default:
                assert false;
        }
    }

    public static boolean isCategoryEnabled(int contentSettingsType) {
        assert !requiresTriStateContentSetting(contentSettingsType);

        switch (contentSettingsType) {
            case ContentSettingsType.ADS:
            case ContentSettingsType.CLIPBOARD_READ:
                // Returns true if JavaScript is enabled. It may return the temporary value set by
                // {@link #setJavaScriptEnabled}. The default is true.
            case ContentSettingsType.JAVASCRIPT:
            case ContentSettingsType.POPUPS:
                // Returns true if websites are allowed to request permission to access USB devices.
            case ContentSettingsType.USB_GUARD:
            case ContentSettingsType.BLUETOOTH_SCANNING:
                return isContentSettingEnabled(contentSettingsType);
            case ContentSettingsType.AUTOMATIC_DOWNLOADS:
                return WebsitePreferenceBridgeJni.get().getAutomaticDownloadsEnabled();
            case ContentSettingsType.AUTOPLAY:
                return WebsitePreferenceBridgeJni.get().getAutoplayEnabled();
            case ContentSettingsType.BACKGROUND_SYNC:
                return WebsitePreferenceBridgeJni.get().getBackgroundSyncEnabled();
            case ContentSettingsType.COOKIES:
                return WebsitePreferenceBridgeJni.get().getAcceptCookiesEnabled();
            case ContentSettingsType.MEDIASTREAM_CAMERA:
                return WebsitePreferenceBridgeJni.get().getCameraEnabled();
            case ContentSettingsType.MEDIASTREAM_MIC:
                return WebsitePreferenceBridgeJni.get().getMicEnabled();
            case ContentSettingsType.NFC:
                return WebsitePreferenceBridgeJni.get().getNfcEnabled();
            case ContentSettingsType.NOTIFICATIONS:
                return WebsitePreferenceBridgeJni.get().getNotificationsEnabled();
            case ContentSettingsType.SENSORS:
                return WebsitePreferenceBridgeJni.get().getSensorsEnabled();
            case ContentSettingsType.SOUND:
                return WebsitePreferenceBridgeJni.get().getSoundEnabled();
            default:
                assert false;
                return false;
        }
    }

    /**
     * Gets the ContentSetting for a settings type. Should only be used for more
     * complex settings where a binary on/off value is not sufficient.
     * Otherwise, use isCategoryEnabled() above.
     * @param contentSettingsType The settings type to get setting for.
     * @return The ContentSetting for |contentSettingsType|.
     */
    public static int getContentSetting(int contentSettingsType) {
        return WebsitePreferenceBridgeJni.get().getContentSetting(contentSettingsType);
    }

    /**
     * @param setting New ContentSetting to set for |contentSettingsType|.
     */
    public static void setContentSetting(int contentSettingsType, int setting) {
        WebsitePreferenceBridgeJni.get().setContentSetting(contentSettingsType, setting);
    }

    /**
     * @return Whether cookies acceptance is modifiable by the user
     */
    public static boolean isAcceptCookiesUserModifiable() {
        return WebsitePreferenceBridgeJni.get().getAcceptCookiesUserModifiable();
    }

    /**
     * @return Whether cookies acceptance is configured by the user's custodian
     * (for supervised users).
     */
    public static boolean isAcceptCookiesManagedByCustodian() {
        return WebsitePreferenceBridgeJni.get().getAcceptCookiesManagedByCustodian();
    }

    /**
     * @return Whether geolocation information can be shared with content.
     */
    public static boolean isAllowLocationEnabled() {
        return WebsitePreferenceBridgeJni.get().getAllowLocationEnabled();
    }

    /**
     * @return Whether geolocation information access is set to be shared with all sites, by policy.
     */
    public static boolean isLocationAllowedByPolicy() {
        return WebsitePreferenceBridgeJni.get().getLocationAllowedByPolicy();
    }

    /**
     * @return Whether the location preference is modifiable by the user.
     */
    public static boolean isAllowLocationUserModifiable() {
        return WebsitePreferenceBridgeJni.get().getAllowLocationUserModifiable();
    }

    /**
     * @return Whether the location preference is
     * being managed by the custodian of the supervised account.
     */
    public static boolean isAllowLocationManagedByCustodian() {
        return WebsitePreferenceBridgeJni.get().getAllowLocationManagedByCustodian();
    }

    /**
     * @return Whether the camera/microphone permission is managed
     * by the custodian of the supervised account.
     */
    public static boolean isCameraManagedByCustodian() {
        return WebsitePreferenceBridgeJni.get().getCameraManagedByCustodian();
    }

    /**
     * @return Whether the camera permission is editable by the user.
     */
    public static boolean isCameraUserModifiable() {
        return WebsitePreferenceBridgeJni.get().getCameraUserModifiable();
    }

    /**
     * @return Whether the microphone permission is managed by the custodian of
     * the supervised account.
     */
    public static boolean isMicManagedByCustodian() {
        return WebsitePreferenceBridgeJni.get().getMicManagedByCustodian();
    }

    /**
     * @return Whether the microphone permission is editable by the user.
     */
    public static boolean isMicUserModifiable() {
        return WebsitePreferenceBridgeJni.get().getMicUserModifiable();
    }

    public static void setContentSettingForPattern(
            int contentSettingType, String pattern, int setting) {
        WebsitePreferenceBridgeJni.get().setContentSettingForPattern(
                contentSettingType, pattern, setting);
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        void getCameraOrigins(Object list, boolean managedOnly);
        void getClipboardOrigins(Object list);
        void getGeolocationOrigins(Object list, boolean managedOnly);
        void getMicrophoneOrigins(Object list, boolean managedOnly);
        void getMidiOrigins(Object list);
        void getNotificationOrigins(Object list);
        void getNfcOrigins(Object list);
        void getProtectedMediaIdentifierOrigins(Object list);
        boolean getNfcEnabled();
        void getSensorsOrigins(Object list);
        int getCameraSettingForOrigin(String origin, String embedder, boolean isIncognito);
        int getClipboardSettingForOrigin(String origin, boolean isIncognito);
        int getGeolocationSettingForOrigin(String origin, String embedder, boolean isIncognito);
        int getMicrophoneSettingForOrigin(String origin, String embedder, boolean isIncognito);
        int getMidiSettingForOrigin(String origin, String embedder, boolean isIncognito);
        int getNfcSettingForOrigin(String origin, String embedder, boolean isIncognito);
        int getNotificationSettingForOrigin(String origin, boolean isIncognito);
        int getProtectedMediaIdentifierSettingForOrigin(
                String origin, String embedder, boolean isIncognito);
        int getSensorsSettingForOrigin(String origin, String embedder, boolean isIncognito);
        void setCameraSettingForOrigin(String origin, int value, boolean isIncognito);
        void setClipboardSettingForOrigin(String origin, int value, boolean isIncognito);
        void setGeolocationSettingForOrigin(
                String origin, String embedder, int value, boolean isIncognito);
        void setMicrophoneSettingForOrigin(String origin, int value, boolean isIncognito);
        void setMidiSettingForOrigin(
                String origin, String embedder, int value, boolean isIncognito);
        void setNfcSettingForOrigin(String origin, String embedder, int value, boolean isIncognito);
        void setNotificationSettingForOrigin(String origin, int value, boolean isIncognito);
        void reportNotificationRevokedForOrigin(
                String origin, int newSettingValue, boolean isIncognito);
        void setProtectedMediaIdentifierSettingForOrigin(
                String origin, String embedder, int value, boolean isIncognito);
        void setSensorsSettingForOrigin(
                String origin, String embedder, int value, boolean isIncognito);
        void clearBannerData(String origin);
        void clearMediaLicenses(String origin);
        void clearCookieData(String path);
        void clearLocalStorageData(String path, Object callback);
        void clearStorageData(String origin, int type, Object callback);
        void getChosenObjects(@ContentSettingsType int type, Object list);
        void resetNotificationsSettingsForTest();
        void revokeObjectPermission(
                @ContentSettingsType int type, String origin, String embedder, String object);
        boolean isContentSettingsPatternValid(String pattern);
        boolean urlMatchesContentSettingsPattern(String url, String pattern);
        void fetchStorageInfo(Object callback);
        void fetchLocalStorageInfo(Object callback, boolean includeImportant);
        boolean isPermissionControlledByDSE(
                @ContentSettingsType int contentSettingsType, String origin, boolean isIncognito);
        boolean getAdBlockingActivated(String origin);
        boolean isContentSettingEnabled(int contentSettingType);
        boolean isContentSettingManaged(int contentSettingType);
        void setContentSettingEnabled(int contentSettingType, boolean allow);
        void getContentSettingsExceptions(
                int contentSettingsType, List<ContentSettingException> list);
        void setContentSettingForPattern(int contentSettingType, String pattern, int setting);
        int getContentSetting(int contentSettingType);
        void setContentSetting(int contentSettingType, int setting);
        boolean getAcceptCookiesEnabled();
        boolean getAcceptCookiesUserModifiable();
        boolean getAcceptCookiesManagedByCustodian();
        boolean getAutomaticDownloadsEnabled();
        boolean getAutoplayEnabled();
        boolean getBackgroundSyncEnabled();
        boolean getAllowLocationUserModifiable();
        boolean getLocationAllowedByPolicy();
        boolean getAllowLocationManagedByCustodian();
        boolean getCameraEnabled();
        void setCameraEnabled(boolean enabled);
        boolean getCameraUserModifiable();
        boolean getCameraManagedByCustodian();
        boolean getMicEnabled();
        void setMicEnabled(boolean enabled);
        boolean getMicUserModifiable();
        boolean getMicManagedByCustodian();
        boolean getSensorsEnabled();
        boolean getSoundEnabled();
        void setAutomaticDownloadsEnabled(boolean enabled);
        void setAutoplayEnabled(boolean enabled);
        void setAllowCookiesEnabled(boolean enabled);
        void setBackgroundSyncEnabled(boolean enabled);
        void setClipboardEnabled(boolean enabled);
        boolean getAllowLocationEnabled();
        boolean getNotificationsEnabled();
        void setAllowLocationEnabled(boolean enabled);
        void setNotificationsEnabled(boolean enabled);
        void setNfcEnabled(boolean enabled);
        void setSensorsEnabled(boolean enabled);
        void setSoundEnabled(boolean enabled);
    }
}
