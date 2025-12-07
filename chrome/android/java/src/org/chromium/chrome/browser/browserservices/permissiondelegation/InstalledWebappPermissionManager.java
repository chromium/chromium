// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static android.Manifest.permission.ACCESS_COARSE_LOCATION;
import static android.Manifest.permission.ACCESS_FINE_LOCATION;
import static android.Manifest.permission.READ_CONTACTS;

import static org.chromium.components.permissions.PermissionUtil.getGeolocationType;

import android.app.Activity;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.UiThread;
import androidx.annotation.VisibleForTesting;
import androidx.browser.trusted.Token;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * Handles preserving and surfacing the permissions of installed webapps (TWAs and WebAPKs) for
 * their associated websites. Communicates with the {@link InstalledWebappPermissionStore} and
 * {@link InstalledWebappBridge}.
 *
 * <p>Lifecycle: This is a singleton. Thread safety: Only call methods on the UI thread as this
 * class may call into native. Native: Does not require native.
 */
@NullMarked
public class InstalledWebappPermissionManager {
    private static final String TAG = "PermissionManager";

    private InstalledWebappPermissionManager() {}

    private static InstalledWebappPermissionStore getStore() {
        return WebappRegistry.getInstance().getPermissionStore();
    }

    static boolean isRunningTwa() {
        CustomTabActivity customTabActivity = getLastTrackedFocusedTwaCustomTabActivity();
        return customTabActivity != null;
    }

    static InstalledWebappBridge.Permission[] getPermissions(
            @ContentSettingsType.EnumType int type) {
        if (type == getGeolocationType()) {
            if (!isRunningTwa()) {
                return new InstalledWebappBridge.Permission[0];
            }
        }

        List<InstalledWebappBridge.Permission> permissions = new ArrayList<>();
        for (String originAsString : getStore().getStoredOrigins()) {
            Origin origin = Origin.create(originAsString);
            assert origin != null
                    : "Found unparsable Origins in the Permission Store : " + originAsString;
            if (origin == null) continue;

            @ContentSetting int setting = getPermission(type, origin);

            if (setting != ContentSetting.DEFAULT) {
                permissions.add(new InstalledWebappBridge.Permission(origin, setting));
            }
        }

        return permissions.toArray(new InstalledWebappBridge.Permission[permissions.size()]);
    }

    @UiThread
    public static void addDelegateApp(Origin origin, String packageName) {
        Token token =
                Token.create(packageName, ContextUtils.getApplicationContext().getPackageManager());
        if (token == null) return;
        getStore().addDelegateApp(origin, token);
    }

    @UiThread
    public static @Nullable Set<Token> getAllDelegateApps(Origin origin) {
        return getStore().getAllDelegateApps(origin);
    }

    @UiThread
    public static void updatePermission(
            Origin origin,
            @Nullable String packageName,
            @ContentSettingsType.EnumType int type,
            @ContentSetting int settingValue) {
        if (packageName == null) return;

        String appName = getAppNameForPackage(packageName);
        if (appName == null) return;

        // It's important that we set the state before we destroy the notification channel. If we
        // did it the other way around there'd be a small moment in time where the website's
        // notification permission could flicker from SET -> UNSET -> SET. This way we transition
        // straight from the channel's permission to the app's permission.
        boolean stateChanged =
                getStore().setStateForOrigin(origin, packageName, appName, type, settingValue);

        if (type == ContentSettingsType.NOTIFICATIONS) {
            NotificationChannelPreserver.deleteChannelIfNeeded(origin);
        }

        if (stateChanged) {
            InstalledWebappBridge.notifyPermissionsChange(type);
        }
    }

    @UiThread
    static void unregister(Origin origin) {
        getStore().removeOrigin(origin);

        NotificationChannelPreserver.restoreChannelIfNeeded(origin);

        InstalledWebappBridge.notifyPermissionsChange(ContentSettingsType.NOTIFICATIONS);
        InstalledWebappBridge.notifyPermissionsChange(ContentSettingsType.GEOLOCATION);
        InstalledWebappBridge.notifyPermissionsChange(ContentSettingsType.GEOLOCATION_WITH_OPTIONS);
    }

    @UiThread
    static void resetStoredPermission(Origin origin, @ContentSettingsType.EnumType int type) {
        getStore().resetPermission(origin, type);
        InstalledWebappBridge.notifyPermissionsChange(type);
    }

    /**
     * Returns the user visible name of the app that will handle permission delegation for the
     * origin.
     */
    public static @Nullable String getDelegateAppName(Origin origin) {
        return getStore().getDelegateAppName(origin);
    }

    /** Returns the package of the app that will handle permission delegation for the origin. */
    public static @Nullable String getDelegatePackageName(Origin origin) {
        return getStore().getDelegatePackageName(origin);
    }

