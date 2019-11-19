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
 * Instrumentation tests for {@link ExploreSitesCategoryCardView} with Dense Title Right feature
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
                + "denseVariation/titleRight"})
public class ExploreSitesCategoryCardViewDenseTitleRightTest
        extends ExploreSitesCategoryCardViewTest {
    // Covers: IS_DENSE=true, MAX_ROWS=3, MAX_COLUMNS=3, numSites=MAX_COLUMNS, numBlacklisted=0
    @Test
    @SmallTest
    public void testTileQuantityDenseRightPerfectRow() {
        runTileQuantityTest(3, 0, false, 1, 3);
    }

    // Covers: IS_DENSE=true, MAX_ROWS=3, MAX_COLUMNS=3, numSites>MAX_COLUMNS, numBlacklisted=0
    @Test
    @SmallTest
    public void testTileQuantityDenseRightImperfectRow() {
        runTileQuantityTest(5, 0, true, 2, 5);
    }

    // Covers: IS_DENSE=true, MAX_ROWS=3, MAX_COLUMNS=3, numSites=MAX_COLUMNS, numBlacklisted=0
    @Test
    @SmallTest
    public void testTileQuantityDenseRightOneTileRow() {
        runTileQuantityTest(4, 0, false, 1, 3);
    }

    // Covers: IS_DENSE=true, MAX_ROWS=3, MAX_COLUMNS=3, numSites>MAX_COLUMNS, numBlacklisted>0
    @Test
    @SmallTest
    public void testTileQuantityDenseRightOneTileRowAfterBlacklisted() {
        runTileQuantityTest(5, 1, true, 2, 4);
    }

    // Covers: IS_DENSE=true, MAX_ROWS=3, MAX_COLUMNS=3, numSites=MAX_COLUMNS, numBlacklisted>0
    @Test
    @SmallTest
    public void testTileQuantityDenseRightPerfectRowAfterBlacklisted() {
        runTileQuantityTest(4, 1, false, 1, 3);
    }

    // Covers: IS_DENSE=true, MAX_ROWS=3, MAX_COLUMNS=3, numSites<MAX_COLUMNS, numBlacklisted>0
    @Test
    @SmallTest
    public void testTileQuantityDenseRightTooFewTiles() {
        runTileQuantityTest(4, 3, true, 1, 1);
    }

    // Covers: IS_DENSE=true, MAX_ROWS=3, MAX_COLUMNS=3, numSites>MAX_TILE_COUNT, numBlacklisted=0
    @Test
    @SmallTest
    public void testTileQuantityDenseRightTooManyTiles() {
        runTileQuantityTest(15, 0, false, 3, 9);
    }
}
