// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType.PRICE_CHANGE;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MAIN;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.EntryPointType.MAIN_MENU;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.EntryPointType.TOOL_BAR;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.CHROME_COLOR;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.COLOR_FROM_HEX;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.DEFAULT;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.IMAGE_FROM_DISK;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.THEME_COLLECTION;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_AQUA;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_BLUE;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_CITRON;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_FUCHSIA;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_GREEN;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_ORANGE;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_ROSE;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_VIOLET;
import static org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_VIRIDIAN;
import static org.chromium.chrome.browser.ntp_customization.theme.upload_image.UploadImagePreviewCoordinator.PreviewInteractionType.CANCEL;
import static org.chromium.chrome.browser.ntp_customization.theme.upload_image.UploadImagePreviewCoordinator.PreviewInteractionType.SAVE;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.UploadImagePreviewCoordinator.PreviewInteractionType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Unit tests for {@link NtpCustomizationMetricsUtils} */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpCustomizationMetricsUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Test
    public void testRecordBottomSheetShown() {
        String histogramName = "NewTabPage.Customization.BottomSheet.Shown";
        @BottomSheetType int[] bottomSheetTypes = new int[] {MAIN, NTP_CARDS};

        for (@BottomSheetType int type : bottomSheetTypes) {
            HistogramWatcher histogramWatcher =
                    HistogramWatcher.newSingleRecordWatcher(histogramName, type);
            NtpCustomizationMetricsUtils.recordBottomSheetShown(type);
            histogramWatcher.assertExpected();
        }
    }

    @Test
    public void testRecordOpenBottomSheetEntry() {
        String histogramName = "NewTabPage.Customization.OpenBottomSheetEntry";
        @NtpCustomizationCoordinator.EntryPointType
        int[] entryPointTypes = new int[] {MAIN_MENU, TOOL_BAR};

        for (@NtpCustomizationCoordinator.EntryPointType int type : entryPointTypes) {
            HistogramWatcher histogramWatcher =
                    HistogramWatcher.newSingleRecordWatcher(histogramName, type);
            NtpCustomizationMetricsUtils.recordOpenBottomSheetEntry(type);
            histogramWatcher.assertExpected();
        }
    }

    @Test
    public void testRecordModuleToggledInBottomSheet() {
        @ModuleDelegate.ModuleType int moduleType = PRICE_CHANGE;
        boolean isEnabled = true;
        String histogramName = "NewTabPage.Customization.TurnOnModule";

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, moduleType);
        NtpCustomizationMetricsUtils.recordModuleToggledInBottomSheet(moduleType, isEnabled);
        histogramWatcher.assertExpected();

        isEnabled = false;
        histogramName = "NewTabPage.Customization.TurnOffModule";

        histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, moduleType);
        NtpCustomizationMetricsUtils.recordModuleToggledInBottomSheet(moduleType, isEnabled);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordMvtToggledInBottomSheet() {
        String histogramName = "NewTabPage.Customization.MvtEnabled";

        boolean isEnabled = true;
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, isEnabled);
        NtpCustomizationMetricsUtils.recordMvtToggledInBottomSheet(isEnabled);
        histogramWatcher.assertExpected();

        isEnabled = false;
        histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, isEnabled);
        NtpCustomizationMetricsUtils.recordMvtToggledInBottomSheet(isEnabled);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordThemeUploadImagePreviewShow() {
        String histogramName = "NewTabPage.Customization.Theme.UploadImage.PreviewShow";

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, true);
        NtpCustomizationMetricsUtils.recordThemeUploadImagePreviewShow();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordThemeUploadImagePreviewInteractions() {
        String histogramName = "NewTabPage.Customization.Theme.UploadImage.PreviewInteractions";
        @PreviewInteractionType int[] interactionTypes = new int[] {CANCEL, SAVE};

        for (@PreviewInteractionType int type : interactionTypes) {
            HistogramWatcher histogramWatcher =
                    HistogramWatcher.newSingleRecordWatcher(histogramName, type);
            NtpCustomizationMetricsUtils.recordThemeUploadImagePreviewInteractions(type);
            histogramWatcher.assertExpected();
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testRecordNtpThemeType() {
        String histogramName = "NewTabPage.Customization.Theme.Type";
        @NtpBackgroundImageType
        int[] backgroundImageTypes =
                new int[] {
                    DEFAULT, IMAGE_FROM_DISK, CHROME_COLOR, COLOR_FROM_HEX, THEME_COLLECTION
                };

        for (@NtpBackgroundImageType int type : backgroundImageTypes) {
            NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(type);
            HistogramWatcher histogramWatcher =
                    HistogramWatcher.newSingleRecordWatcher(histogramName, type);
            NtpCustomizationMetricsUtils.recordNtpThemeType();
            histogramWatcher.assertExpected();
        }

        NtpCustomizationUtils.resetSharedPreferenceForTesting();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testRecordNtpThemeType_flagDisabled() {
        String histogramName = "NewTabPage.Customization.Theme.Type";
        @NtpBackgroundImageType int backgroundImageTypes = IMAGE_FROM_DISK;

        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(backgroundImageTypes);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, DEFAULT);
        NtpCustomizationMetricsUtils.recordNtpThemeType();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordThemeCollectionShow() {
        String histogramName = "NewTabPage.Customization.Theme.ThemeCollection.CollectionShow";
        int themeCollectionHash = 123; // Mock hash value for testing

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, themeCollectionHash);
        NtpCustomizationMetricsUtils.recordThemeCollectionShow(themeCollectionHash);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordThemeCollectionSelected() {
        String histogramName = "NewTabPage.Customization.Theme.ThemeCollection.CollectionSelected";
        int themeCollectionHash = 123; // Mock hash value for testing

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, themeCollectionHash);
        NtpCustomizationMetricsUtils.recordThemeCollectionSelected(themeCollectionHash);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordMvtUserEngagement() {
        final String histogramName =
                "NewTabPage.Customization.UserEngagement.MostVisitedSitesEnabled";
        final String prefKey = ChromePreferenceKeys.IS_MVT_VISIBLE;
        final SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();

        // --- Test Case 1: MVT is visible (true) ---
        prefs.writeBoolean(prefKey, true);
        HistogramWatcher histogramWatcherTrue =
                HistogramWatcher.newSingleRecordWatcher(histogramName, true);

        NtpCustomizationMetricsUtils.recordMvtUserEngagement();

        histogramWatcherTrue.assertExpected();

        // --- Test Case 2: MVT is not visible (false) ---
        prefs.writeBoolean(prefKey, false);
        HistogramWatcher histogramWatcherFalse =
                HistogramWatcher.newSingleRecordWatcher(histogramName, false);

        NtpCustomizationMetricsUtils.recordMvtUserEngagement();

        histogramWatcherFalse.assertExpected();

        // Cleanup: Remove the key to not affect other tests.
        prefs.removeKey(prefKey);
    }

    @Test
    public void testRecordChromeColorSelected() {
        String histogramName = "NewTabPage.Customization.Theme.ChromeColor.Click";
        @NtpThemeColorId
        int[] chromeColorIds =
                new int[] {
                    NTP_COLORS_BLUE,
                    NTP_COLORS_AQUA,
                    NTP_COLORS_GREEN,
                    NTP_COLORS_VIRIDIAN,
                    NTP_COLORS_CITRON,
                    NTP_COLORS_ORANGE,
                    NTP_COLORS_ROSE,
                    NTP_COLORS_FUCHSIA,
                    NTP_COLORS_VIOLET
                };

        for (@NtpThemeColorId int colorId : chromeColorIds) {
            HistogramWatcher histogramWatcher =
                    HistogramWatcher.newSingleRecordWatcher(histogramName, colorId);
            NtpCustomizationMetricsUtils.recordChromeColorId(colorId);
            histogramWatcher.assertExpected();
        }
    }

    @Test
    public void testRecordChromeColorTurnOnDailyRefresh() {
        String histogramName = "NewTabPage.Customization.Theme.ChromeColor.TurnOnDailyRefresh";

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(histogramName, true);
        NtpCustomizationMetricsUtils.recordChromeColorTurnOnDailyRefresh(true);
        histogramWatcher.assertExpected();

        histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, false);
        NtpCustomizationMetricsUtils.recordChromeColorTurnOnDailyRefresh(false);
        histogramWatcher.assertExpected();
    }
}
