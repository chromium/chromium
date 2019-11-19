// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.Manifest;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.preferences.website.WebsitePreferenceBridge;
import org.chromium.components.location.LocationSettingsDialogContext;
import org.chromium.components.location.LocationSettingsDialogOutcome;
import org.chromium.components.location.LocationUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Provides methods for querying Chrome's internal location setting and
 * combining that with the system-wide setting and permissions.
 *
 * This class should be used only on the UI thread.
 */
public class LocationSettings {

    private static LocationSettings sInstance;

    /**
     * Don't use this; use getInstance() instead. This should be used only by the Application inside
     * of createLocationSettings().
     */
    protected LocationSettings() {
    }

    /**
     * Returns the singleton instance of LocationSettings, creating it if needed.
     */
    public static LocationSettings getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = AppHooks.get().createLocationSettings();
        }
        return sInstance;
    }

    @CalledByNative
    private static boolean hasAndroidLocationPermission() {
        return LocationUtils.getInstance().hasAndroidLocationPermission();
    }

    @CalledByNative
    private static boolean canPromptForAndroidLocationPermission(WebContents webContents) {
        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return false;

        return windowAndroid.canRequestPermission(Manifest.permission.ACCESS_FINE_LOCATION);
    }

    @CalledByNative
    private static boolean isSystemLocationSettingEnabled() {
        return LocationUtils.getInstance().isSystemLocationSettingEnabled();
    }

    @CalledByNative
    private static boolean canPromptToEnableSystemLocationSetting() {
        return LocationUtils.getInstance().canPromptToEnableSystemLocationSetting();
    }

    @CalledByNative
    private static void promptToEnableSystemLocationSetting(
            @LocationSettingsDialogContext int promptContext, WebContents webContents,
            final long nativeCallback) {
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) {
            LocationSettingsJni.get().onLocationSettingsDialogOutcome(
                    nativeCallback, LocationSettingsDialogOutcome.NO_PROMPT);
            return;
        }
        LocationUtils.getInstance().promptToEnableSystemLocationSetting(
                promptContext, window, new Callback<Integer>() {
                    @Override
                    public void onResult(Integer result) {
                        LocationSettingsJni.get().onLocationSettingsDialogOutcome(
                                nativeCallback, result);
                    }
                });
    }

    /**
     * Returns true if location is enabled system-wide and the Chrome location setting is enabled.
     */
    public boolean areAllLocationSettingsEnabled() {
        return isChromeLocationSettingEnabled()
                && LocationUtils.getInstance().isSystemLocationSettingEnabled();
    }

    /**
     * Returns whether Chrome's user-configurable location setting is enabled.
     */
    public boolean isChromeLocationSettingEnabled() {
        return WebsitePreferenceBridge.isAllowLocationEnabled();
    }

    @VisibleForTesting
    public static void setInstanceForTesting(LocationSettings instance) {
        sInstance = instance;
    }

    @NativeMethods
    interface Natives {
        void onLocationSettingsDialogOutcome(long callback, int result);
    }
}
