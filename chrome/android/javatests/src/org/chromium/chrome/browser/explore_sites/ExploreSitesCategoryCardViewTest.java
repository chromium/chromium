// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import static org.junit.Assert.assertEquals;

import android.support.test.filters.SmallTest;
import android.support.v7.widget.RecyclerView;
import android.util.Pair;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;

/**
 * Instrumentation tests for {@link ExploreSitesCategoryCardView}. Only covers the original feature
 * variation.
 *
 * Tested Methods:
 *  - ExploreSitesCategoryCardView.allowIncompleteRow
 *  - ExploreSitesCategoryCardView.maxRows
 *  - ExploreSitesCategoryCardView.tilesToDisplay
 *
 * Test Partitions:
 *  - MAX_ROWS: 2, 3
 *  - MAX_COLUMNS: 3, 4, 5
 *  - IS_DENSE: true, false
 *  - category:
 *     - numSites: <MAX_COLUMNS, MAX_COLUMNS, >MAX_COLUMNS, >MAX_TILE_COUNT
 *     - numBlacklisted: 0, >0
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ExploreSitesCategoryCardViewTest {
    // Private test helper for bootstrapping categories
    // All black listed sites are at beginning of category
    // numMockSites is the total, it should be greater than numBlacklisted
    private ExploreSitesCategory createSyntheticCategory(int numMockSites, int numBlacklisted) {
        final int id = 1;
        @ExploreSitesCategory.CategoryType
        final int type = ExploreSitesCategory.CategoryType.SCIENCE;
        final String title = "Category Title";
        final int ntpShownCount = 0;
        final int interactionCount = 0;
        ExploreSitesCategory syntheticCategory =
                new ExploreSitesCategory(id, type, title, ntpShownCount, interactionCount);

        for (int i = 0; i < numMockSites; i++) {
            final int site_id = i;
            final String site_title = "Site #" + i;
            final String site_url = "http://example.com/" + i;
            final boolean isBlacklisted = i < numBlacklisted;
            ExploreSitesSite mockSite =
                    new ExploreSitesSite(site_id, site_title, site_url, isBlacklisted);
            syntheticCategory.addSite(mockSite);
        }

        return syntheticCategory;
    }

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private Pair<ExploreSitesCategoryCardView, ExploreSitesCategory> initializeCategoryAndView(
            int numSitesTotal, int numBlacklisted) {
        ExploreSitesCategory category = createSyntheticCategory(numSitesTotal, numBlacklisted);

        ArrayList<ExploreSitesCategory> catalog = new ArrayList<>();
        catalog.add(category);

        ExploreSitesBridge.setCatalogForTesting(catalog);

        mActivityTestRule.startMainActivityWithURL("about:blank");
        mActivityTestRule.loadUrl(UrlConstants.EXPLORE_URL);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        ExploreSitesPageTest.waitForEspLoaded(tab);

        Assert.assertTrue(tab.getNativePage() instanceof ExploreSitesPage);
        ExploreSitesPage esp = (ExploreSitesPage) tab.getNativePage();
        RecyclerView recyclerView =
                esp.getView().findViewById(R.id.explore_sites_category_recycler);

        ExploreSitesCategoryCardView categoryCardView =
                (ExploreSitesCategoryCardView) recyclerView.getChildAt(0);

        return Pair.create(categoryCardView, category);
    }

    // package-private helper for running a tile quantity render tests.
    void runTileQuantityTest(int numSitesTotal, int numBlacklisted, boolean incompleteAllowed,
            int expectedMaxRows, int expectedTilesToDisplay) {
        Pair<ExploreSitesCategoryCardView, ExploreSitesCategory> categoryState =
                initializeCategoryAndView(numSitesTotal, numBlacklisted);

        ExploreSitesCategoryCardView categoryCardView = categoryState.first;
        ExploreSitesCategory category = categoryState.second;

        boolean incompleteAllowedActual = categoryCardView.allowIncompleteRow(category);

        assertEquals(incompleteAllowed, incompleteAllowedActual);
        assertEquals(
                expectedMaxRows, categoryCardView.rowsToDisplay(category, incompleteAllowedActual));
        assertEquals(expectedTilesToDisplay,
                categoryCardView.tilesToDisplay(category, incompleteAllowedActual));
    }

    // Tests that cover the original tile quantity rendering logic

    // Covers: IS_DENSE=false, MAX_ROWS=2, MAX_COLUMNS=4, numSites=MAX_COLUMNS, numBlacklisted=0
    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"enable-features=ExploreSites<FakeStudyName", "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:variation/mostLikelyTile/"
                    + "denseVariation/original"})
    public void
    testTileQuantityOriginalPerfectRow() {
        runTileQuantityTest(4, 0, false, 1, 4);
    }

    // Covers: IS_DENSE=false, MAX_ROWS=2, MAX_COLUMNS=4, numSites>MAX_COLUMNS, numBlacklisted=0
    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"enable-features=ExploreSites<FakeStudyName", "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:variation/mostLikelyTile/"
                    + "denseVariation/original"})
    public void
    testTileQuantityOriginalImperfectRow() {
        runTileQuantityTest(5, 0, false, 1, 4);
    }

    // Covers: IS_DENSE=false, MAX_ROWS=2, MAX_COLUMNS=4, numSites=MAX_COLUMNS, numBlacklisted>0
    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"enable-features=ExploreSites<FakeStudyName", "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:variation/mostLikelyTile/"
                    + "denseVariation/original"})
    public void
    testTileQuantityOriginalPerfectRowAfterBlacklisted() {
        runTileQuantityTest(5, 1, false, 1, 4);
    }

    // Covers: IS_DENSE=false, MAX_ROWS=2, MAX_COLUMNS=4, numSites>MAX_COLUMNS, numBlacklisted>0
    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"enable-features=ExploreSites<FakeStudyName", "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:variation/mostLikelyTile/"
                    + "denseVariation/original"})
    public void
    testTileQuantityOriginalImperfectRowAfterBlacklisted() {
        runTileQuantityTest(8, 2, false, 2, 6);
    }

    // Covers: IS_DENSE=true, MAX_ROWS=2, MAX_COLUMNS=4, numSites<MAX_COLUMNS, numBlacklisted>0
    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"enable-features=ExploreSites<FakeStudyName", "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:variation/mostLikelyTile/"
                    + "denseVariation/original"})
    public void
    testTileQuantityOriginalTooFewTilesAfterBlacklisted() {
        runTileQuantityTest(5, 4, false, 1, 1);
    }

    // Covers: IS_DENSE=true, MAX_ROWS=2, MAX_COLUMNS=4, numSites>MAX_TILE_COUNT, numBlacklisted=0
    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"enable-features=ExploreSites<FakeStudyName", "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:variation/mostLikelyTile/"
                    + "denseVariation/original"})
    public void
    testTileQuantityOriginalTooManyTiles() {
        runTileQuantityTest(15, 0, false, 2, 8);
    }
}
