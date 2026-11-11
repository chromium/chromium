// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.os.Build;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabListLayoutType;

import java.util.Locale;
import java.util.Set;

/** A class to handle the state of flags for tab_management. */
@NullMarked
public class TabUiFeatureUtilities {
    private static final Set<String> TAB_TEARING_OEM_ALLOWLIST = Set.of("samsung");

    /** Returns whether device OEM is allow-listed for tab tearing */
    public static boolean doesOemSupportDragToCreateInstance() {
        return TAB_TEARING_OEM_ALLOWLIST.contains(Build.MANUFACTURER.toLowerCase(Locale.US));
    }

    /** Returns whether the Android Tab UI refactor is enabled. */
    public static boolean isAndroidTabUiRefactorEnabled() {
        return ChromeFeatureList.sAndroidTabUiRefactor.isEnabled();
    }

    /**
     * Returns whether Tab UI data virtualization is enabled for the given layout type.
     *
     * @param layoutType The {@link TabListLayoutType} to check.
     * @return Whether Tab UI data virtualization is enabled.
     */
    public static boolean isTabUiDataVirtualizationEnabled(@TabListLayoutType int layoutType) {
        if (!ChromeFeatureList.sTabUiDataVirtualization.isEnabled()) {
            return false;
        }
        return switch (layoutType) {
            case TabListLayoutType.FLAT ->
                    ChromeFeatureList.sTabUiDataVirtualizationFlat.getValue();
            // The grouped surfaces depend on the Android Tab UI refactor.
            case TabListLayoutType.GROUPED ->
                    isAndroidTabUiRefactorEnabled()
                            && ChromeFeatureList.sTabUiDataVirtualizationGrouped.getValue();
            case TabListLayoutType.NESTED ->
                    ChromeFeatureList.sTabUiDataVirtualizationNested.getValue();
            default -> false;
        };
    }
}
