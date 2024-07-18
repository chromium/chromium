// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;

import androidx.test.filters.MediumTest;

import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.FixMethodOrder;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.MethodSorters;

import org.chromium.base.test.transit.BatchedPublicTransitRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.ChromeTabbedActivityPublicTransitEntryPoints;
import org.chromium.chrome.test.transit.ntp.MvtsFacility;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;

import java.util.List;

/** Tests the Most Visited Tiles in the NTP. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class MostVisitedTilesPTTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BatchedPublicTransitRule<RegularNewTabPageStation> mBatchedRule =
            new BatchedPublicTransitRule<>(
                    RegularNewTabPageStation.class, /* expectResetByTest= */ true);

    private final ChromeTabbedActivityPublicTransitEntryPoints mEntryPoints =
            new ChromeTabbedActivityPublicTransitEntryPoints(sActivityTestRule);

    @ClassRule
    public static SuggestionsDependenciesRule sSuggestionsDeps = new SuggestionsDependenciesRule();

    private static List<SiteSuggestion> sSiteSuggestions;

    @BeforeClass
    public static void beforeClass() {
        sSiteSuggestions =
                NewTabPageTestUtils.createFakeSiteSuggestions(sActivityTestRule.getTestServer());
        FakeMostVisitedSites mostVisitedSites = new FakeMostVisitedSites();
        mostVisitedSites.setTileSuggestions(sSiteSuggestions);
        sSuggestionsDeps.getFactory().mostVisitedSites = mostVisitedSites;
    }

    @Test
    @MediumTest
    public void test010_ClickFirstMVT() {
        doClickMVTTest(0);
    }

    @Test
    @MediumTest
    public void test020_ClickLastMVT() {
        doClickMVTTest(7);
    }

    private void doClickMVTTest(int index) {
        RegularNewTabPageStation page = mEntryPoints.startOnNtp(mBatchedRule);
        MvtsFacility mvts = page.focusOnMvts(sSiteSuggestions);
        WebPageStation mostVisitedPage;
        try (var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "NewTabPage.Module.Click", ModuleTypeOnStartAndNtp.MOST_VISITED_TILES)) {
            mostVisitedPage = mvts.scrollToAndSelectByIndex(index);
        }

        // Reset back to the NTP for batching
        page =
                mostVisitedPage.pressBack(
                        RegularNewTabPageStation.newBuilder()
                                .withIncognito(false)
                                .withIsOpeningTabs(0)
                                .withTabAlreadySelected(mostVisitedPage.getLoadedTab())
                                .build());
        assertFinalDestination(page);
    }
}
