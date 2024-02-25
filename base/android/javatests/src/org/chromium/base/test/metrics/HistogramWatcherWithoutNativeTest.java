// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.metrics;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;

/**
 * Tests the {@link HistogramWatcher} test util before native load.
 *
 * Contains exclusive tests that aren't run in all scenarios.
 *
 * Both histogram snapshots are taken through CachingUmaRecorder, so the deltas are calculated
 * across buckets of a single value, since CachingUmaRecorder stores the raw values.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class HistogramWatcherWithoutNativeTest extends HistogramWatcherTestBase {
    @Test
    @MediumTest
    public void testSingleRecordMissing_failure() {
        doTestSingleRecordMissing_failure(TestScenario.WITHOUT_NATIVE);
    }

    @Test
    @MediumTest
    public void testFourTimesHistograms_success() {
        doTestFourTimesHistograms_success(TestScenario.WITHOUT_NATIVE);
    }

    @Test
    @MediumTest
    public void testExtraRecord_failure() {
        doTestExtraRecord_failure(TestScenario.WITHOUT_NATIVE);
    }

    @Test
    @MediumTest
    public void testExtraRecordAllowed_success() {
        doTestExtraRecordAllowed_success(TestScenario.WITHOUT_NATIVE);
    }

    @Test
    @MediumTest
    public void testExtraRecordAllowed_failure() {
        doTestExtraRecordAllowed_failure(TestScenario.WITHOUT_NATIVE);
    }

    @Test
    @MediumTest
    public void testExtraRecordAllowedAny_success() {
        doTestExtraRecordAllowedAny_success(TestScenario.WITHOUT_NATIVE);
    }

    @Test
    @MediumTest
    public void testExtraRecordAllowedAny_failure() {
        doTestExtraRecordAllowedAny_failure(TestScenario.WITHOUT_NATIVE);
    }

    @Test
    @MediumTest
    public void testMissingRecord_failure() {
        doTestMissingLastRecord_failure(TestScenario.WITHOUT_NATIVE);
    }

    @Test
    @MediumTest
    public void testExpectNoRecords_failure() {
        doTestExpectNoRecords_failure(TestScenario.WITHOUT_NATIVE);
    }

    @Test
    @MediumTest
    public void testExpectAnyRecords_missing_failure() {
        doTestExpectAnyRecords_missing_failure(TestScenario.WITHOUT_NATIVE);
    }

    @Test
    @MediumTest
    public void testExpectAnyRecords_extras_failure() {
        doTestExpectAnyRecords_extras_failure(TestScenario.WITHOUT_NATIVE);
    }

    @Test
    @MediumTest
    public void testExpectAnyRecords_success() {
        doTestExpectAnyRecords_success(TestScenario.WITHOUT_NATIVE);
    }

    @Test
    @MediumTest
    public void testMultipleHistograms_success() {
        doTestMultipleHistograms_success(TestScenario.WITHOUT_NATIVE);
    }

    @Test
    @MediumTest
    public void testMultipleHistograms_failure() {
        doTestMultipleHistograms_failure(TestScenario.WITHOUT_NATIVE);
    }

    @Test
    @MediumTest
    public void testExpectIntRecords_success() {
        doTestExpectIntRecords_success(TestScenario.WITHOUT_NATIVE);
    }

    @Test
    @MediumTest
    public void testExpectIntRecords_failure() {
        doTestExpectIntRecords_failure(TestScenario.WITHOUT_NATIVE);
    }

    @Test
    @MediumTest
    public void testIgnoreOtherHistograms_success() {
        doTestIgnoreOtherHistograms_success(TestScenario.WITHOUT_NATIVE);
    }

    @Test
    @MediumTest
    public void testMissingFirstRecord_failure() {
        doTestMissingFirstRecord_failure(TestScenario.WITHOUT_NATIVE);
    }

    @Test
    @MediumTest
    public void testMissingMiddleRecord_failure() {
        doTestMissingMiddleRecord_failure(TestScenario.WITHOUT_NATIVE);
    }

    // Exclusive tests

    @Test
    @MediumTest
    public void testCustomMessage() {
        // Arrange
        mWatcher = HistogramWatcher.newSingleRecordWatcher(BOOLEAN_HISTOGRAM, true);

        // Act
        RecordHistogram.recordBooleanHistogram(BOOLEAN_HISTOGRAM, false);

        // Assert
        String customMessage = "Test Custom Message";
        try {
            mWatcher.assertExpected(customMessage);
        } catch (AssertionError e) {
            assertContains(customMessage, e.getMessage());
            assertContains(BOOLEAN_HISTOGRAM, e.getMessage());
            assertContains("1 record(s) expected: [1]", e.getMessage());
            assertContains("1 record(s) seen: [0]", e.getMessage());
            return;
        }
        Assert.fail("Expected AssertionError");
    }

    @Test
    @MediumTest
    public void testOutOfOrderExpectations_success() {
        // Arrange
        mWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TIMES_HISTOGRAM_1, 8000)
                        .expectIntRecord(TIMES_HISTOGRAM_1, 6000)
                        .expectIntRecord(TIMES_HISTOGRAM_1, 7000)
                        .build();

        // Act
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 6000);
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 7000);
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 8000);

        // Assert
        mWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testOutOfOrderExpectations_failure() {
        // Arrange
        mWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TIMES_HISTOGRAM_1, 8000)
                        .expectIntRecord(TIMES_HISTOGRAM_1, 6000)
                        .expectIntRecord(TIMES_HISTOGRAM_1, 7000)
                        .build();

        // Act
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 7000);
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 8000);

        // Assert
        try {
            mWatcher.assertExpected();
        } catch (AssertionError e) {
            assertContains(TIMES_HISTOGRAM_1, e.getMessage());
            assertContains("3 record(s) expected: [6000, 7000, 8000]", e.getMessage());
            assertContains("2 record(s) seen: [7000, 8000]", e.getMessage());
            return;
        }
        Assert.fail("Expected AssertionError");
    }

    @Test
    @MediumTest
    public void testZeroCountExpectations_failure() {
        try {
            mWatcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecordTimes(TIMES_HISTOGRAM_1, 1, 0)
                            .build();
        } catch (IllegalArgumentException e) {
            assertContains("zero", e.getMessage());
            return;
        }
        Assert.fail("Expected IllegalArgumentException");
    }

    @Test
    @MediumTest
    public void testNegativeCountExpectations_failure() {
        try {
            mWatcher =
                    HistogramWatcher.newBuilder()
                            .expectIntRecordTimes(TIMES_HISTOGRAM_1, 1, -1)
                            .build();
        } catch (IllegalArgumentException e) {
            assertContains("negative", e.getMessage());
            return;
        }
        Assert.fail("Expected IllegalArgumentException");
    }

    @Test
    @MediumTest
    public void testTryWithResources_success() {
        try (HistogramWatcher ignored = HistogramWatcher.newSingleRecordWatcher(ENUM_HISTOGRAM)) {
            RecordHistogram.recordEnumeratedHistogram(ENUM_HISTOGRAM, 0, 10);
        }
    }

    @Test
    @MediumTest
    public void testTryWithResources_failure() {
        try (HistogramWatcher ignored = HistogramWatcher.newSingleRecordWatcher(ENUM_HISTOGRAM)) {
        } catch (AssertionError e) {
            assertContains(ENUM_HISTOGRAM, e.getMessage());
            return;
        }
        Assert.fail("Expected AssertionError");
    }
}
