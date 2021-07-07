// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;

import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;

/**
 * Helpers to determine colors in toolbars.
 */
public class ToolbarColors {
    /**
     * Returns whether the incognito toolbar theme color can be used in overview mode.
     * @param context The activity context.
     */
    public static boolean canUseIncognitoToolbarThemeColorInOverview(Context context) {
        final boolean isAccessibilityEnabled = DeviceClassManager.enableAccessibilityLayout();
        final boolean isTabGridEnabled = TabUiFeatureUtilities.isGridTabSwitcherEnabled(context);
        final boolean isStartSurfaceEnabled =
                ReturnToChromeExperimentsUtil.isStartSurfaceHomepageEnabled();
        return (isAccessibilityEnabled || isTabGridEnabled || isStartSurfaceEnabled);
    }
}
