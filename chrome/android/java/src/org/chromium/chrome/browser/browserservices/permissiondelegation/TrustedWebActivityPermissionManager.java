// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.preferences.website.ContentSettingValues;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

import javax.inject.Inject;
import javax.inject.Singleton;

import dagger.Lazy;

/**
 * Handles the preserving and surfacing the permissions of TWA client apps for their associated
 * websites. Communicates with the {@link TrustedWebActivityPermissionStore} and
 * {@link InstalledWebappBridge}.
 *
 * Lifecycle: This is a singleton.
 * Thread safety: Only call methods on the UI thread as this class may call into native.
 * Native: Does not require native.
 */
@Singleton
public class TrustedWebActivityPermissionManager {
    private static final String TAG = "TwaPermissionManager";

    private final TrustedWebActivityPermissionStore mStore;

    // Use a Lazy instance so we don't instantiate it on Android versions pre-O.
    private final Lazy<NotificationChannelPreserver> mPermissionPreserver;

    public static TrustedWebActivityPermissionManager get() {
        return ChromeApplication.getComponent().resolveTwaPermissionManager();
    }

    @Inject
    public TrustedWebActivityPermissionManager(TrustedWebActivityPermissionStore store,
            Lazy<NotificationChannelPreserver> preserver) {
        mStore = store;
        mPermissionPreserver = preserver;
    }

    InstalledWebappBridge.Permission[] getNotificationPermissions() {
        List<InstalledWebappBridge.Permission> permissions = new ArrayList<>();
        for (String originAsString : mStore.getStoredOrigins()) {
            Origin origin = Origin.create(originAsString);
            assert origin != null
                    : "Found unparsable Origins in the Permission Store : " + originAsString;
            if (origin == null) continue;

            Boolean enabled = mStore.areNotificationsEnabled(origin);

            if (enabled == null) {
                Log.w(TAG, "%s is known but has no notification permission.", origin);
                continue;
            }

            @ContentSettingValues
            int setting = enabled ? ContentSettingValues.ALLOW : ContentSettingValues.BLOCK;

            permissions.add(new InstalledWebappBridge.Permission(origin, setting));
        }

        return permissions.toArray(new InstalledWebappBridge.Permission[permissions.size()]);
    }

    @UiThread
    void register(Origin origin, String packageName, boolean notificationsEnabled) {
        // TODO(peconn): Only trigger if this is for the first time?

        String appName = getAppNameForPackage(packageName);
        if (appName == null) return;

        // It's important that we set the state before we destroy the notification channel. If we
        // did it the other way around there'd be a small moment in time where the website's
        // notification permission could flicker from SET -> UNSET -> SET. This way we transition
        // straight from the channel's permission to the app's permission.
        boolean stateChanged =
                mStore.setStateForOrigin(origin, packageName, appName, notificationsEnabled);

        NotificationChannelPreserver.deleteChannelIfNeeded(mPermissionPreserver, origin);

        if (stateChanged) InstalledWebappBridge.notifyPermissionsChange();
    }

    @UiThread
    void unregister(Origin origin) {
        mStore.removeOrigin(origin);

        NotificationChannelPreserver.restoreChannelIfNeeded(mPermissionPreserver, origin);

        InstalledWebappBridge.notifyPermissionsChange();
    }

    /**
     * Returns the user visible name of the app that will handle permission delegation for the
     * origin.
     */
    @Nullable
    public String getDelegateAppName(Origin origin) {
        return mStore.getAppName(origin);
    }

    /** Returns the package of the app that will handle permission delegation for the origin. */
    @Nullable
    public String getDelegatePackageName(Origin origin) {
        return mStore.getPackageName(origin);
    }

    /** Gets all the origins that we delegate permissions for. */
    public Set<String> getAllDelegatedOrigins() {
        return mStore.getStoredOrigins();
    }

    @VisibleForTesting
    void clearForTesting() {
        mStore.clearForTesting();
    }

    @Nullable
    private static String getAppNameForPackage(String packageName) {
        // TODO(peconn): Dedupe logic with ClientAppDataRecorder.
        try {
            PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
            int getAppInfoFlags = 0;
            ApplicationInfo ai = pm.getApplicationInfo(packageName, getAppInfoFlags);

            String appLabel = pm.getApplicationLabel(ai).toString();

            if (TextUtils.isEmpty(appLabel)) {
                Log.e(TAG,"Invalid details for client package: %s", appLabel);
                return null;
            }

            return appLabel;
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Couldn't find name for client package: %s", packageName);
            return null;
        }
    }
}
