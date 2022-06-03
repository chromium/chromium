// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Base64;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.annotation.WorkerThread;
import androidx.browser.trusted.Token;

import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * Stores data about origins associated with a Trusted Web Activity for the purpose of Permission
 * Delegation. Primarily we store (indexed by origin):
 *
 * - A list of all TWAs associated with an origin.
 * - The TWA that will be used for delegation.
 * - The permission state of the TWA that will be used for delegation.
 *
 * We did not use a similar technique to
 * {@link org.chromium.chrome.browser.webapps.WebappDataStorage}, because the data backing each
 * WebappDataStore is stored in its own Preferences file, so while
 * {@link org.chromium.chrome.browser.webapps.WebappRegistry} is eagerly loaded when Chrome starts
 * up, we don't want the first permission check to cause loading separate Preferences files for
 * each installed TWA.
 *
 * A key difference between this class and the
 * {@link org.chromium.chrome.browser.browserservices.ClientAppDataRegister} is that the register
 * stores data keyed by the client app, whereas this class stores data keyed by the origin. There
 * may be two client apps installed for the same origin, the ClientAppDataRegister will hold two
 * entries, whereas this class will hold one entry.
 *
 * Lifecycle: This class is designed to be owned by
 * {@link org.chromium.chrome.browser.webapps.WebappRegistry}, get it from there, don't create your
 * own instance.
 * Thread safety: Is thread-safe (only operates on underlying SharedPreferences).
 * Native: Does not require native.
 *
 * TODO(peconn): Unify this and WebappDataStorage?
 */
public class TrustedWebActivityPermissionStore {
    private static final String SHARED_PREFS_FILE = "twa_permission_registry";

    private static final String KEY_ALL_ORIGINS = "origins";

    private static final String KEY_NOTIFICATION_PERMISSION_PREFIX = "notification_permission.";
    private static final String KEY_GEOLOCATION_PERMISSION_PREFIX = "geolocation_permission.";
    private static final String KEY_PACKAGE_NAME_PREFIX = "package_name.";
    private static final String KEY_APP_NAME_PREFIX = "app_name.";
    private static final String KEY_PRE_TWA_NOTIFICATION_PERMISSION_PREFIX
            = "pre_twa_notification_permission.";
    private static final String KEY_ALL_DELEGATE_APPS = "all_delegate_apps.";

    private final SharedPreferences mPreferences;

    /**
     * Reads the underlying storage into memory, should be called initially on a background thread.
     */
    @WorkerThread
    public void initStorage() {
        // Read some value from the Preferences to ensure it's in memory.
        getStoredOrigins();
    }

