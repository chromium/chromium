// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Base64;

import androidx.annotation.Nullable;
import androidx.annotation.WorkerThread;
import androidx.browser.trusted.Token;

import org.chromium.base.ContextUtils;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * Stores data about origins associated with an installed webapp (TWA or WebAPK) for the purpose of
 * Permission Delegation. Primarily we store (indexed by origin):
 *
 * - A list of all apps associated with an origin.
 * - The app that will be used for delegation.
 * - The permission state of the app that will be used for delegation.
 *
 * We did not use a similar technique to
 * {@link org.chromium.chrome.browser.webapps.WebappDataStorage}, because the data backing each
 * WebappDataStore is stored in its own Preferences file, so while
 * {@link org.chromium.chrome.browser.webapps.WebappRegistry} is eagerly loaded when Chrome starts
 * up, we don't want the first permission check to cause loading separate Preferences files for
 * each installed app.
 *
 * A key difference between this class and the
 * {@link org.chromium.chrome.browser.browserservices.InstalledWebappDataRegister} is that the
 * register stores data keyed by the client app, whereas this class stores data keyed by the origin.
 * There may be two client apps installed for the same origin, the InstalledWebappDataRegister will
 * hold two entries, whereas this class will hold one entry.
 *
 * Lifecycle: This class is designed to be owned by
 * {@link org.chromium.chrome.browser.webapps.WebappRegistry}, get it from there, don't create your
 * own instance.
 * Thread safety: Is thread-safe (only operates on underlying SharedPreferences).
 * Native: Does not require native.
 *
 * TODO(peconn): Unify this and WebappDataStorage?
 */
public class InstalledWebappPermissionStore {
    private static final String SHARED_PREFS_FILE = "twa_permission_registry";

    private static final String KEY_ALL_ORIGINS = "origins";

    private static final String KEY_NOTIFICATION_PERMISSION_PREFIX = "notification_permission.";
    private static final String KEY_NOTIFICATION_PERMISSION_SETTING_PREFIX =
            "notification_permission_setting.";
    private static final String KEY_GEOLOCATION_PERMISSION_PREFIX = "geolocation_permission.";
    private static final String KEY_GEOLOCATION_PERMISSION_SETTING_PREFIX =
            "geolocation_permission_setting.";
    private static final String KEY_PACKAGE_NAME_PREFIX = "package_name.";
    private static final String KEY_APP_NAME_PREFIX = "app_name.";
    private static final String KEY_PRE_INSTALL_NOTIFICATION_PERMISSION_PREFIX =
            "pre_twa_notification_permission.";
    private static final String KEY_PRE_INSTALL_NOTIFICATION_PERMISSION_SETTING_PREFIX =
            "pre_twa_notification_permission_setting.";
    private static final String KEY_ALL_DELEGATE_APPS_PREFIX = "all_delegate_apps.";

    private final SharedPreferences mPreferences;

    /**
     * Reads the underlying storage into memory, should be called initially on a background thread.
     */
    @WorkerThread
    public void initStorage() {
        // Read some value from the Preferences to ensure it's in memory.
        getStoredOrigins();
    }

    /** Creates a {@link InstalledWebappPermissionStore}. */
    public InstalledWebappPermissionStore() {
        // On some versions of Android, creating the Preferences object involves a disk read (to
        // check if the Preferences directory exists, not even to read the actual Preferences).
        mPreferences =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(SHARED_PREFS_FILE, Context.MODE_PRIVATE);
    }

    /**
     * Retrieves the permission setting of {@link ContentSettingsType} for the origin due to
     * delegation to an app. Returns {@code null} if the origin is not linked to an app.
     */
    @Nullable
    public @ContentSettingValues Integer getPermission(
            @ContentSettingsType.EnumType int type, Origin origin) {
        String key = createPermissionSettingKey(type, origin);

        if (!mPreferences.contains(key)) {
            // TODO(crbug.com/40838462): Clean up this fallback.
            String fallbackKey = createPermissionKey(type, origin);
            if (!mPreferences.contains(fallbackKey)) return null;
            boolean enabled = mPreferences.getBoolean(fallbackKey, false);
            return enabled ? ContentSettingValues.ALLOW : ContentSettingValues.BLOCK;
        }

        return mPreferences.getInt(key, ContentSettingValues.ASK);
    }

