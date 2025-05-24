// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import android.support.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;

/** The utility class for logging the NTP customization bottom sheet's metrics. */
@NullMarked
public class NtpCustomizationMetricsUtils {
    @VisibleForTesting
    static final String HISTOGRAM_NTP_CUSTOMIZATION_PREFIX = "NewTabPage.Customization";

    @VisibleForTesting
    static final String HISTOGRAM_BOTTOM_SHEET_SHOWN =
            HISTOGRAM_NTP_CUSTOMIZATION_PREFIX + ".BottomSheet.Shown";

    @VisibleForTesting
    static final String HISTOGRAM_BOTTOM_SHEET_ENTRY =
            HISTOGRAM_NTP_CUSTOMIZATION_PREFIX + ".OpenBottomSheetEntry";

    @VisibleForTesting static final String HISTOGRAM_CUSTOMIZATION_TURN_ON_MODULE = ".TurnOnModule";

    @VisibleForTesting
    static final String HISTOGRAM_CUSTOMIZATION_TURN_OFF_MODULE = ".TurnOffModule";

    /**
     * Records the total number of times each NTP customization bottom sheet is shown. Each opening
     * of the bottom sheet is counted, regardless of whether it has been opened previously.
     *
     * @param bottomSheetType The type of the NTP customization bottom sheet.
     */
    public static void recordBottomSheetShown(
            @NtpCustomizationCoordinator.BottomSheetType int bottomSheetType) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_BOTTOM_SHEET_SHOWN,
                bottomSheetType,
                NtpCustomizationCoordinator.BottomSheetType.NUM_ENTRIES);
    }

    /**
     * Records the number of times the NTP customization bottom sheet is opened by the user,
     * categorized by the specific source of the opening action: either from the main menu or from
     * the toolbar.
     *
     * @param entryPointType The type of the entry point to open the NTP customization main bottom
     *     sheet.
     */
    public static void recordOpenBottomSheetEntry(
            @NtpCustomizationCoordinator.EntryPointType int entryPointType) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_BOTTOM_SHEET_ENTRY,
                entryPointType,
                NtpCustomizationCoordinator.EntryPointType.NUM_ENTRIES);
    }

    /**
     * Records when a magic stack module is activated or deactivated in the bottom sheet.
     *
     * @param moduleType The type of the module.
     * @param isEnabled True if the module is turned on.
     */
    public static void recordModuleToggledInBottomSheet(
            @ModuleDelegate.ModuleType int moduleType, boolean isEnabled) {
        String name =
                isEnabled
                        ? HISTOGRAM_CUSTOMIZATION_TURN_ON_MODULE
                        : HISTOGRAM_CUSTOMIZATION_TURN_OFF_MODULE;
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_NTP_CUSTOMIZATION_PREFIX + name,
                moduleType,
                NtpCustomizationCoordinator.BottomSheetType.NUM_ENTRIES);
    }
}
