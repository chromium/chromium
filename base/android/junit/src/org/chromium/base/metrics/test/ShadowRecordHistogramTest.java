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
    private static final String HISTOGRAM = "HISTOGRAM";
    private static final boolean UNUSED = true;

    @Test
    public void testRecordAndGetForTesting() {
        // Add some
        RecordHistogram.recordBooleanHistogram(HISTOGRAM, UNUSED);
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(HISTOGRAM), greaterThan(0));
    }

    @Test
    public void testReset() {
        // Add some
        RecordHistogram.recordBooleanHistogram(HISTOGRAM, UNUSED);
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(HISTOGRAM), greaterThan(0));
        // Remove state
        ShadowRecordHistogram.reset();
        // Are the deleted ones gone?
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(HISTOGRAM), equalTo(0));

        RecordHistogram.recordBooleanHistogram(HISTOGRAM, UNUSED);
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(HISTOGRAM), equalTo(1));
    }
}
