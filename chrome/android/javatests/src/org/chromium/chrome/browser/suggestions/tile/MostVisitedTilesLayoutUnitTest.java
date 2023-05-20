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
        int tileNum = 8;
        mMvTilesLayout.setInitialTileNumForTesting(tileNum);

        int tileViewWidth = 160;
        mMvTilesLayout.setTileViewWidthForTesting(tileViewWidth);
        int tileViewMinIntervalPaddingTablet = 8;
        mMvTilesLayout.setTileViewMinIntervalPaddingTabletForTesting(
                tileViewMinIntervalPaddingTablet);
        int tileViewMaxIntervalPaddingTablet = 128;
        mMvTilesLayout.setTileViewMaxIntervalPaddingTabletForTesting(
                tileViewMaxIntervalPaddingTablet);

        int totalWidth = 1392;
        int expectedIntervalPadding = 72;
        int intervalPadding = mMvTilesLayout.calculateTabletIntervalPadding(totalWidth);
        Assert.assertEquals("The result of function CalculateTabletIntervalPadding is wrong",
                expectedIntervalPadding, intervalPadding);

        totalWidth = 1416;
        expectedIntervalPadding = 76;
        intervalPadding = mMvTilesLayout.calculateTabletIntervalPadding(totalWidth);
        Assert.assertEquals("The result of function CalculateTabletIntervalPadding is wrong",
                expectedIntervalPadding, intervalPadding);

        tileViewWidth = 100;
        mMvTilesLayout.setTileViewWidthForTesting(tileViewWidth);
        tileViewMinIntervalPaddingTablet = 5;
        mMvTilesLayout.setTileViewMinIntervalPaddingTabletForTesting(
                tileViewMinIntervalPaddingTablet);
        tileViewMaxIntervalPaddingTablet = 80;
        mMvTilesLayout.setTileViewMaxIntervalPaddingTabletForTesting(
                tileViewMaxIntervalPaddingTablet);

        totalWidth = 930;
        expectedIntervalPadding = 55;
        intervalPadding = mMvTilesLayout.calculateTabletIntervalPadding(totalWidth);
        Assert.assertEquals("The result of function CalculateTabletIntervalPadding is wrong",
                expectedIntervalPadding, intervalPadding);

        totalWidth = 1270;
        expectedIntervalPadding = 81;
        intervalPadding = mMvTilesLayout.calculateTabletIntervalPadding(totalWidth);
        Assert.assertEquals("The result of function CalculateTabletIntervalPadding is wrong",
                expectedIntervalPadding, intervalPadding);
    }
}