    @Nullable
    String getDelegateAppName(Origin origin) {
        return mPreferences.getString(createAppNameKey(origin), null);
    }

    @Nullable
    String getDelegatePackageName(Origin origin) {
        return mPreferences.getString(createPackageNameKey(origin), null);
    }

    @Nullable
    Set<Token> getAllDelegateApps(Origin origin) {
        Set<String> tokens = mPreferences.getStringSet(createAllDelegateAppsKey(origin), null);
        if (tokens == null) return null;

        Set<Token> result = new HashSet<>();
        for (String tokenAsString : tokens) {
            result.add(Token.deserialize(stringToByteArray(tokenAsString)));
        }
        return result;
    }

    void addDelegateApp(Origin origin, Token token) {
        String key = createAllDelegateAppsKey(origin);
        Set<String> allDelegateApps =
                new HashSet<>(mPreferences.getStringSet(key, Collections.emptySet()));
        allDelegateApps.add(byteArrayToString(token.serialize()));
        mPreferences.edit().putStringSet(key, allDelegateApps).apply();
    }

    /** Gets all the origins of registered TWAs. */
    public Set<String> getStoredOrigins() {
        // In case the pre-emptive disk read in initStorage hasn't occurred by the time we actually
        // need the value.
        // The set returned by getStringSet must not be modified. The consistency of the stored
        // data is not guaranteed if you do, nor is your ability to modify the instance at all.
        return new HashSet<>(mPreferences.getStringSet(KEY_ALL_ORIGINS, new HashSet<>()));
    }

    /**
     * Sets the permission state for the origin. Returns whether {@code true} if state was changed,
     * {@code false} if the provided state was the same as the state beforehand.
     */
    boolean setStateForOrigin(
            Origin origin,
            String packageName,
            String appName,
            @ContentSettingsType.EnumType int type,
            @ContentSettingValues int settingValue) {
        boolean modified = !getStoredOrigins().contains(origin.toString());

        if (!modified) {
            // Don't bother with these extra checks if we have a brand new origin.
            boolean settingChanged =
                    settingValue
                            != mPreferences.getInt(
                                    createPermissionSettingKey(type, origin),
                                    ContentSettingValues.ASK);
            boolean packageChanged =
                    !packageName.equals(mPreferences.getString(createPackageNameKey(origin), null));
            boolean appNameChanged =
                    !appName.equals(mPreferences.getString(createAppNameKey(origin), null));
            modified = settingChanged || packageChanged || appNameChanged;
        }

        addOrigin(origin);

        mPreferences
                .edit()
                .putInt(createPermissionSettingKey(type, origin), settingValue)
                .putString(createPackageNameKey(origin), packageName)
                .putString(createAppNameKey(origin), appName)
                .apply();

        return modified;
    }

    /** Removes the origin from the store. */
    void removeOrigin(Origin origin) {
        Set<String> origins = getStoredOrigins();
        origins.remove(origin.toString());

        mPreferences
                .edit()
                .putStringSet(KEY_ALL_ORIGINS, origins)
                .remove(createPermissionKey(ContentSettingsType.NOTIFICATIONS, origin))
                .remove(createPermissionSettingKey(ContentSettingsType.NOTIFICATIONS, origin))
                .remove(createPermissionKey(ContentSettingsType.GEOLOCATION, origin))
                .remove(createPermissionSettingKey(ContentSettingsType.GEOLOCATION, origin))
                .remove(createAppNameKey(origin))
                .remove(createPackageNameKey(origin))
                .remove(createAllDelegateAppsKey(origin))
                .apply();
    }

    /** Reset permission {@type} from the store. */
    void resetPermission(Origin origin, @ContentSettingsType.EnumType int type) {
        mPreferences
                .edit()
                .remove(createPermissionKey(type, origin))
                .remove(createPermissionSettingKey(type, origin))
                .apply();
    }

