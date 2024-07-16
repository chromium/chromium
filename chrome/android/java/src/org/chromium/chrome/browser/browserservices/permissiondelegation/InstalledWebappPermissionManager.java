// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static android.Manifest.permission.ACCESS_COARSE_LOCATION;
import static android.Manifest.permission.ACCESS_FINE_LOCATION;

import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.APP_CONTEXT;

import android.app.Activity;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import androidx.annotation.VisibleForTesting;
import androidx.browser.trusted.Token;

import dagger.Lazy;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

import javax.inject.Inject;
import javax.inject.Named;
import javax.inject.Singleton;

/**
 * Handles preserving and surfacing the permissions of installed webapps (TWAs and WebAPKs) for
 * their associated websites. Communicates with the {@link InstalledWebappPermissionStore} and
 * {@link InstalledWebappBridge}.
 *
 * Lifecycle: This is a singleton.
 * Thread safety: Only call methods on the UI thread as this class may call into native.
 * Native: Does not require native.
 */
@Singleton
public class InstalledWebappPermissionManager {
    private static final String TAG = "PermissionManager";

    private final InstalledWebappPermissionStore mStore;
    private final PackageManager mPackageManager;

    // Use a Lazy instance so we don't instantiate it on Android versions pre-O.
    private final Lazy<NotificationChannelPreserver> mChannelPreserver;

    public static InstalledWebappPermissionManager get() {
        return ChromeApplicationImpl.getComponent().resolvePermissionManager();
    }

    @Inject
    public InstalledWebappPermissionManager(
            @Named(APP_CONTEXT) Context context,
            InstalledWebappPermissionStore store,
            Lazy<NotificationChannelPreserver> channelPreserver) {
        mPackageManager = context.getPackageManager();
        mStore = store;
        mChannelPreserver = channelPreserver;
    }

    boolean isRunningTwa() {
        CustomTabActivity customTabActivity = getLastTrackedFocusedTwaCustomTabActivity();
        return customTabActivity != null;
    }

    InstalledWebappBridge.Permission[] getPermissions(@ContentSettingsType.EnumType int type) {
        if (type == ContentSettingsType.GEOLOCATION) {
            if (!isRunningTwa()) {
                return new InstalledWebappBridge.Permission[0];
            }
        }

        List<InstalledWebappBridge.Permission> permissions = new ArrayList<>();
        for (String originAsString : mStore.getStoredOrigins()) {
            Origin origin = Origin.create(originAsString);
            assert origin != null
                    : "Found unparsable Origins in the Permission Store : " + originAsString;
            if (origin == null) continue;

            @ContentSettingValues int setting = getPermission(type, origin);

            if (setting != ContentSettingValues.DEFAULT) {
                permissions.add(new InstalledWebappBridge.Permission(origin, setting));
            }
        }

        return permissions.toArray(new InstalledWebappBridge.Permission[permissions.size()]);
    }

    @UiThread
    public void addDelegateApp(Origin origin, String packageName) {
        Token token = Token.create(packageName, mPackageManager);
        if (token == null) return;
        mStore.addDelegateApp(origin, token);
    }

    @UiThread
    @Nullable
    public Set<Token> getAllDelegateApps(Origin origin) {
        return mStore.getAllDelegateApps(origin);
    }

    @UiThread
    public void updatePermission(
            Origin origin,
            String packageName,
            @ContentSettingsType.EnumType int type,
            @ContentSettingValues int settingValue) {
        String appName = getAppNameForPackage(packageName);
        if (appName == null) return;

        // It's important that we set the state before we destroy the notification channel. If we
        // did it the other way around there'd be a small moment in time where the website's
        // notification permission could flicker from SET -> UNSET -> SET. This way we transition
        // straight from the channel's permission to the app's permission.
        boolean stateChanged =
                mStore.setStateForOrigin(origin, packageName, appName, type, settingValue);

        if (type == ContentSettingsType.NOTIFICATIONS) {
            NotificationChannelPreserver.deleteChannelIfNeeded(mChannelPreserver, origin);
        }

        if (stateChanged) {
            InstalledWebappBridge.notifyPermissionsChange(type);
        }
    }

