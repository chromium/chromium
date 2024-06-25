// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.DeviceFormFactor;

/** A class to handle the state of flags for tab_management. */
public class TabUiFeatureUtilities {
    private static final String TAG = "TabFeatureUtilities";

    // Cached and fixed values.
    private static boolean sTabListEditorLongPressEntryEnabled;

    /** Set whether the longpress entry for TabListEditor is enabled. Currently only in tests. */
    public static void setTabListEditorLongPressEntryEnabledForTesting(boolean enabled) {
        var oldValue = sTabListEditorLongPressEntryEnabled;
        sTabListEditorLongPressEntryEnabled = enabled;
        ResettersForTesting.register(() -> sTabListEditorLongPressEntryEnabled = oldValue);
    }

    /** Whether the longpress entry for TabListEditor is enabled. Currently only in tests. */
    public static boolean isTabListEditorLongPressEntryEnabled() {
        return sTabListEditorLongPressEntryEnabled;
    }

    /**
     * @return Whether we should delay the placeholder tab strip removal on startup.
     * @param context The activity context.
     */
    public static boolean isDelayTempStripRemovalEnabled(Context context) {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                && ChromeFeatureList.sDelayTempStripRemoval.isEnabled();
    }

    /** Returns whether the Grid Tab Switcher UI should use list mode. */
    public static boolean shouldUseListMode() {
        // Low-end forces list mode.
        return SysUtils.isLowEndDevice() || ChromeFeatureList.sForceListTabSwitcher.isEnabled();
    }

    /**
     * @return whether tab drag as window is enabled.
     */
    public static boolean isTabDragAsWindowEnabled() {
        return ChromeFeatureList.sTabDragDropAsWindowAndroid.isEnabled();
    }

    /** Returns if the tab group pane should be displayed in the hub. */
    public static boolean isTabGroupPaneEnabled() {
        return ChromeFeatureList.sTabGroupPaneAndroid.isEnabled();
    }

    /** Returns whether drag drop from tab strip to create new instance is enabled. */
    // TODO(crbug/328511660): This flag is similar with {@link #isTabDragAsWindowEnabled()}.
    // Consider merge code logic.
    public static boolean isTabTearingEnabled() {
        // TODO(crbug/328511660): Add OS version check once available.
        return ChromeFeatureList.isEnabled(ChromeFeatureList.DRAG_DROP_TAB_TEARING)
                && !isTabDragAsWindowEnabled();
    }
}
