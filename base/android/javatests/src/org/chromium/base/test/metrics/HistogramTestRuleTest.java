// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.metrics;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.model.Statement;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;

/**
 * Tests HistogramTestRule.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class HistogramTestRuleTest {
    @Before
    public void setUp() {
        LibraryLoader.getInstance().ensureInitialized();
    }

    /**
     * Tests that histogram counts are reset between test runs.
     */
    @Test
    @SmallTest
    public void testResetBetweenTestRuns() {
        String histogram = "HelloWorld.BooleanMetric";

        final HistogramTestRule rule = new HistogramTestRule();
        rule.apply(new Statement() {
            @Override
            public void evaluate() {
                assertEquals(0, rule.getHistogramTotalCount(histogram));
                assertEquals(0, rule.getHistogramValueCount(histogram, 0));
                assertEquals(0, rule.getHistogramValueCount(histogram, 1));

                RecordHistogram.recordBooleanHistogram(histogram, true);
                assertEquals(1, rule.getHistogramTotalCount(histogram));
                assertEquals(0, rule.getHistogramValueCount(histogram, 0));
                assertEquals(1, rule.getHistogramValueCount(histogram, 1));

                RecordHistogram.recordBooleanHistogram(histogram, true);
                assertEquals(2, rule.getHistogramTotalCount(histogram));
                assertEquals(0, rule.getHistogramValueCount(histogram, 0));
                assertEquals(2, rule.getHistogramValueCount(histogram, 1));

                RecordHistogram.recordBooleanHistogram(histogram, false);
                assertEquals(3, rule.getHistogramTotalCount(histogram));
                assertEquals(1, rule.getHistogramValueCount(histogram, 0));
                assertEquals(2, rule.getHistogramValueCount(histogram, 1));
            }
        }, null);

        rule.apply(new Statement() {
            @Override
            public void evaluate() {
                assertEquals(0, rule.getHistogramTotalCount(histogram));
                assertEquals(0, rule.getHistogramValueCount(histogram, 0));
                assertEquals(0, rule.getHistogramValueCount(histogram, 1));

                RecordHistogram.recordBooleanHistogram(histogram, true);
                assertEquals(1, rule.getHistogramTotalCount(histogram));
                assertEquals(0, rule.getHistogramValueCount(histogram, 0));
                assertEquals(1, rule.getHistogramValueCount(histogram, 1));
            }
        }, null);
    }
}