    @UiThread
    void unregister(Origin origin) {
        mStore.removeOrigin(origin);

        NotificationChannelPreserver.restoreChannelIfNeeded(mChannelPreserver, origin);

        InstalledWebappBridge.notifyPermissionsChange(ContentSettingsType.NOTIFICATIONS);
        InstalledWebappBridge.notifyPermissionsChange(ContentSettingsType.GEOLOCATION);
    }

    @UiThread
    void resetStoredPermission(Origin origin, @ContentSettingsType.EnumType int type) {
        mStore.resetPermission(origin, type);
        InstalledWebappBridge.notifyPermissionsChange(type);
    }

    /**
     * Returns the user visible name of the app that will handle permission delegation for the
     * origin.
     */
    public @Nullable String getDelegateAppName(Origin origin) {
        return mStore.getDelegateAppName(origin);
    }

    /** Returns the package of the app that will handle permission delegation for the origin. */
    public @Nullable String getDelegatePackageName(Origin origin) {
        return mStore.getDelegatePackageName(origin);
    }

    /** Gets all the origins that we delegate permissions for. */
    public Set<String> getAllDelegatedOrigins() {
        return mStore.getStoredOrigins();
    }

    void clearForTesting() {
        mStore.clearForTesting();
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
    @ContentSettingValues
    int getPermission(@ContentSettingsType.EnumType int type, Origin origin) {
        switch (type) {
            case ContentSettingsType.NOTIFICATIONS:
                {
                    @ContentSettingValues Integer settingValue = mStore.getPermission(type, origin);
                    if (settingValue == null) {
                        Log.w(TAG, "Origin %s is known but has no permission set.", origin);
                        break;
                    }
                    return settingValue;
                }
            case ContentSettingsType.GEOLOCATION:
                {
                    String packageName = getDelegatePackageName(origin);
                    Boolean enabled = hasAndroidLocationPermission(packageName);

                    // Skip if the delegated app did not enable location delegation.
                    if (enabled == null) break;

                    @ContentSettingValues
                    Integer storedPermission = mStore.getPermission(type, origin);

                    // Return |ASK| if is the first time (no previous state), and is not enabled.
                    if (storedPermission == null && !enabled) return ContentSettingValues.ASK;

                    // This is a temperate solution for the new Android one-time permission. Since
                    // we are not able to detect if use is changing the setting to "ask every
                    // time", when there is no permission, return ASK to let the client app decide
                    // whether to show the prompt.
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                        if (!enabled) return ContentSettingValues.ASK;
                    }

                    @ContentSettingValues
                    int settingValue =
                            enabled ? ContentSettingValues.ALLOW : ContentSettingValues.BLOCK;

                    updatePermission(
                            origin, packageName, ContentSettingsType.GEOLOCATION, settingValue);

                    return settingValue;
                }
        }
        return ContentSettingValues.DEFAULT;
    }

    /**
     * Returns whether the delegate application for the origin has Android location permission, or
     * {@code null} if it does not exist or did not request location permission.
     **/
    public static @Nullable Boolean hasAndroidLocationPermission(String packageName) {
        if (packageName == null) return null;

        try {
            PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
            PackageInfo packageInfo =
                    pm.getPackageInfo(packageName, PackageManager.GET_PERMISSIONS);

            String[] requestedPermissions = packageInfo.requestedPermissions;
            int[] requestedPermissionsFlags = packageInfo.requestedPermissionsFlags;

            if (requestedPermissions != null) {
                boolean locationRequested = false;
                for (int i = 0; i < requestedPermissions.length; ++i) {
                    if (ACCESS_COARSE_LOCATION.equals(requestedPermissions[i])
                            || ACCESS_FINE_LOCATION.equals(requestedPermissions[i])) {
                        if ((requestedPermissionsFlags[i]
                                        & PackageInfo.REQUESTED_PERMISSION_GRANTED)
                                != 0) {
                            return true;
                        }
                        locationRequested = true;
                    }
                }
                // Coarse or fine Location requested but not granted.
                if (locationRequested) return false;
            }
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Couldn't find name for client package: %s", packageName);
        }
        return null;
    }

    private @Nullable CustomTabActivity getLastTrackedFocusedTwaCustomTabActivity() {
        final Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (!(activity instanceof CustomTabActivity)) return null;
        CustomTabActivity customTabActivity = (CustomTabActivity) activity;
        if (customTabActivity.isInTwaMode()) return customTabActivity;
        return null;
    }
}
