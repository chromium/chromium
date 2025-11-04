// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.ntp_customization.theme.UploadImagePreviewCoordinator;

/** The utility class for logging the NTP customization bottom sheet's metrics. */
@NullMarked
public class NtpCustomizationMetricsUtils {
    @VisibleForTesting
    static final String HISTOGRAM_NTP_CUSTOMIZATION_PREFIX = "NewTabPage.Customization";

    @VisibleForTesting
    static final String HISTOGRAM_NTP_CUSTOMIZATION_MVT_ENABLED =
            HISTOGRAM_NTP_CUSTOMIZATION_PREFIX + ".MvtEnabled";

    @VisibleForTesting
    static final String HISTOGRAM_BOTTOM_SHEET_SHOWN =
            HISTOGRAM_NTP_CUSTOMIZATION_PREFIX + ".BottomSheet.Shown";

    @VisibleForTesting
    static final String HISTOGRAM_BOTTOM_SHEET_ENTRY =
            HISTOGRAM_NTP_CUSTOMIZATION_PREFIX + ".OpenBottomSheetEntry";

    @VisibleForTesting static final String HISTOGRAM_CUSTOMIZATION_TURN_ON_MODULE = ".TurnOnModule";

    @VisibleForTesting
    static final String HISTOGRAM_CUSTOMIZATION_TURN_OFF_MODULE = ".TurnOffModule";

    @VisibleForTesting
    static final String HISTOGRAM_THEME_UPLOAD_IMAGE_PREFIX =
            "NewTabPage.Customization.Theme.UploadImage";

    @VisibleForTesting
    static final String HISTOGRAM_THEME_UPLOAD_IMAGE_PREVIEW_SHOW =
            HISTOGRAM_THEME_UPLOAD_IMAGE_PREFIX + ".PreviewShow";

    @VisibleForTesting
    static final String HISTOGRAM_THEME_UPLOAD_IMAGE_PREVIEW_INTERACTIONS =
            HISTOGRAM_THEME_UPLOAD_IMAGE_PREFIX + ".PreviewInteractions";

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
                ModuleDelegate.ModuleType.NUM_ENTRIES);
    }

    /**
     * Records the visibility of the Most Visited Tiles section on the New Tab Page as controlled by
     * the toggle in the bottom sheet.
     *
     * @param isEnabled True if the module is enabled (visible).
     */
    public static void recordMvtToggledInBottomSheet(boolean isEnabled) {
        RecordHistogram.recordBooleanHistogram(HISTOGRAM_NTP_CUSTOMIZATION_MVT_ENABLED, isEnabled);
    }

    /** Records the total number of times the Upload Image Preview is shown. */
    public static void recordThemeUploadImagePreviewShow() {
        RecordHistogram.recordBooleanHistogram(HISTOGRAM_THEME_UPLOAD_IMAGE_PREVIEW_SHOW, true);
    }

    /**
     * Records the user interactions with the Upload Image Preview dialog.
     *
     * @param interactionType The type of user interaction.
     */
    public static void recordThemeUploadImagePreviewInteractions(
            @UploadImagePreviewCoordinator.PreviewInteractionType int interactionType) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_THEME_UPLOAD_IMAGE_PREVIEW_INTERACTIONS,
                interactionType,
                UploadImagePreviewCoordinator.PreviewInteractionType.NUM_ENTRIES);
    }
}
