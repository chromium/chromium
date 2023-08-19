// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cryptids;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

import java.util.Random;

/**
 * Unit tests for ProbabilisticCryptidRenderer.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.PROBABILISTIC_CRYPTID_RENDERER)
public class ProbabilisticCryptidRendererUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final long PERIOD_LENGTH = 60 * 60 * 24 * 1000; // Arbitrary value of 1 day
    private static final int MAX_PROBABILITY = 200000; // Arbitrary value of 20%
    private static final int NUM_RUNS = 10000; // How many runs to use for aggregator tests.
    private static final int TOLERANCE =
            (int) Math.round(.01 * NUM_RUNS); // 1% TOLERANCE for aggregator tests.

    // Simple method-level unit tests

    @Test
    @SmallTest
    public void testCalculateProbability() {
        int probDuringMoratorium = ProbabilisticCryptidRenderer.calculateProbability(
                /* lastRenderTimestamp = */ 0,
                /* currentTimestamp = */ 1,
                /* renderingMoratoriumLength = */ 100,
                /* rampUpLength = */ 100,
                /* maxProbability = */ MAX_PROBABILITY);
        Assert.assertEquals(0, probDuringMoratorium);

        int probEarlyRampup = ProbabilisticCryptidRenderer.calculateProbability(
                /* lastRenderTimestamp = */ 0,
                /* currentTimestamp = */ 125,
                /* renderingMoratoriumLength = */ 100,
                /* rampUpLength = */ 100,
                /* maxProbability = */ MAX_PROBABILITY);
        // 125 is 25% between 100 and 200
        Assert.assertEquals((int) Math.round(MAX_PROBABILITY * .25), probEarlyRampup);

        int probLateRampup = ProbabilisticCryptidRenderer.calculateProbability(
                /* lastRenderTimestamp = */ 0,
                /* currentTimestamp = */ 180,
                /* renderingMoratoriumLength = */ 100,
                /* rampUpLength = */ 100,
                /* maxProbability = */ MAX_PROBABILITY);
        // 180 is 80% between 100 and 200
        Assert.assertEquals((int) Math.round(MAX_PROBABILITY * .8), probLateRampup);

        int probPostRampup = ProbabilisticCryptidRenderer.calculateProbability(
                /* lastRenderTimestamp = */ 0,
                /* currentTimestamp = */ 300,
                /* renderingMoratoriumLength = */ 100,
                /* rampUpLength = */ 100,
                /* maxProbability = */ MAX_PROBABILITY);
        Assert.assertEquals(MAX_PROBABILITY, probPostRampup);
    }

    @Test
    @SmallTest
    public void testPrefStorageDefaultValue() {
        ProbabilisticCryptidRenderer render = new ProbabilisticCryptidRenderer();
        long defaultRenderTimestamp = render.getLastRenderTimestampMillis();

        long howLongSinceDefaultTime = System.currentTimeMillis() - defaultRenderTimestamp;
        // We expect |howLongSinceDefaultTime| to be one moratorium length, because the default
        // case should be rigged up to start users at the end of the moratorium.
        long delta = howLongSinceDefaultTime - render.getRenderingMoratoriumLengthMillis();
        Assert.assertTrue(String.format("Delta %d was larger than 10 seconds (10000)", delta),
                delta < 10000); // Allow a 10 second grace period in case the test is slow.
    }

    @Test
    @SmallTest
    public void testPrefStorage() {
        ProbabilisticCryptidRenderer render = new ProbabilisticCryptidRenderer();
        // Set the last render event to be a crazy long time ago, ensuring max probability.
        render.recordRenderEvent(1);
        Assert.assertEquals(render.getMaxProbability(), render.calculateProbability());

        // Immediately after an event, the probability should have dropped to 0.
        render.recordRenderEvent();
        Assert.assertEquals(0, render.calculateProbability());
    }

    // Aggregator tests: these tests run |NUM_RUNS| invocations of shouldUseCryptidRendering to
    // verify that the number of trues returned is consistent with our assumptions about how often
    // this should happen, within |TOLERANCE|.

    /**
     * This fake bypasses any calls to pref logic, and also uses a seeded RNG.
     */
    private static class FakeProbabilisticCrpytidRenderer extends ProbabilisticCryptidRenderer {
        public FakeProbabilisticCrpytidRenderer(long lastRenderDeltaFromNow) {
            mLastRenderTimestamp = System.currentTimeMillis() + lastRenderDeltaFromNow;
        }

        @Override
        protected long getLastRenderTimestampMillis() {
            return mLastRenderTimestamp;
        }

        @Override
        protected long getRenderingMoratoriumLengthMillis() {
            return PERIOD_LENGTH;
        }

        @Override
        protected long getRampUpLengthMillis() {
            return PERIOD_LENGTH;
        }

        @Override
        protected int getMaxProbability() {
            return MAX_PROBABILITY;
        }

        @Override
        protected int getRandom() {
            return mRandom.nextInt(ProbabilisticCryptidRenderer.DENOMINATOR);
        }

        private long mLastRenderTimestamp;
        private Random mRandom = new Random(7); // Seeded random to reduce test flakiness.
    }

    /*
     * Runs NUM_RUNS instances of this method and returns the number of true returns.
     */
    private int shouldUseCryptidRenderingTestHelper(long lastRenderDeltaFromNow) {
        ProbabilisticCryptidRenderer render =
                new FakeProbabilisticCrpytidRenderer(lastRenderDeltaFromNow);
        int numTrueRuns = 0;
        for (int i = 0; i < NUM_RUNS; i++) {
            numTrueRuns += render.shouldUseCryptidRendering(/*profile=*/null) ? 1 : 0;
        }
        return numTrueRuns;
    }

    @Test
    @MediumTest
    public void testShouldUseCryptidRenderingInMoratorium() {
        // Last render was 0 millis ago, so expect exactly 0 true values.
        int result = shouldUseCryptidRenderingTestHelper(0);
        Assert.assertEquals(0, result);
    }

    @Test
    @MediumTest
    public void testShouldUseCryptidRenderingEarlyRampUp() {
        // Last render was moratiorium length + 25% of ramp-up length, or 1.25 PERIOD_LENGTHs, ago.
        long delta = Math.round(-1 * 1.25 * PERIOD_LENGTH);
        int expectedHits = (int) Math.round(
                .25 * MAX_PROBABILITY * NUM_RUNS / ProbabilisticCryptidRenderer.DENOMINATOR);
        int result = shouldUseCryptidRenderingTestHelper(delta);
        int error = Math.abs(expectedHits - result);
        Assert.assertTrue(
                String.format(
                        "Number of hits %d was outside acceptable range %d - %d (target was %d)",
                        result, expectedHits - TOLERANCE, expectedHits + TOLERANCE, expectedHits),
                error < TOLERANCE);
    }

    @Test
    @MediumTest
    public void testShouldUseCryptidRenderingLateRampUp() {
        // Last render was moratiorium length + 80% of ramp-up length, or 1.8 PERIOD_LENGTHs, ago.
        long delta = Math.round(-1 * 1.8 * PERIOD_LENGTH);
        int expectedHits = (int) Math.round(
                .8 * MAX_PROBABILITY * NUM_RUNS / ProbabilisticCryptidRenderer.DENOMINATOR);
        int result = shouldUseCryptidRenderingTestHelper(delta);
        int error = Math.abs(expectedHits - result);
        Assert.assertTrue(
                String.format(
                        "Number of hits %d was outside acceptable range %d - %d (target was %d)",
                        result, expectedHits - TOLERANCE, expectedHits + TOLERANCE, expectedHits),
                error < TOLERANCE);
    }

    @Test
    @MediumTest
    public void testShouldUseCryptidRenderingPostRampUp() {
        // Last render was moratiorium length + 120% of ramp-up length, or 2.2 PERIOD_LENGTHs, ago.
        long delta = Math.round(-1 * 2.2 * PERIOD_LENGTH);
        int expectedHits = MAX_PROBABILITY * NUM_RUNS / ProbabilisticCryptidRenderer.DENOMINATOR;
        int result = shouldUseCryptidRenderingTestHelper(delta);
        int error = Math.abs(expectedHits - result);
        Assert.assertTrue(
                String.format(
                        "Number of hits %d was outside acceptable range %d - %d (target was %d)",
                        result, expectedHits - TOLERANCE, expectedHits + TOLERANCE, expectedHits),
                error < TOLERANCE);
    }
}
