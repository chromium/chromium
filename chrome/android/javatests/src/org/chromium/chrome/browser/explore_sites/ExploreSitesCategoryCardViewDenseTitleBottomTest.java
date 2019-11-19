// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.support.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Instrumentation tests for {@link ExploreSitesCategoryCardView} with Dense Title Bottom feature
 * variation enabled.
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
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=ExploreSites<FakeStudyName", "force-fieldtrials=FakeStudyName/Enabled",
        "force-fieldtrial-params=FakeStudyName.Enabled:variation/mostLikelyTile/"
                + "denseVariation/titleBottom"})
public class ExploreSitesCategoryCardViewDenseTitleBottomTest
        extends ExploreSitesCategoryCardViewTest {
    // Covers: IS_DENSE=true, MAX_ROWS=2, MAX_COLUMNS=5, numSites=MAX_COLUMNS, numBlacklisted=0
    @Test
    @SmallTest
    public void testTileQuantityDenseBottomPerfectRow() {
        runTileQuantityTest(5, 0, false, 1, 5);
    }

    // Covers: IS_DENSE=true, MAX_ROWS=2, MAX_COLUMNS=5, numSites>MAX_COLUMNS, numBlacklisted=0
    @Test
    @SmallTest
    public void testTileQuantityDenseBottomImperfectRow() {
        runTileQuantityTest(7, 0, true, 2, 7);
    }

    // Covers: IS_DENSE=true, MAX_ROWS=2, MAX_COLUMNS=5, numSites=MAX_COLUMNS, numBlacklisted=0
    @Test
    @SmallTest
    public void testTileQuantityDenseBottomOneTileRow() {
        runTileQuantityTest(6, 0, false, 1, 5);
    }

    // Covers: IS_DENSE=true, MAX_ROWS=2, MAX_COLUMNS=5, numSites=MAX_COLUMNS, numBlacklisted>0
    @Test
    @SmallTest
    public void testTileQuantityDenseBottomOneTileRowAfterBlacklisted() {
        runTileQuantityTest(7, 1, true, 2, 6);
    }

    // Covers: IS_DENSE=true, MAX_ROWS=2, MAX_COLUMNS=5, numSites=MAX_COLUMNS, numBlacklisted>0
    @Test
    @SmallTest
    public void testTileQuantityDenseBottomPerfectRowAfterBlacklisted() {
        runTileQuantityTest(6, 1, false, 1, 5);
    }

    // Covers: IS_DENSE=true, MAX_ROWS=2, MAX_COLUMNS=5, numSites<MAX_COLUMNS, numBlacklisted>0
    @Test
    @SmallTest
    public void testTileQuantityDenseBottomTooFewTiles() {
        runTileQuantityTest(5, 4, true, 1, 1);
    }

    // Covers: IS_DENSE=true, MAX_ROWS=2, MAX_COLUMNS=5, numSites>MAX_TILE_COUNT, numBlacklisted=0
    @Test
    @SmallTest
    public void testTileQuantityDenseBottomTooManyTiles() {
        runTileQuantityTest(15, 0, false, 2, 10);
    }
}
