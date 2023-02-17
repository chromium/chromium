// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.metrics;

import androidx.test.filters.MediumTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.HistogramWatcher;

/**
 * Tests the {@link HistogramWatcher} test util with a transition to native.
 *
 * The transition is significant because the first histogram snapshot is taken through
 * CachingUmaRecorder, but the second is taken through NativeUmaRecorder, triggering the more
 * complex logic to calculate a delta with different bucket sizes.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@DoNotBatch(reason = "Requires loading native at specific times")
public class HistogramWatcherTransitionToNativeTest extends HistogramWatcherTestBase {
    @Test
    @MediumTest
    public void testSingleRecordMissing_failure() {
        doTestSingleRecordMissing_failure(TestScenario.TRANSITION_TO_NATIVE);
    }

    @Test
    @MediumTest
    public void testFourTimesHistograms_success() {
        doTestFourTimesHistograms_success(TestScenario.TRANSITION_TO_NATIVE);
    }

    @Test
    @MediumTest
    public void testExtraRecord_failure() {
        doTestExtraRecord_failure(TestScenario.TRANSITION_TO_NATIVE);
    }

    @Test
    @MediumTest
    public void testExtraRecordAllowed_success() {
        doTestExtraRecordAllowed_success(TestScenario.TRANSITION_TO_NATIVE);
    }

    @Test
    @MediumTest
    public void testExtraRecordAllowed_failure() {
        doTestExtraRecordAllowed_failure(TestScenario.TRANSITION_TO_NATIVE);
    }

    @Test
    @MediumTest
    public void testMissingRecord_failure() {
        doTestMissingLastRecord_failure(TestScenario.TRANSITION_TO_NATIVE);
    }

    @Test
    @MediumTest
    public void testExpectNoRecords_failure() {
        doTestExpectNoRecords_failure(TestScenario.TRANSITION_TO_NATIVE);
    }

    @Test
    @MediumTest
    public void testExpectAnyRecords_missing_failure() {
        doTestExpectAnyRecords_missing_failure(TestScenario.TRANSITION_TO_NATIVE);
    }

    @Test
    @MediumTest
    public void testExpectAnyRecords_extras_failure() {
        doTestExpectAnyRecords_extras_failure(TestScenario.TRANSITION_TO_NATIVE);
    }

    @Test
    @MediumTest
    public void testExpectAnyRecords_success() {
        doTestExpectAnyRecords_success(TestScenario.TRANSITION_TO_NATIVE);
    }

    @Test
    @MediumTest
    public void testMultipleHistograms_success() {
        doTestMultipleHistograms_success(TestScenario.TRANSITION_TO_NATIVE);
    }

    @Test
    @MediumTest
    public void testMultipleHistograms_failure() {
        doTestMultipleHistograms_failure(TestScenario.TRANSITION_TO_NATIVE);
    }

    @Test
    @MediumTest
    public void testIgnoreOtherHistograms_success() {
        doTestIgnoreOtherHistograms_success(TestScenario.TRANSITION_TO_NATIVE);
    }

    @Test
    @MediumTest
    public void testMissingFirstRecord_failure() {
        doTestMissingFirstRecord_failure(TestScenario.TRANSITION_TO_NATIVE);
    }

    @Test
    @MediumTest
    public void testMissingMiddleRecord_failure() {
        doTestMissingMiddleRecord_failure(TestScenario.TRANSITION_TO_NATIVE);
    }
}
