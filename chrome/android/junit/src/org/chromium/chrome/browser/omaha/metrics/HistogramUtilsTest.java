// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.metrics;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for HistogramUtils. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HistogramUtilsTest {
    /** Tests {@link HistogramUtils#recordStartedUpdateHistogram(boolean)} */
    @Test
    public void testStartHistogram() {
        HistogramUtils.recordStartedUpdateHistogram(false);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.StartingUpdateState", 0));
        UmaRecorderHolder.resetForTesting();

        HistogramUtils.recordStartedUpdateHistogram(true);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.StartingUpdateState", 1));
        UmaRecorderHolder.resetForTesting();
    }
}
