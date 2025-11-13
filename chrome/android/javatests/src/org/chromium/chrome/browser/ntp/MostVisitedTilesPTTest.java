// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.os.Build;

import androidx.test.filters.MediumTest;

import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.FixMethodOrder;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.MethodSorters;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ntp.MvtsFacility;
import org.chromium.chrome.test.transit.ntp.MvtsTileContextMenuFacility;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.List;

/** Tests the Most Visited Tiles in the NTP. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class MostVisitedTilesPTTest {
    @ClassRule public static EmbeddedTestServerRule sTestServerRule = new EmbeddedTestServerRule();

    @ClassRule
    public static SuggestionsDependenciesRule sSuggestionsDeps = new SuggestionsDependenciesRule();

    private static List<SiteSuggestion> sSiteSuggestions;

    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @BeforeClass
    public static void beforeClass() {
        sSiteSuggestions =
                NewTabPageTestUtils.createFakeSiteSuggestions(sTestServerRule.getServer());
        FakeMostVisitedSites mostVisitedSites = new FakeMostVisitedSites();
        mostVisitedSites.setTileSuggestions(sSiteSuggestions);
        sSuggestionsDeps.getFactory().mostVisitedSites = mostVisitedSites;
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION})
    public void testClickFirstMVT_DisableMvtCustomization() {
        doClickMVTTest(0);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION})
    public void testClickFirstMVT_EnableMvtCustomization() {
        doClickMVTTest(0);
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION})
    public void testClickLastMVT_DisableMvtCustomization() {
        doClickMVTTest(7);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION})
    public void testClickLastMVT_EnableMvtCustomization() {
        doClickMVTTest(7);
    }

    private void doClickMVTTest(int index) {
        RegularNewTabPageStation page = mCtaTestRule.startOnNtp();

        MvtsFacility mvts = page.focusOnMvts(sSiteSuggestions);
        try (var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "NewTabPage.Module.Click", ModuleTypeOnStartAndNtp.MOST_VISITED_TILES)) {
            mvts.ensureTileIsDisplayedAndGet(index).clickToNavigateToWebPage();
        }
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void testOpenItemInIncognitoTab() {
        RegularNewTabPageStation page = mCtaTestRule.startOnNtp();

        MvtsFacility mvts = page.focusOnMvts(sSiteSuggestions);
        MvtsTileContextMenuFacility menu = mvts.ensureTileIsDisplayedAndGet(1).openContextMenu();
        HistogramWatcher histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "NewTabPage.Module.Click", ModuleTypeOnStartAndNtp.MOST_VISITED_TILES);

        menu.selectOpenInIncognitoTab();

        histogram.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    @MinAndroidSdkLevel(Build.VERSION_CODES.S)
    public void testOpenItemInIncognitoWindow() {
        RegularNewTabPageStation page = mCtaTestRule.startOnNtp();

        MvtsFacility mvts = page.focusOnMvts(sSiteSuggestions);
        MvtsTileContextMenuFacility menu = mvts.ensureTileIsDisplayedAndGet(1).openContextMenu();
        HistogramWatcher histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "NewTabPage.Module.Click", ModuleTypeOnStartAndNtp.MOST_VISITED_TILES);

        menu.selectOpenInIncognitoWindow();

        histogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testOpenItemInNewTab() {
        RegularNewTabPageStation page = mCtaTestRule.startOnNtp();

        MvtsFacility mvts = page.focusOnMvts(sSiteSuggestions);
        MvtsTileContextMenuFacility menu = mvts.ensureTileIsDisplayedAndGet(1).openContextMenu();
        HistogramWatcher histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "NewTabPage.Module.Click", ModuleTypeOnStartAndNtp.MOST_VISITED_TILES);

        menu.selectOpenInNewTab();

        histogram.assertExpected();
    }
}
