// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

/** Tests for the query tiles section on new tab page. */
@RunWith(BaseRobolectricTestRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class QueryTileUtilsTest {

    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    @Test
    @EnableFeatures({ChromeFeatureList.QUERY_TILES, ChromeFeatureList.QUERY_TILES_IN_NTP})
    @DisableFeatures(ChromeFeatureList.QUERY_TILES_SEGMENTATION)
    public void testIsQueryTilesEnabledOnNtpWithoutSegmentation() {
        Assert.assertTrue(QueryTileUtils.isQueryTilesEnabledOnNtp());
    }
}