    /** Stores the notification permission setting the origin had before the app was installed. */
    void setPreInstallNotificationPermission(
            Origin origin, @ContentSettingValues int settingValue) {
        mPreferences
                .edit()
                .putInt(createPreInstallNotificationPermissionSettingKey(origin), settingValue)
                .apply();
    }

    /**
     * Retrieves the notification permission setting the origin had before the app was installed.
     * {@code null} if no setting is stored. If a setting was stored, calling this method removes
     * it.
     */
    @Nullable
    @ContentSettingValues
    Integer getAndRemovePreInstallNotificationPermission(Origin origin) {
        String key = createPreInstallNotificationPermissionSettingKey(origin);

        if (!mPreferences.contains(key)) {
            // TODO(crbug.com/40838462): Clean up this fallback.
            String fallbackKey = createNotificationPreInstallPermissionKey(origin);
            if (!mPreferences.contains(fallbackKey)) return null;
            boolean enabled = mPreferences.getBoolean(fallbackKey, false);
            mPreferences.edit().remove(fallbackKey).apply();
            return enabled ? ContentSettingValues.ALLOW : ContentSettingValues.BLOCK;
        }

        @ContentSettingValues int settingValue = mPreferences.getInt(key, ContentSettingValues.ASK);
        mPreferences.edit().remove(key).apply();
        return settingValue;
    }

    /** Clears the store, for testing. */
    public void clearForTesting() {
        mPreferences.edit().clear().apply();
    }

    private void addOrigin(Origin origin) {
        Set<String> origins = getStoredOrigins();
        origins.add(origin.toString());

        mPreferences.edit().putStringSet(KEY_ALL_ORIGINS, origins).apply();
    }

    private static String getKeyPermissionPrefix(@ContentSettingsType.EnumType int type) {
        switch (type) {
            case ContentSettingsType.NOTIFICATIONS:
                return KEY_NOTIFICATION_PERMISSION_PREFIX;
            case ContentSettingsType.GEOLOCATION:
                return KEY_GEOLOCATION_PERMISSION_PREFIX;
            default:
                throw new IllegalStateException("Unsupported permission type.");
        }
    }

    private static String getPermissionSettingKeyPrefix(@ContentSettingsType.EnumType int type) {
        switch (type) {
            case ContentSettingsType.NOTIFICATIONS:
                return KEY_NOTIFICATION_PERMISSION_SETTING_PREFIX;
            case ContentSettingsType.GEOLOCATION:
                return KEY_GEOLOCATION_PERMISSION_SETTING_PREFIX;
            default:
                throw new IllegalStateException("Unsupported permission type.");
        }
    }

    private static String createPermissionKey(
            @ContentSettingsType.EnumType int type, Origin origin) {
        return getKeyPermissionPrefix(type) + origin.toString();
    }

    private static String createPermissionSettingKey(
            @ContentSettingsType.EnumType int type, Origin origin) {
        return getPermissionSettingKeyPrefix(type) + origin.toString();
    }

    private static String createNotificationPreInstallPermissionKey(Origin origin) {
        return KEY_PRE_INSTALL_NOTIFICATION_PERMISSION_PREFIX + origin.toString();
    }

    private static String createPreInstallNotificationPermissionSettingKey(Origin origin) {
        return KEY_PRE_INSTALL_NOTIFICATION_PERMISSION_SETTING_PREFIX + origin.toString();
    }

    private static String createPackageNameKey(Origin origin) {
        return KEY_PACKAGE_NAME_PREFIX + origin.toString();
    }

    private static String createAppNameKey(Origin origin) {
        return KEY_APP_NAME_PREFIX + origin.toString();
    }

    private static String createAllDelegateAppsKey(Origin origin) {
        return KEY_ALL_DELEGATE_APPS_PREFIX + origin.toString();
    }

    private static byte[] stringToByteArray(String string) {
        return Base64.decode(string, Base64.NO_WRAP | Base64.NO_PADDING);
    }

    private static String byteArrayToString(byte[] byteArray) {
        return Base64.encodeToString(byteArray, Base64.NO_WRAP | Base64.NO_PADDING);
    }
}
