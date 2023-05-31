// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/**
 * Tests for {@link MostVisitedTilesLayout}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public final class MostVisitedTilesLayoutUnitTest extends BlankUiTestActivityTestCase {
    private MostVisitedTilesCarouselLayout mMvTilesLayout;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        mMvTilesLayout = new MostVisitedTilesCarouselLayout(getActivity(), null);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testCalculateTabletIntervalPadding() {
        int tileViewWidth = 160;
        mMvTilesLayout.setTileViewWidthForTesting(tileViewWidth);
        int tileViewMinIntervalPaddingTablet = 32;
        mMvTilesLayout.setTileViewMinIntervalPaddingTabletForTesting(
                tileViewMinIntervalPaddingTablet);
        int tileViewMaxIntervalPaddingTablet = 96;
        mMvTilesLayout.setTileViewMaxIntervalPaddingTabletForTesting(
                tileViewMaxIntervalPaddingTablet);

        int tileNum = 8;
        mMvTilesLayout.setInitialTileNumForTesting(tileNum);

        int totalWidth = 1504;
        int expectedIntervalPadding = 32;
        int intervalPadding =
                mMvTilesLayout.calculateTabletIntervalPadding(totalWidth, /*isHalfMvt=*/false);
        Assert.assertEquals("The result of function CalculateTabletIntervalPadding is wrong",
                expectedIntervalPadding, intervalPadding);

        totalWidth = 1528;
        expectedIntervalPadding = 35;
        intervalPadding =
                mMvTilesLayout.calculateTabletIntervalPadding(totalWidth, /*isHalfMvt=*/false);
        Assert.assertEquals("The result of function CalculateTabletIntervalPadding is wrong",
                expectedIntervalPadding, intervalPadding);

        tileNum = 12;
        mMvTilesLayout.setInitialTileNumForTesting(tileNum);

        totalWidth = 1472;
        expectedIntervalPadding = 38;
        intervalPadding =
                mMvTilesLayout.calculateTabletIntervalPadding(totalWidth, /*isHalfMvt=*/true);
        Assert.assertEquals("The result of function CalculateTabletIntervalPadding is wrong",
                expectedIntervalPadding, intervalPadding);

        totalWidth = 1496;
        expectedIntervalPadding = 42;
        intervalPadding =
                mMvTilesLayout.calculateTabletIntervalPadding(totalWidth, /*isHalfMvt=*/true);
        Assert.assertEquals("The result of function CalculateTabletIntervalPadding is wrong",
                expectedIntervalPadding, intervalPadding);

        tileNum = 3;
        mMvTilesLayout.setInitialTileNumForTesting(tileNum);

        totalWidth = 1504;
        expectedIntervalPadding = 96;
        intervalPadding =
                mMvTilesLayout.calculateTabletIntervalPadding(totalWidth, /*isHalfMvt=*/false);
        Assert.assertEquals("The result of function CalculateTabletIntervalPadding is wrong",
                expectedIntervalPadding, intervalPadding);

        totalWidth = 1528;
        expectedIntervalPadding = 96;
        intervalPadding =
                mMvTilesLayout.calculateTabletIntervalPadding(totalWidth, /*isHalfMvt=*/false);
        Assert.assertEquals("The result of function CalculateTabletIntervalPadding is wrong",
                expectedIntervalPadding, intervalPadding);
    }
}
