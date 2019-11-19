// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.annotation.WorkerThread;

import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.chrome.browser.browserservices.Origin;

import java.util.HashSet;
import java.util.Set;

/**
 * Stores cached data about the permissions (currently just the notification permission) of
 * installed Trusted Web Activity Client Apps. This is used to determine what permissions to give
 * to the associated websites. TWAs are indexed by their associated Origins.
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
 * stores data keyed by the client app, where as this class stores data keyed by the origin. There
 * may be two client apps installed for the same origin, the ClientAppDataRegister will hold two
 * entries, whereas this class will hold data for one client app that will be used for permission
 * delegation.
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
    private static final String KEY_PACKAGE_NAME_PREFIX = "package_name.";
    private static final String KEY_APP_NAME_PREFIX = "app_name.";
    private static final String KEY_PRE_TWA_NOTIFICATION_PERMISSION_PREFIX
            = "pre_twa_notification_permission.";

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
     * Whether notifications for that origin should be enabled due to a TWA. {@code null} if
     * given origin is not linked to a TWA.
     */
    public Boolean areNotificationsEnabled(Origin origin) {
        String key = createNotificationPermissionKey(origin);
        if (!mPreferences.contains(key)) return null;
        return mPreferences.getBoolean(key, false);
    }

    @Nullable
    String getAppName(Origin origin) {
        return mPreferences.getString(createAppNameKey(origin), null);
    }

    @Nullable
    String getPackageName(Origin origin) {
        return mPreferences.getString(createPackageNameKey(origin), null);
    }

    /** Gets all the origins of registered TWAs. */
    Set<String> getStoredOrigins() {
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
     * Sets the notification state for the origin.
     * Returns whether {@code true} if state was changed, {@code false} if the provided state was
     * the same as the state beforehand.
     */
    boolean setStateForOrigin(Origin origin, String packageName, String appName, boolean enabled) {
        boolean modified = !getStoredOrigins().contains(origin.toString());

        if (!modified) {
            // Don't bother with these extra checks if we have a brand new origin.
            boolean enabledChanged = enabled !=
                    mPreferences.getBoolean(createNotificationPermissionKey(origin), false);
            boolean packageChanged = !packageName.equals(
                    mPreferences.getString(createPackageNameKey(origin), null));
            boolean appNameChanged = !appName.equals(
                    mPreferences.getString(createAppNameKey(origin), null));
            modified = enabledChanged || packageChanged || appNameChanged;
        }

        addOrigin(origin);

        mPreferences.edit()
                .putBoolean(createNotificationPermissionKey(origin), enabled)
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
                .remove(createNotificationPermissionKey(origin))
                .remove(createAppNameKey(origin))
                .remove(createPackageNameKey(origin))
                .apply();
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

    private String createNotificationPermissionKey(Origin origin) {
        return KEY_NOTIFICATION_PERMISSION_PREFIX + origin.toString();
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
}