    /** Gets all the origins that we delegate permissions for. */
    public static Set<String> getAllDelegatedOrigins() {
        return getStore().getStoredOrigins();
    }

    private static @Nullable String getAppNameForPackage(String packageName) {
        // TODO(peconn): Dedupe logic with InstalledWebappDataRecorder.
        try {
            PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
            int getAppInfoFlags = 0;
            ApplicationInfo ai = pm.getApplicationInfo(packageName, getAppInfoFlags);

            String appLabel = pm.getApplicationLabel(ai).toString();

            if (TextUtils.isEmpty(appLabel)) {
                Log.e(TAG, "Invalid details for client package: %s", appLabel);
                return null;
            }

            return appLabel;
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Couldn't find name for client package: %s", packageName);
            return null;
        }
    }

    @VisibleForTesting
    @ContentSetting
    static int getPermission(@ContentSettingsType.EnumType int type, Origin origin) {
        switch (type) {
            case ContentSettingsType.NOTIFICATIONS:
                {
                    @ContentSetting Integer settingValue = getStore().getPermission(type, origin);
                    if (settingValue == null) {
                        Log.w(TAG, "Origin %s is known but has no permission set.", origin);
                        break;
                    }
                    return settingValue;
                }
            case ContentSettingsType.GEOLOCATION:
            case ContentSettingsType.GEOLOCATION_WITH_OPTIONS:
                {
                    String packageName = getDelegatePackageName(origin);
                    Boolean enabled = hasAndroidLocationPermission(packageName);

                    // Skip if the delegated app did not enable location delegation.
                    if (enabled == null) break;

                    @ContentSetting
                    Integer storedPermission = getStore().getPermission(type, origin);

                    // Return |ASK| if is the first time (no previous state), and is not enabled.
                    if (storedPermission == null && !enabled) return ContentSetting.ASK;

                    // This is a temperate solution for the new Android one-time permission. Since
                    // we are not able to detect if use is changing the setting to "ask every
                    // time", when there is no permission, return ASK to let the client app decide
                    // whether to show the prompt.
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                        if (!enabled) return ContentSetting.ASK;
                    }

                    @ContentSetting
                    int settingValue = enabled ? ContentSetting.ALLOW : ContentSetting.BLOCK;

                    updatePermission(origin, packageName, getGeolocationType(), settingValue);

                    return settingValue;
                }
        }
        return ContentSetting.DEFAULT;
    }

    /**
     * Returns whether the delegate application for the origin has Android contacts permission, or
     * {@code null} if it does not exist or did not request contacts permission.
     */
    public static @Nullable Boolean hasAndroidContactsPermission(@Nullable String packageName) {
        return hasAndroidPermissions(packageName, new String[] {READ_CONTACTS});
    }

    /**
     * Returns whether the delegate application for the origin has Android location permission, or
     * {@code null} if it does not exist or did not request location permission.
     */
    public static @Nullable Boolean hasAndroidLocationPermission(@Nullable String packageName) {
        return hasAndroidPermissions(
                packageName, new String[] {ACCESS_COARSE_LOCATION, ACCESS_FINE_LOCATION});
    }

    /**
     * Returns whether the delegate application for the origin has any of specific Android
     * permissions, or {@code null} if it does not exist or did not request those permissions.
     */
    public static @Nullable Boolean hasAndroidPermissions(
            @Nullable String packageName, String[] permissions) {
        if (packageName == null) return null;

        try {
            PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
            PackageInfo packageInfo =
                    pm.getPackageInfo(packageName, PackageManager.GET_PERMISSIONS);

            String[] requestedPermissions = packageInfo.requestedPermissions;
            int[] requestedPermissionsFlags = packageInfo.requestedPermissionsFlags;

            if (requestedPermissions == null) {
                return null;
            }

            boolean requested = false;
            for (int i = 0; i < requestedPermissions.length; ++i) {
                for (String permission : permissions) {
                    if (permission.equals(requestedPermissions[i])) {
                        if (requestedPermissionsFlags != null
                                && ((requestedPermissionsFlags[i]
                                                & PackageInfo.REQUESTED_PERMISSION_GRANTED)
                                        != 0)) {
                            return true;
                        }
                        requested = true;
                        break;
                    }
                }
            }
            // Permissions requested but not granted.
            if (requested) return false;
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Couldn't find name for client package: %s", packageName);
        }
        return null;
    }

    private static @Nullable CustomTabActivity getLastTrackedFocusedTwaCustomTabActivity() {
        final Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (!(activity instanceof CustomTabActivity)) return null;
        CustomTabActivity customTabActivity = (CustomTabActivity) activity;
        if (customTabActivity.isInTwaMode()) return customTabActivity;
        return null;
    }
}
