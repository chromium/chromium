// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.DeviceFormFactor;

/** Utility class for checking if Settings in Tab feature is enabled. */
@NullMarked
public class SettingsInTab {
    /** Returns true if the feature flag is enabled and the device form factor is tablet/desktop. */
    public static boolean isEnabled() {
        if (!ChromeFeatureList.sSettingsInTab.isEnabled()) return false;

        // SettingsInTab requires SettingsMultiColumn, which is disabled by some tests.
        if (!ChromeFeatureList.sSettingsMultiColumn.isEnabled()) return false;

        // Settings in a tab is supported on desktop and tablet form factors.
        // DeviceInfo.isDesktop() is checked in addition to isNonMultiDisplayContextOnTablet()
        // because desktop windows can be resized to narrow widths (< 600dp).
        return DeviceInfo.isDesktop()
                || DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                        ContextUtils.getApplicationContext());
    }
}
