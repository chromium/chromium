// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.DeviceFormFactor;

/** Utility class for checking if Settings in Tab feature is enabled. */
@NullMarked
public class SettingsInTab {
    /**
     * Returns true if the feature flag is enabled and the device form factor is tablet/desktop.
     * Desktop uses SettingsInTabDesktop; tablet uses SettingsInTab.
     */
    public static boolean isEnabled() {
        // SettingsInTab requires SettingsMultiColumn, which is disabled by some tests.
        if (!ChromeFeatureList.sSettingsMultiColumn.isEnabled()) return false;

        // DeviceInfo.isDesktop() is checked in addition to isNonMultiDisplayContextOnTablet()
        // because desktop windows can be resized to narrow widths (< 600dp).
        if (DeviceInfo.isDesktop()) {
            return ChromeFeatureList.sSettingsInTabDesktop.isEnabled();
        }

        // Tablets and foldables use the SettingsInTab flag.
        if (!ChromeFeatureList.sSettingsInTab.isEnabled()) return false;

        // Use an Activity context when available because theme changes reset application-level
        // resource configurations, causing getApplicationContext() to lose its tablet screen width
        // qualifiers (-sw600dp).
        Context context = ApplicationStatus.getLastTrackedFocusedActivity();
        if (context == null) {
            context = ContextUtils.getApplicationContext();
        }
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }
}
