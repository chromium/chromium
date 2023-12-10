// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.metrics;

import androidx.annotation.IntDef;

import org.junit.Assert;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.HistogramWatcher;

/**
 * Tests the {@link HistogramWatcher} test util in the following contexts:
 * - Before native load
 * - Transitioning from CachingUmaRecorder (before native load) to NativeUmaRecorder (after).
 * - After native load
 *
 * Tests are split into multiple classes so they can be batched by scenario.
 */
public class HistogramWatcherTestBase {
    protected static final String TIMES_HISTOGRAM_1 = "TimesHistogram1";
    protected static final String BOOLEAN_HISTOGRAM = "BooleanHistogram";
    protected static final String EXACT_LINEAR_HISTOGRAM_1 = "ExactLinearHistogram"; // max 10
    protected static final String EXACT_LINEAR_HISTOGRAM_2 = "ExactLinearHistogram2"; // max 20
    protected static final String EXACT_LINEAR_HISTOGRAM_3 = "ExactLinearHistogram3"; // max 30
    protected static final String ENUM_HISTOGRAM = "EnumHistogram"; // max 10

    @IntDef({
        TestScenario.WITHOUT_NATIVE,
        TestScenario.TRANSITION_TO_NATIVE,
        TestScenario.WITH_NATIVE
    })
    protected @interface TestScenario {
        int WITHOUT_NATIVE = 0;
        int TRANSITION_TO_NATIVE = 1;
        int WITH_NATIVE = 2;
    }

    protected HistogramWatcher mWatcher;

    protected void doTestSingleRecordMissing_failure(@TestScenario int scenario) {
        // Arrange
        maybeLoadNativeFirst(scenario);
        mWatcher = HistogramWatcher.newSingleRecordWatcher(TIMES_HISTOGRAM_1, 6000);

        // Act
        maybeLoadNativeAfterRecord(scenario);

        // Assert
        try {
            mWatcher.assertExpected();
        } catch (AssertionError e) {
            assertContains(TIMES_HISTOGRAM_1, e.getMessage());
            assertContains("1 record(s) expected: [6000]", e.getMessage());
            assertContains("0 record(s) seen: []", e.getMessage());
            return;
        }
        Assert.fail("Expected AssertionError");
    }

    protected void doTestFourTimesHistograms_success(@TestScenario int scenario) {
        // Arrange
        maybeLoadNativeFirst(scenario);
        mWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TIMES_HISTOGRAM_1, 6000)
                        .expectIntRecord(TIMES_HISTOGRAM_1, 7000)
                        .expectIntRecordTimes(TIMES_HISTOGRAM_1, 8000, 2)
                        .build();

