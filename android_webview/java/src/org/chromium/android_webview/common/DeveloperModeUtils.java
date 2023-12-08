// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;

import org.chromium.android_webview.common.services.ServiceNames;
import org.chromium.base.ContextUtils;

import java.util.HashMap;
import java.util.Map;

/**
 * Utilities for communication with the developer mode ContentProvider.
 *
 * <p>This should only be called in processes which have called {@link
 * ContextUtils.initApplicationContext(Context)}.
 */
public final class DeveloperModeUtils {
    // Do not instantiate this class.
    private DeveloperModeUtils() {}

    public static final String DEVELOPER_MODE_STATE_COMPONENT =
            "org.chromium.android_webview.devui.DeveloperModeState";
    public static final String URI_AUTHORITY_SUFFIX = ".DeveloperModeContentProvider";
    public static final String FLAG_OVERRIDE_URI_PATH = "/flag-overrides";
    public static final String FLAG_OVERRIDE_NAME_COLUMN = "flagName";
    public static final String FLAG_OVERRIDE_STATE_COLUMN = "flagState";

    /**
     * Quickly determine whether developer mode is enabled. Developer mode is off-by-default.
     *
     * <p>This makes no guarantees about which processes are alive, it only indicates whether the
     * user has stepped in or out of "developer mode." Developer mode may be enabled and the
     * ContentProvider process may be dead if the user has taken a WebView update since enabling
     * developer mode.
     *
     * @param webViewPackageName the package name of the WebView implementation to fetch the flags
     *     from (generally this is the current WebView provider).
     */
    public static boolean isDeveloperModeEnabled(String webViewPackageName) {
        final Context context = ContextUtils.getApplicationContext();
        ComponentName developerModeComponent =
                new ComponentName(webViewPackageName, DEVELOPER_MODE_STATE_COMPONENT);
        int enabledState =
                context.getPackageManager().getComponentEnabledSetting(developerModeComponent);
        return enabledState == PackageManager.COMPONENT_ENABLED_STATE_ENABLED;
    }

    private static void startDeveloperUiService(String webViewPackageName) {
        final Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent();
        intent.setClassName(webViewPackageName, ServiceNames.DEVELOPER_UI_SERVICE);
        // Best effort attempt to start the service. If this fails, proceed anyway.
        try {
            context.startForegroundService(intent);
        } catch (IllegalStateException e) {
            assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                    : "Unable to start DeveloperUiService, this is only expected on Android S";
        }
    }

    /**
     * Fetch the flag overrides from the developer mode ContentProvider. This should only be called
     * if {@link #isDeveloperModeEnabled(String)} returns {@code true}, otherwise this may incur
     * unnecessary IPC or start up processes unnecessarily.
     *
     * @param webViewPackageName the package name of the WebView implementation to fetch the flags
     *     from (generally this is the current WebView provider).
     */
    public static Map<String, Boolean> getFlagOverrides(String webViewPackageName) {
        Map<String, Boolean> flagOverrides = new HashMap<>();

        Uri uri =
                new Uri.Builder()
                        .scheme("content")
                        .authority(webViewPackageName + URI_AUTHORITY_SUFFIX)
                        .path(FLAG_OVERRIDE_URI_PATH)
                        .build();
        final Context appContext = ContextUtils.getApplicationContext();
        startDeveloperUiService(webViewPackageName);
        try (Cursor cursor =
                appContext
                        .getContentResolver()
                        .query(
                                uri,
                                /* projection= */ null,
                                /* selection= */ null,
                                /* selectionArgs= */ null,
                                /* sortOrder= */ null)) {
            assert cursor != null : "ContentProvider doesn't support querying '" + uri + "'";
            int flagNameColumnIndex = cursor.getColumnIndexOrThrow(FLAG_OVERRIDE_NAME_COLUMN);
            int flagStateColumnIndex = cursor.getColumnIndexOrThrow(FLAG_OVERRIDE_STATE_COLUMN);
            while (cursor.moveToNext()) {
                String flagName = cursor.getString(flagNameColumnIndex);
                boolean flagState = cursor.getInt(flagStateColumnIndex) != 0;
                flagOverrides.put(flagName, flagState);
            }
        }
        return flagOverrides;
    }
}
