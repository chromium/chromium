// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics.test;

import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.greaterThan;
import static org.junit.Assert.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ShadowRecordHistogram}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowRecordHistogram.class})
public final class ShadowRecordHistogramTest {
    private static final String HISTOGRAM_TO_KEEP_1 = "HISTOGRAM_TO_KEEP_1";
    private static final String HISTOGRAM_TO_KEEP_2 = "HISTOGRAM_TO_KEEP_2";
    private static final String HISTOGRAM_TO_DELETE_1 = "HISTOGRAM_TO_DELETE_1";
    private static final String HISTOGRAM_TO_DELETE_2 = "HISTOGRAM_TO_DELETE_2";
    private static final boolean UNUSED = true;

    @Test
    public void testRecordAndGetForTesting() {
        // Add some
        RecordHistogram.recordBooleanHistogram(HISTOGRAM_TO_DELETE_1, UNUSED);
        RecordHistogram.recordBooleanHistogram(HISTOGRAM_TO_KEEP_1, UNUSED);
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(HISTOGRAM_TO_KEEP_1),
                greaterThan(0));
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(HISTOGRAM_TO_DELETE_1),
                greaterThan(0));
    }

    @Test
    public void testForgetHistogramForTesting() {
        // Add some
        RecordHistogram.recordBooleanHistogram(HISTOGRAM_TO_DELETE_1, UNUSED);
        RecordHistogram.recordBooleanHistogram(HISTOGRAM_TO_KEEP_1, UNUSED);
        RecordHistogram.recordBooleanHistogram(HISTOGRAM_TO_DELETE_2, UNUSED);
        RecordHistogram.recordBooleanHistogram(HISTOGRAM_TO_KEEP_2, UNUSED);
        // Remove some
        RecordHistogram.forgetHistogramForTesting(HISTOGRAM_TO_DELETE_1);
        RecordHistogram.forgetHistogramForTesting(HISTOGRAM_TO_DELETE_2);
        // Removing again should be OK
        RecordHistogram.forgetHistogramForTesting(HISTOGRAM_TO_DELETE_1);
        // Are the deleted ones gone?
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(HISTOGRAM_TO_DELETE_1),
                equalTo(0));
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(HISTOGRAM_TO_DELETE_2),
                equalTo(0));
        assertThat(RecordHistogram.getHistogramValueCountForTesting(HISTOGRAM_TO_DELETE_1, 1),
                equalTo(0));
        assertThat(RecordHistogram.getHistogramValueCountForTesting(HISTOGRAM_TO_DELETE_2, 1),
                equalTo(0));
        // Are the non-deleted ones still there?
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(HISTOGRAM_TO_KEEP_1),
                greaterThan(0));
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(HISTOGRAM_TO_KEEP_2),
                greaterThan(0));
        assertThat(RecordHistogram.getHistogramValueCountForTesting(HISTOGRAM_TO_KEEP_1, 1),
                greaterThan(0));
        assertThat(RecordHistogram.getHistogramValueCountForTesting(HISTOGRAM_TO_KEEP_2, 1),
                greaterThan(0));
    }
}