        // Act
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 6000);
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 7000);
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 8000);
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 8000);
        maybeLoadNativeAfterRecord(scenario);

        // Assert
        mWatcher.assertExpected();
    }

    protected void doTestExtraRecord_failure(@TestScenario int scenario) {
        // Arrange
        maybeLoadNativeFirst(scenario);
        mWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TIMES_HISTOGRAM_1, 6000)
                        .expectIntRecord(TIMES_HISTOGRAM_1, 7000)
                        .expectIntRecordTimes(TIMES_HISTOGRAM_1, 8000, 2)
                        .build();

        // Act
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 6000);
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 7000);
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 8000);
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 8000);
        // Extra record that should break the test
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 8000);
        maybeLoadNativeAfterRecord(scenario);

        // Assert
        try {
            mWatcher.assertExpected();
        } catch (AssertionError e) {
            assertContains(TIMES_HISTOGRAM_1, e.getMessage());
            assertContains("4 record(s) expected: [6000, 7000, 8000 (2 times)]", e.getMessage());
            // Exact histogram buckets can be arbitrary
            assertContains("5 record(s) seen: [", e.getMessage());
            return;
        }
        Assert.fail("Expected AssertionError");
    }

    protected void doTestExtraRecordAllowed_success(@TestScenario int scenario) {
        // Arrange
        maybeLoadNativeFirst(scenario);
        mWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(ENUM_HISTOGRAM, 6)
                        .expectIntRecord(ENUM_HISTOGRAM, 7)
                        .expectIntRecordTimes(ENUM_HISTOGRAM, 8, 2)
                        .allowExtraRecordsForHistogramsAbove()
                        .build();

        // Act
        RecordHistogram.recordEnumeratedHistogram(ENUM_HISTOGRAM, 6, 10);
        RecordHistogram.recordEnumeratedHistogram(ENUM_HISTOGRAM, 7, 10);
        RecordHistogram.recordEnumeratedHistogram(ENUM_HISTOGRAM, 8, 10);
        RecordHistogram.recordEnumeratedHistogram(ENUM_HISTOGRAM, 8, 10);
        // Extra record that should not break the test
        RecordHistogram.recordEnumeratedHistogram(ENUM_HISTOGRAM, 8, 10);
        maybeLoadNativeAfterRecord(scenario);

        // Assert
        mWatcher.assertExpected();
    }

    protected void doTestExtraRecordAllowed_failure(@TestScenario int scenario) {
        // Arrange
        maybeLoadNativeFirst(scenario);
        mWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(ENUM_HISTOGRAM, 6)
                        .expectIntRecord(ENUM_HISTOGRAM, 7)
                        .expectIntRecordTimes(ENUM_HISTOGRAM, 8, 2)
                        .allowExtraRecordsForHistogramsAbove()
                        .build();

        // Act
        RecordHistogram.recordEnumeratedHistogram(ENUM_HISTOGRAM, 6, 10);
        RecordHistogram.recordEnumeratedHistogram(ENUM_HISTOGRAM, 7, 10);
        RecordHistogram.recordEnumeratedHistogram(ENUM_HISTOGRAM, 8, 10);
        RecordHistogram.recordEnumeratedHistogram(ENUM_HISTOGRAM, 9, 10);
        maybeLoadNativeAfterRecord(scenario);

        // Assert
        try {
            mWatcher.assertExpected();
        } catch (AssertionError e) {
            assertContains(ENUM_HISTOGRAM, e.getMessage());
            assertContains("4 record(s) expected: [6, 7, 8 (2 times)]", e.getMessage());
            // Exact histogram buckets can be arbitrary
            assertContains("4 record(s) seen: [6, 7, 8, 9]", e.getMessage());
            return;
        }
        Assert.fail("Expected AssertionError");
    }

    protected void doTestExtraRecordAllowedAny_success(@TestScenario int scenario) {
        // Arrange
        maybeLoadNativeFirst(scenario);
        mWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes(BOOLEAN_HISTOGRAM, 3)
                        .allowExtraRecordsForHistogramsAbove()
                        .build();

        // Act
        RecordHistogram.recordBooleanHistogram(BOOLEAN_HISTOGRAM, false);
        RecordHistogram.recordBooleanHistogram(BOOLEAN_HISTOGRAM, false);
        RecordHistogram.recordBooleanHistogram(BOOLEAN_HISTOGRAM, true);
        RecordHistogram.recordBooleanHistogram(BOOLEAN_HISTOGRAM, true);
        maybeLoadNativeAfterRecord(scenario);

        // Assert
        mWatcher.assertExpected();
    }

    protected void doTestExtraRecordAllowedAny_failure(@TestScenario int scenario) {
        // Arrange
        maybeLoadNativeFirst(scenario);
        mWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes(BOOLEAN_HISTOGRAM, 3)
                        .allowExtraRecordsForHistogramsAbove()
                        .build();

        // Act
        RecordHistogram.recordBooleanHistogram(BOOLEAN_HISTOGRAM, false);
        RecordHistogram.recordBooleanHistogram(BOOLEAN_HISTOGRAM, false);
        maybeLoadNativeAfterRecord(scenario);

        // Assert
        try {
            mWatcher.assertExpected();
        } catch (AssertionError e) {
            assertContains(BOOLEAN_HISTOGRAM, e.getMessage());
            assertContains("3 record(s) expected: [Any (3 times)]", e.getMessage());
            assertContains("2 record(s) seen: [0 (2 times)]", e.getMessage());
            return;
        }
        Assert.fail("Expected AssertionError");
    }

    protected void doTestMissingLastRecord_failure(@TestScenario int scenario) {
        // Arrange
        maybeLoadNativeFirst(scenario);
        mWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TIMES_HISTOGRAM_1, 6000)
                        .expectIntRecord(TIMES_HISTOGRAM_1, 7000)
                        .expectIntRecordTimes(TIMES_HISTOGRAM_1, 8000, 2)
                        .build();

        // Act
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 6000);
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 7000);
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 8000);
        maybeLoadNativeAfterRecord(scenario);

        // Assert
        try {
            mWatcher.assertExpected();
        } catch (AssertionError e) {
            assertContains(TIMES_HISTOGRAM_1, e.getMessage());
            assertContains("4 record(s) expected: [6000, 7000, 8000 (2 times)]", e.getMessage());
            // Exact histogram buckets can be arbitrary
            assertContains("3 record(s) seen: [", e.getMessage());
            return;
        }
        Assert.fail("Expected AssertionError");
    }

    protected void doTestExpectNoRecords_failure(@TestScenario int scenario) {
        // Arrange
        maybeLoadNativeFirst(scenario);
        mWatcher = HistogramWatcher.newBuilder().expectNoRecords(ENUM_HISTOGRAM).build();

        // Act
        RecordHistogram.recordEnumeratedHistogram(ENUM_HISTOGRAM, 5, 10);
        maybeLoadNativeAfterRecord(scenario);

        // Assert
        try {
            mWatcher.assertExpected();
        } catch (AssertionError e) {
            assertContains(ENUM_HISTOGRAM, e.getMessage());
            assertContains("0 record(s) expected: []", e.getMessage());
            assertContains("1 record(s) seen: [5]", e.getMessage());
            return;
        }
        Assert.fail("Expected AssertionError");
    }

    protected void doTestExpectAnyRecords_missing_failure(@TestScenario int scenario) {
        // Arrange
        maybeLoadNativeFirst(scenario);
        mWatcher = HistogramWatcher.newBuilder().expectAnyRecordTimes(BOOLEAN_HISTOGRAM, 3).build();

        // Act
        RecordHistogram.recordBooleanHistogram(BOOLEAN_HISTOGRAM, false);
        RecordHistogram.recordBooleanHistogram(BOOLEAN_HISTOGRAM, false);
        maybeLoadNativeAfterRecord(scenario);

        // Assert
        try {
            mWatcher.assertExpected();
        } catch (AssertionError e) {
            assertContains(BOOLEAN_HISTOGRAM, e.getMessage());
            assertContains("3 record(s) expected: [Any (3 times)]", e.getMessage());
            assertContains("2 record(s) seen: [0 (2 times)]", e.getMessage());
            return;
        }
        Assert.fail("Expected AssertionError");
    }

    protected void doTestExpectAnyRecords_extras_failure(@TestScenario int scenario) {
        // Arrange
        maybeLoadNativeFirst(scenario);
        mWatcher = HistogramWatcher.newBuilder().expectAnyRecordTimes(BOOLEAN_HISTOGRAM, 3).build();

        // Act
        RecordHistogram.recordBooleanHistogram(BOOLEAN_HISTOGRAM, false);
        RecordHistogram.recordBooleanHistogram(BOOLEAN_HISTOGRAM, false);
        RecordHistogram.recordBooleanHistogram(BOOLEAN_HISTOGRAM, true);
        RecordHistogram.recordBooleanHistogram(BOOLEAN_HISTOGRAM, true);
        maybeLoadNativeAfterRecord(scenario);

        // Assert
        try {
            mWatcher.assertExpected();
        } catch (AssertionError e) {
            assertContains(BOOLEAN_HISTOGRAM, e.getMessage());
            assertContains("3 record(s) expected: [Any (3 times)]", e.getMessage());
            assertContains("4 record(s) seen: [0 (2 times), 1 (2 times)]", e.getMessage());
            return;
        }
        Assert.fail("Expected AssertionError");
    }

    protected void doTestExpectAnyRecords_success(@TestScenario int scenario) {
        // Arrange
        maybeLoadNativeFirst(scenario);
        mWatcher = HistogramWatcher.newBuilder().expectAnyRecordTimes(BOOLEAN_HISTOGRAM, 3).build();

        // Act
        RecordHistogram.recordBooleanHistogram(BOOLEAN_HISTOGRAM, false);
        RecordHistogram.recordBooleanHistogram(BOOLEAN_HISTOGRAM, false);
        RecordHistogram.recordBooleanHistogram(BOOLEAN_HISTOGRAM, true);
        maybeLoadNativeAfterRecord(scenario);

        // Assert
        mWatcher.assertExpected();
    }

    protected void doTestMultipleHistograms_success(@TestScenario int scenario) {
        // Arrange
        maybeLoadNativeFirst(scenario);
        mWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(EXACT_LINEAR_HISTOGRAM_1, 5)
                        .expectIntRecord(EXACT_LINEAR_HISTOGRAM_1, 6)
                        .expectIntRecord(EXACT_LINEAR_HISTOGRAM_2, 15)
                        .expectIntRecord(EXACT_LINEAR_HISTOGRAM_2, 16)
                        .expectIntRecord(EXACT_LINEAR_HISTOGRAM_3, 25)
                        .expectIntRecord(EXACT_LINEAR_HISTOGRAM_3, 26)
                        .build();

        // Act
        RecordHistogram.recordExactLinearHistogram(EXACT_LINEAR_HISTOGRAM_1, 5, 10);
        RecordHistogram.recordExactLinearHistogram(EXACT_LINEAR_HISTOGRAM_2, 15, 20);
        RecordHistogram.recordExactLinearHistogram(EXACT_LINEAR_HISTOGRAM_3, 25, 30);
        RecordHistogram.recordExactLinearHistogram(EXACT_LINEAR_HISTOGRAM_1, 6, 10);
        RecordHistogram.recordExactLinearHistogram(EXACT_LINEAR_HISTOGRAM_2, 16, 20);
        RecordHistogram.recordExactLinearHistogram(EXACT_LINEAR_HISTOGRAM_3, 26, 30);
        maybeLoadNativeAfterRecord(scenario);

        // Assert
        mWatcher.assertExpected();
    }

    protected void doTestMultipleHistograms_failure(@TestScenario int scenario) {
        // Arrange
        maybeLoadNativeFirst(scenario);
        mWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(EXACT_LINEAR_HISTOGRAM_1, 5)
                        .expectIntRecord(EXACT_LINEAR_HISTOGRAM_1, 6)
                        .expectIntRecord(EXACT_LINEAR_HISTOGRAM_2, 15)
                        .expectIntRecord(EXACT_LINEAR_HISTOGRAM_2, 16)
                        .expectIntRecord(EXACT_LINEAR_HISTOGRAM_3, 25)
                        .expectIntRecord(EXACT_LINEAR_HISTOGRAM_3, 26)
                        .build();

        // Act
        RecordHistogram.recordExactLinearHistogram(EXACT_LINEAR_HISTOGRAM_1, 5, 10);
        RecordHistogram.recordExactLinearHistogram(EXACT_LINEAR_HISTOGRAM_2, 15, 20);
        RecordHistogram.recordExactLinearHistogram(EXACT_LINEAR_HISTOGRAM_3, 25, 30);
        RecordHistogram.recordExactLinearHistogram(EXACT_LINEAR_HISTOGRAM_1, 6, 10);
        // Miss recording EXACT_LINEAR_HISTOGRAM_2 with value 16.
        RecordHistogram.recordExactLinearHistogram(EXACT_LINEAR_HISTOGRAM_3, 26, 30);
        maybeLoadNativeAfterRecord(scenario);

        // Assert
        try {
            mWatcher.assertExpected();
        } catch (AssertionError e) {
            assertContains(EXACT_LINEAR_HISTOGRAM_2, e.getMessage());
            assertContains("2 record(s) expected: [15, 16]", e.getMessage());
            assertContains("1 record(s) seen: [15]", e.getMessage());
            return;
        }
        Assert.fail("Expected AssertionError");
    }

    protected void doTestExpectIntRecords_success(@TestScenario int scenario) {
        // Arrange
        maybeLoadNativeFirst(scenario);
        mWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(EXACT_LINEAR_HISTOGRAM_1, 5, 7, 6, 5)
                        .build();

        // Act
        RecordHistogram.recordExactLinearHistogram(EXACT_LINEAR_HISTOGRAM_1, 6, 10);
        RecordHistogram.recordExactLinearHistogram(EXACT_LINEAR_HISTOGRAM_1, 5, 10);
        RecordHistogram.recordExactLinearHistogram(EXACT_LINEAR_HISTOGRAM_1, 7, 10);
        RecordHistogram.recordExactLinearHistogram(EXACT_LINEAR_HISTOGRAM_1, 5, 10);
        maybeLoadNativeAfterRecord(scenario);

        // Assert
        mWatcher.assertExpected();
    }

    protected void doTestExpectIntRecords_failure(@TestScenario int scenario) {
        // Arrange
        maybeLoadNativeFirst(scenario);
        mWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(EXACT_LINEAR_HISTOGRAM_1, 5, 7, 6, 5)
                        .build();

        // Act
        RecordHistogram.recordExactLinearHistogram(EXACT_LINEAR_HISTOGRAM_1, 6, 10);
        RecordHistogram.recordExactLinearHistogram(EXACT_LINEAR_HISTOGRAM_1, 5, 10);
        RecordHistogram.recordExactLinearHistogram(EXACT_LINEAR_HISTOGRAM_1, 7, 10);
        // Miss recording EXACT_LINEAR_HISTOGRAM_1 with value 5.
        maybeLoadNativeAfterRecord(scenario);

        // Assert
        try {
            mWatcher.assertExpected();
        } catch (AssertionError e) {
            assertContains(EXACT_LINEAR_HISTOGRAM_1, e.getMessage());
            assertContains("4 record(s) expected: [5 (2 times), 6, 7]", e.getMessage());
            assertContains("3 record(s) seen: [5, 6, 7]", e.getMessage());
            return;
        }
        Assert.fail("Expected AssertionError");
    }

    protected void doTestIgnoreOtherHistograms_success(@TestScenario int scenario) {
        // Arrange
        maybeLoadNativeFirst(scenario);
        mWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(EXACT_LINEAR_HISTOGRAM_1, 5).build();

        // Act
        RecordHistogram.recordExactLinearHistogram(EXACT_LINEAR_HISTOGRAM_1, 5, 10);
        RecordHistogram.recordExactLinearHistogram(EXACT_LINEAR_HISTOGRAM_2, 15, 20);
        maybeLoadNativeAfterRecord(scenario);

        // Assert
        mWatcher.assertExpected();
    }

    protected void doTestMissingFirstRecord_failure(@TestScenario int scenario) {
        // Arrange
        maybeLoadNativeFirst(scenario);
        mWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TIMES_HISTOGRAM_1, 6000)
                        .expectIntRecord(TIMES_HISTOGRAM_1, 7000)
                        .expectIntRecord(TIMES_HISTOGRAM_1, 8000)
                        .build();

        // Act
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 7000);
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 8000);
        maybeLoadNativeAfterRecord(scenario);

        // Assert
        try {
            mWatcher.assertExpected();
        } catch (AssertionError e) {
            assertContains(TIMES_HISTOGRAM_1, e.getMessage());
            assertContains("3 record(s) expected: [6000, 7000, 8000]", e.getMessage());
            // Exact histogram buckets can be arbitrary
            assertContains("2 record(s) seen: [", e.getMessage());
            return;
        }
        Assert.fail("Expected AssertionError");
    }

    protected void doTestMissingMiddleRecord_failure(@TestScenario int scenario) {
        // Arrange
        maybeLoadNativeFirst(scenario);
        mWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(TIMES_HISTOGRAM_1, 6000)
                        .expectIntRecord(TIMES_HISTOGRAM_1, 7000)
                        .expectIntRecord(TIMES_HISTOGRAM_1, 8000)
                        .build();

        // Act
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 6000);
        RecordHistogram.recordTimesHistogram(TIMES_HISTOGRAM_1, 8000);
        maybeLoadNativeAfterRecord(scenario);

        // Assert
        try {
            mWatcher.assertExpected();
        } catch (AssertionError e) {
            assertContains(TIMES_HISTOGRAM_1, e.getMessage());
            assertContains("3 record(s) expected: [6000, 7000, 8000]", e.getMessage());
            // Exact histogram buckets can be arbitrary
            assertContains("2 record(s) seen: [", e.getMessage());
            return;
        }
        Assert.fail("Expected AssertionError");
    }

    protected void maybeLoadNativeAfterRecord(int scenario) {
        if (scenario == TestScenario.TRANSITION_TO_NATIVE) {
            LibraryLoader.getInstance().ensureInitialized();
        }
    }

    protected void maybeLoadNativeFirst(int scenario) {
        if (scenario == TestScenario.WITH_NATIVE) {
            LibraryLoader.getInstance().ensureInitialized();
        }
    }

    protected static void assertContains(String expectedSubstring, String actualString) {
        Assert.assertNotNull(actualString);
        if (!actualString.contains(expectedSubstring)) {
            Assert.fail(
                    String.format(
                            "Substring <%s> not found in string <%s>",
                            expectedSubstring, actualString));
        }
    }
}
