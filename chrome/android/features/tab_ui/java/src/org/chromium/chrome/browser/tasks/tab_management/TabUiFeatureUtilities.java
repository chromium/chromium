// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.os.Build;

import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.util.XrUtils;

import java.util.Locale;
import java.util.Set;

/** A class to handle the state of flags for tab_management. */
public class TabUiFeatureUtilities {
    private static final Set<String> TAB_TEARING_OEM_ALLOWLIST = Set.of("samsung");

    /** Returns whether the Grid Tab Switcher UI should use list mode. */
    public static boolean shouldUseListMode() {
        if (ChromeFeatureList.sDisableListTabSwitcher.isEnabled()) {
            return false;
        }
        // Low-end forces list mode.
        return SysUtils.isLowEndDevice() || ChromeFeatureList.sForceListTabSwitcher.isEnabled();
    }

    /**
     * @return whether tab drag as window is enabled.
     */
    public static boolean isTabDragAsWindowEnabled() {
        return XrUtils.isXrDevice();
    }

    /** Returns whether drag drop from tab strip to create new instance is enabled. */
    public static boolean isTabDragToCreateInstanceSupported() {
        // TODO(crbug/328511660): Add OS version check once available.
        return doesOemSupportDragToCreateInstance() || !isTabDragAsWindowEnabled();
    }

    /** Returns whether device OEM is allow-listed for tab tearing */
    public static boolean doesOemSupportDragToCreateInstance() {
        return TAB_TEARING_OEM_ALLOWLIST.contains(Build.MANUFACTURER.toLowerCase(Locale.US));
    }
}
