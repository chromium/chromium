// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;

/**
 * Helpers to determine colors in toolbars.
 */
public class ToolbarColors {
    /**
     * Returns whether the incognito toolbar theme color can be used in overview mode.
     */
    public static boolean canUseIncognitoToolbarThemeColorInOverview() {
        final boolean isAccessibilityEnabled = DeviceClassManager.enableAccessibilityLayout();
        final boolean isTabGridEnabled = TabUiFeatureUtilities.isGridTabSwitcherEnabled();
        final boolean isStartSurfaceEnabled = StartSurfaceConfiguration.isStartSurfaceEnabled();
        return (isAccessibilityEnabled || isTabGridEnabled || isStartSurfaceEnabled);
    }
}
