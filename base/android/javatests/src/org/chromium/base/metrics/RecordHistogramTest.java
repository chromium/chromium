// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;

/**
 * Tests for the Java API for recording UMA histograms.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class RecordHistogramTest {
    @Before
    public void setUp() {
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
    }

    /**
     * Tests recording of boolean histograms.
     */
    @Test
    @SmallTest
    public void testRecordBooleanHistogram() {
        String histogram = "HelloWorld.BooleanMetric";
        HistogramDelta falseCount = new HistogramDelta(histogram, 0);
        HistogramDelta trueCount = new HistogramDelta(histogram, 1);
        Assert.assertEquals(0, trueCount.getDelta());
        Assert.assertEquals(0, falseCount.getDelta());

        RecordHistogram.recordBooleanHistogram(histogram, true);
        Assert.assertEquals(1, trueCount.getDelta());
        Assert.assertEquals(0, falseCount.getDelta());

        RecordHistogram.recordBooleanHistogram(histogram, true);
        Assert.assertEquals(2, trueCount.getDelta());
        Assert.assertEquals(0, falseCount.getDelta());

        RecordHistogram.recordBooleanHistogram(histogram, false);
        Assert.assertEquals(2, trueCount.getDelta());
        Assert.assertEquals(1, falseCount.getDelta());
    }

    /**
     * Tests recording of enumerated histograms.
     */
    @Test
    @SmallTest
    public void testRecordEnumeratedHistogram() {
        String histogram = "HelloWorld.EnumeratedMetric";
        HistogramDelta zeroCount = new HistogramDelta(histogram, 0);
        HistogramDelta oneCount = new HistogramDelta(histogram, 1);
        HistogramDelta twoCount = new HistogramDelta(histogram, 2);
        final int boundary = 3;

        Assert.assertEquals(0, zeroCount.getDelta());
        Assert.assertEquals(0, oneCount.getDelta());
        Assert.assertEquals(0, twoCount.getDelta());

        RecordHistogram.recordEnumeratedHistogram(histogram, 0, boundary);
        Assert.assertEquals(1, zeroCount.getDelta());
        Assert.assertEquals(0, oneCount.getDelta());
        Assert.assertEquals(0, twoCount.getDelta());

        RecordHistogram.recordEnumeratedHistogram(histogram, 0, boundary);
        Assert.assertEquals(2, zeroCount.getDelta());
        Assert.assertEquals(0, oneCount.getDelta());
        Assert.assertEquals(0, twoCount.getDelta());

        RecordHistogram.recordEnumeratedHistogram(histogram, 2, boundary);
        Assert.assertEquals(2, zeroCount.getDelta());
        Assert.assertEquals(0, oneCount.getDelta());
        Assert.assertEquals(1, twoCount.getDelta());
    }

    /**
     * Tests recording of count histograms.
     */
    @Test
    @SmallTest
    public void testRecordCountHistogram() {
        String histogram = "HelloWorld.CountMetric";
        HistogramDelta zeroCount = new HistogramDelta(histogram, 0);
        HistogramDelta oneCount = new HistogramDelta(histogram, 1);
        HistogramDelta twoCount = new HistogramDelta(histogram, 2);
        HistogramDelta eightThousandCount = new HistogramDelta(histogram, 8000);

        Assert.assertEquals(0, zeroCount.getDelta());
        Assert.assertEquals(0, oneCount.getDelta());
        Assert.assertEquals(0, twoCount.getDelta());
        Assert.assertEquals(0, eightThousandCount.getDelta());

        RecordHistogram.recordCountHistogram(histogram, 0);
        Assert.assertEquals(1, zeroCount.getDelta());
        Assert.assertEquals(0, oneCount.getDelta());
        Assert.assertEquals(0, twoCount.getDelta());
        Assert.assertEquals(0, eightThousandCount.getDelta());

        RecordHistogram.recordCountHistogram(histogram, 0);
        Assert.assertEquals(2, zeroCount.getDelta());
        Assert.assertEquals(0, oneCount.getDelta());
        Assert.assertEquals(0, twoCount.getDelta());
        Assert.assertEquals(0, eightThousandCount.getDelta());

        RecordHistogram.recordCountHistogram(histogram, 2);
        Assert.assertEquals(2, zeroCount.getDelta());
        Assert.assertEquals(0, oneCount.getDelta());
        Assert.assertEquals(1, twoCount.getDelta());
        Assert.assertEquals(0, eightThousandCount.getDelta());

        RecordHistogram.recordCountHistogram(histogram, 8000);
        Assert.assertEquals(2, zeroCount.getDelta());
        Assert.assertEquals(0, oneCount.getDelta());
        Assert.assertEquals(1, twoCount.getDelta());
        Assert.assertEquals(1, eightThousandCount.getDelta());
    }

    /**
     * Tests recording of custom times histograms.
     */
    @Test
    @SmallTest
    public void testRecordCustomTimesHistogram() {
        String histogram = "HelloWorld.CustomTimesMetric";
        HistogramDelta zeroCount = new HistogramDelta(histogram, 0);
        HistogramDelta oneCount = new HistogramDelta(histogram, 1);
        HistogramDelta twoCount = new HistogramDelta(histogram, 100);

        Assert.assertEquals(0, zeroCount.getDelta());
        Assert.assertEquals(0, oneCount.getDelta());
        Assert.assertEquals(0, twoCount.getDelta());

        RecordHistogram.recordCustomTimesHistogram(histogram, 0, 1, 100, 3);
        Assert.assertEquals(1, zeroCount.getDelta());
        Assert.assertEquals(0, oneCount.getDelta());
        Assert.assertEquals(0, twoCount.getDelta());

        RecordHistogram.recordCustomTimesHistogram(histogram, 0, 1, 100, 3);
        Assert.assertEquals(2, zeroCount.getDelta());
        Assert.assertEquals(0, oneCount.getDelta());
        Assert.assertEquals(0, twoCount.getDelta());

        RecordHistogram.recordCustomTimesHistogram(histogram, 95, 1, 100, 3);
        Assert.assertEquals(2, zeroCount.getDelta());
        Assert.assertEquals(1, oneCount.getDelta());
        Assert.assertEquals(0, twoCount.getDelta());

        RecordHistogram.recordCustomTimesHistogram(histogram, 200, 1, 100, 3);
        Assert.assertEquals(2, zeroCount.getDelta());
        Assert.assertEquals(1, oneCount.getDelta());
        Assert.assertEquals(1, twoCount.getDelta());
    }

    /**
     * Tests recording of linear count histograms.
     */
    @Test
    @SmallTest
    public void testRecordLinearCountHistogram() {
        String histogram = "HelloWorld.LinearCountMetric";
        HistogramDelta zeroCount = new HistogramDelta(histogram, 0);
        HistogramDelta oneCount = new HistogramDelta(histogram, 1);
        HistogramDelta twoCount = new HistogramDelta(histogram, 2);
        final int min = 1;
        final int max = 3;
        final int numBuckets = 4;

        Assert.assertEquals(0, zeroCount.getDelta());
        Assert.assertEquals(0, oneCount.getDelta());
        Assert.assertEquals(0, twoCount.getDelta());

        RecordHistogram.recordLinearCountHistogram(histogram, 0, min, max, numBuckets);
        Assert.assertEquals(1, zeroCount.getDelta());
        Assert.assertEquals(0, oneCount.getDelta());
        Assert.assertEquals(0, twoCount.getDelta());

        RecordHistogram.recordLinearCountHistogram(histogram, 0, min, max, numBuckets);
        Assert.assertEquals(2, zeroCount.getDelta());
        Assert.assertEquals(0, oneCount.getDelta());
        Assert.assertEquals(0, twoCount.getDelta());

        RecordHistogram.recordLinearCountHistogram(histogram, 2, min, max, numBuckets);
        Assert.assertEquals(2, zeroCount.getDelta());
        Assert.assertEquals(0, oneCount.getDelta());
        Assert.assertEquals(1, twoCount.getDelta());
    }
}