    /** Creates a {@link TrustedWebActivityPermissionStore}. */
    public TrustedWebActivityPermissionStore() {
        // On some versions of Android, creating the Preferences object involves a disk read (to
        // check if the Preferences directory exists, not even to read the actual Preferences).
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            mPreferences = ContextUtils.getApplicationContext().getSharedPreferences(
                    SHARED_PREFS_FILE, Context.MODE_PRIVATE);
        }
    }

    /**
     * Whether permission of {@link ContentSettingsType} for that origin should be enabled due to a
     * TWA. {@code null} if given origin is not linked to a TWA.
     */
    public Boolean arePermissionEnabled(@ContentSettingsType int type, Origin origin) {
        String key = createPermissionKey(type, origin);
        if (!mPreferences.contains(key)) return null;
        return mPreferences.getBoolean(key, false);
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
        Set<String> tokens =
                mPreferences.getStringSet(createAllDelegateAppsKey(origin), null);
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
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            // The set returned by getStringSet cannot be modified.
            return new HashSet<>(mPreferences.getStringSet(KEY_ALL_ORIGINS, new HashSet<>()));
        }
    }

    /** Returns true if there's a registered TWA for the origin. */
    public boolean isTwaInstalled(String origin) {
        return getStoredOrigins().contains(origin);
    }

    /**
     * Sets the permission state for the origin.
     * Returns whether {@code true} if state was changed, {@code false} if the provided state was
     * the same as the state beforehand.
     */
    boolean setStateForOrigin(Origin origin, String packageName, String appName,
            @ContentSettingsType int type, boolean enabled) {
        boolean modified = !getStoredOrigins().contains(origin.toString());

        if (!modified) {
            // Don't bother with these extra checks if we have a brand new origin.
            boolean enabledChanged =
                    enabled != mPreferences.getBoolean(createPermissionKey(type, origin), false);
            boolean packageChanged = !packageName.equals(
                    mPreferences.getString(createPackageNameKey(origin), null));
            boolean appNameChanged = !appName.equals(
                    mPreferences.getString(createAppNameKey(origin), null));
            modified = enabledChanged || packageChanged || appNameChanged;
        }

        addOrigin(origin);

        mPreferences.edit()
                .putBoolean(createPermissionKey(type, origin), enabled)
                .putString(createPackageNameKey(origin), packageName)
                .putString(createAppNameKey(origin), appName)
                .apply();

        return modified;
    }

    /** Removes the origin from the store. */
    void removeOrigin(Origin origin) {
        Set<String> origins = getStoredOrigins();
        origins.remove(origin.toString());

        mPreferences.edit()
                .putStringSet(KEY_ALL_ORIGINS, origins)
                .remove(createPermissionKey(ContentSettingsType.NOTIFICATIONS, origin))
                .remove(createPermissionKey(ContentSettingsType.GEOLOCATION, origin))
                .remove(createAppNameKey(origin))
                .remove(createPackageNameKey(origin))
                .remove(createAllDelegateAppsKey(origin))
                .apply();
    }

    /** Reset permission {@type} from the store. */
    void resetPermission(Origin origin, @ContentSettingsType int type) {
        mPreferences.edit().remove(createPermissionKey(type, origin)).apply();
    }

    /** Stores the notification state the origin had before the TWA was installed. */
    void setPreTwaNotificationState(Origin origin, boolean enabled) {
        mPreferences.edit()
                .putBoolean(createNotificationPreTwaPermissionKey(origin), enabled)
                .apply();
    }

    /**
     * Retrieves the notification state the origin had before the TWA was installed. {@code null} if
     * no state is stored. If a value was stored, calling this method removes it.
     */
    @Nullable
    Boolean getPreTwaNotificationState(Origin origin) {
        String key = createNotificationPreTwaPermissionKey(origin);
        if (!mPreferences.contains(key)) return null;

        boolean enabled = mPreferences.getBoolean(key, false);

        mPreferences.edit().remove(key).apply();

        return enabled;
    }

    /** Clears the store, for testing. */
    @VisibleForTesting
    public void clearForTesting() {
        mPreferences.edit().clear().apply();
    }

    private void addOrigin(Origin origin) {
        Set<String> origins = getStoredOrigins();
        origins.add(origin.toString());

        mPreferences.edit()
                .putStringSet(KEY_ALL_ORIGINS, origins)
                .apply();
    }

    private String getKeyPermissionPrefix(@ContentSettingsType int type) {
        switch (type) {
            case ContentSettingsType.NOTIFICATIONS:
                return KEY_NOTIFICATION_PERMISSION_PREFIX;
            case ContentSettingsType.GEOLOCATION:
                return KEY_GEOLOCATION_PERMISSION_PREFIX;
            default:
                throw new IllegalStateException("Unsupported permission type.");
        }
    }

    private String createPermissionKey(@ContentSettingsType int type, Origin origin) {
        return getKeyPermissionPrefix(type) + origin.toString();
    }

    private String createNotificationPreTwaPermissionKey(Origin origin) {
        return KEY_PRE_TWA_NOTIFICATION_PERMISSION_PREFIX + origin.toString();
    }

    private String createPackageNameKey(Origin origin) {
        return KEY_PACKAGE_NAME_PREFIX + origin.toString();
    }

    private String createAppNameKey(Origin origin) {
        return KEY_APP_NAME_PREFIX + origin.toString();
    }

    private String createAllDelegateAppsKey(Origin origin) {
        return KEY_ALL_DELEGATE_APPS + origin.toString();
    }

    private static byte[] stringToByteArray(String string) {
        return Base64.decode(string, Base64.NO_WRAP | Base64.NO_PADDING);
    }

    private static String byteArrayToString(byte[] byteArray) {
        return Base64.encodeToString(byteArray, Base64.NO_WRAP | Base64.NO_PADDING);
    }
}
