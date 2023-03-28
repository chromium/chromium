// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.metrics;

import androidx.test.filters.MediumTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;

/**
 * Tests the {@link HistogramWatcher} test util after native load.
 *
 * Both histogram snapshots are taken through NativeUmaRecorder, so the deltas are calculated
 * across the less granular buckets that native generates.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class HistogramWatcherWithNativeTest extends HistogramWatcherTestBase {
    @Test
    @MediumTest
    public void testSingleRecordMissing_failure() {
        doTestSingleRecordMissing_failure(TestScenario.WITH_NATIVE);
    }

    @Test
    @MediumTest
    public void testFourTimesHistograms_success() {
        doTestFourTimesHistograms_success(TestScenario.WITH_NATIVE);
    }

    @Test
    @MediumTest
    public void testExtraRecord_failure() {
        doTestExtraRecord_failure(TestScenario.WITH_NATIVE);
    }

    @Test
    @MediumTest
    public void testExtraRecordAllowed_success() {
        doTestExtraRecordAllowed_success(TestScenario.WITH_NATIVE);
    }

    @Test
    @MediumTest
    public void testExtraRecordAllowed_failure() {
        doTestExtraRecordAllowed_failure(TestScenario.WITH_NATIVE);
    }

    @Test
    @MediumTest
    public void testExtraRecordAllowedAny_success() {
        doTestExtraRecordAllowedAny_success(TestScenario.WITH_NATIVE);
    }

    @Test
    @MediumTest
    public void testExtraRecordAllowedAny_failure() {
        doTestExtraRecordAllowedAny_failure(TestScenario.WITH_NATIVE);
    }

    @Test
    @MediumTest
    public void testMissingRecord_failure() {
        doTestMissingLastRecord_failure(TestScenario.WITH_NATIVE);
    }

    @Test
    @MediumTest
    public void testExpectNoRecords_failure() {
        doTestExpectNoRecords_failure(TestScenario.WITH_NATIVE);
    }

    @Test
    @MediumTest
    public void testExpectAnyRecords_missing_failure() {
        doTestExpectAnyRecords_missing_failure(TestScenario.WITH_NATIVE);
    }

    @Test
    @MediumTest
    public void testExpectAnyRecords_extras_failure() {
        doTestExpectAnyRecords_extras_failure(TestScenario.WITH_NATIVE);
    }

    @Test
    @MediumTest
    public void testExpectAnyRecords_success() {
        doTestExpectAnyRecords_success(TestScenario.WITH_NATIVE);
    }

    @Test
    @MediumTest
    public void testMultipleHistograms_success() {
        doTestMultipleHistograms_success(TestScenario.WITH_NATIVE);
    }

    @Test
    @MediumTest
    public void testMultipleHistograms_failure() {
        doTestMultipleHistograms_failure(TestScenario.WITH_NATIVE);
    }

    @Test
    @MediumTest
    public void testIgnoreOtherHistograms_success() {
        doTestIgnoreOtherHistograms_success(TestScenario.WITH_NATIVE);
    }

    @Test
    @MediumTest
    public void testExpectIntRecords_success() {
        doTestExpectIntRecords_success(TestScenario.WITH_NATIVE);
    }

    @Test
    @MediumTest
    public void testExpectIntRecords_failure() {
        doTestExpectIntRecords_failure(TestScenario.WITH_NATIVE);
    }

    @Test
    @MediumTest
    public void testMissingFirstRecord_failure() {
        doTestMissingFirstRecord_failure(TestScenario.WITH_NATIVE);
    }

    @Test
    @MediumTest
    public void testMissingMiddleRecord_failure() {
        doTestMissingMiddleRecord_failure(TestScenario.WITH_NATIVE);
    }
}
