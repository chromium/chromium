// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.UploadImagePreviewCoordinator;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** The utility class for logging the NTP customization bottom sheet's metrics. */
@NullMarked
public class NtpCustomizationMetricsUtils {
    @VisibleForTesting
    static final String HISTOGRAM_NTP_CUSTOMIZATION_PREFIX = "NewTabPage.Customization";

    @VisibleForTesting
    static final String HISTOGRAM_NTP_CUSTOMIZATION_ALL_CARDS_ENABLED =
            HISTOGRAM_NTP_CUSTOMIZATION_PREFIX + ".AllCardsEnabled";

    @VisibleForTesting
    static final String HISTOGRAM_NTP_CUSTOMIZATION_MVT_ENABLED =
            HISTOGRAM_NTP_CUSTOMIZATION_PREFIX + ".MvtEnabled";

    @VisibleForTesting
    static final String HISTOGRAM_NTP_CUSTOMIZATION_USER_ENGAGEMENT_MVT_ENABLED =
            HISTOGRAM_NTP_CUSTOMIZATION_PREFIX + ".UserEngagement.MostVisitedSitesEnabled";

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

    @VisibleForTesting
    static final String HISTOGRAM_THEME_TYPE = HISTOGRAM_NTP_CUSTOMIZATION_PREFIX + ".Theme.Type";

    @VisibleForTesting
    static final String HISTOGRAM_THEME_COLLECTION =
            "NewTabPage.Customization.Theme.ThemeCollection";

    @VisibleForTesting
    static final String HISTOGRAM_THEME_COLLECTION_SHOW =
            HISTOGRAM_THEME_COLLECTION + ".CollectionShow";

    @VisibleForTesting
    static final String HISTOGRAM_THEME_COLLECTION_SELECTED =
            HISTOGRAM_THEME_COLLECTION + ".CollectionSelected";

    @VisibleForTesting
    static final String HISTOGRAM_THEME_CHROME_COLOR = "NewTabPage.Customization.Theme.ChromeColor";

    @VisibleForTesting
    static final String HISTOGRAM_THEME_CHROME_COLOR_ID = HISTOGRAM_THEME_CHROME_COLOR + ".Click";

    @VisibleForTesting
    static final String HISTOGRAM_CHROME_COLOR_TURN_ON_DAILY_REFRESH =
            HISTOGRAM_THEME_CHROME_COLOR + ".TurnOnDailyRefresh";

    /**
     * Records the type of theme selected for the New Tab Page background. This is logged once on
     * cold startup.
     *
     * @param themeType The type of the NTP customization theme.
     */
    public static void recordNtpThemeType() {
        int themeType = NtpCustomizationUtils.getNtpBackgroundImageType();
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_THEME_TYPE, themeType, NtpBackgroundImageType.NUM_ENTRIES);
    }

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
     * Records the visibility of all NTP cards as controlled by the toggle in the bottom sheet.
     *
     * @param isEnabled True if all cards are enabled (visible).
     */
    public static void recordAllCardsToggledInConfiguration(boolean isEnabled) {
        RecordHistogram.recordBooleanHistogram(
                HISTOGRAM_NTP_CUSTOMIZATION_ALL_CARDS_ENABLED, isEnabled);
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

    /**
     * Records the visibility state of the Most Visited Tiles section on the New Tab Page.
     *
     * <p>This method is called during Chrome's launch sequence to log whether the MVT section is
     * configured to be visible.
     */
    public static void recordMvtUserEngagement() {
        RecordHistogram.recordBooleanHistogram(
                HISTOGRAM_NTP_CUSTOMIZATION_USER_ENGAGEMENT_MVT_ENABLED,
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.IS_MVT_VISIBLE, true));
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

    /**
     * Records the total number of impressions of each theme collection sub category shown,
     * regardless of whether it has been shown previously.
     *
     * @param themeCollectionHash The hash of the collection ID.
     */
    public static void recordThemeCollectionShow(int themeCollectionHash) {
        RecordHistogram.recordSparseHistogram(HISTOGRAM_THEME_COLLECTION_SHOW, themeCollectionHash);
    }

    /**
     * Records when a theme from a collection is selected. This is logged only for the first
     * selection within a specific theme collection's view. Navigating to a different collection and
     * making a selection will trigger a new record, but multiple selections within the same
     * collection view will not.
     *
     * @param themeCollectionHash The hash of the collection ID.
     */
    public static void recordThemeCollectionSelected(int themeCollectionHash) {
        RecordHistogram.recordSparseHistogram(
                HISTOGRAM_THEME_COLLECTION_SELECTED, themeCollectionHash);
    }

    /**
     * Records when a Chrome color is selected from the Chrome Colors bottom sheet.
     *
     * @param themeColorId The ID of the color.
     */
    public static void recordChromeColorId(@NtpThemeColorId int themeColorId) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_THEME_CHROME_COLOR_ID, themeColorId, NtpThemeColorId.NUM_ENTRIES);
    }

    /**
     * Records whether the daily refresh for Chrome Colors is turned on or off.
     *
     * @param isTurnedOn Whether daily refresh for Chrome Colors is turned on.
     */
    public static void recordChromeColorTurnOnDailyRefresh(boolean isTurnedOn) {
        RecordHistogram.recordBooleanHistogram(
                HISTOGRAM_CHROME_COLOR_TURN_ON_DAILY_REFRESH, isTurnedOn);
    }
}
