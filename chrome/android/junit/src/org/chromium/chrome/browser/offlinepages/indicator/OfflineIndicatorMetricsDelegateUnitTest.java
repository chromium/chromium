// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link OfflineIndicatorMetricsDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class OfflineIndicatorMetricsDelegateUnitTest {
    /**
     * Fake of OfflineIndicatorMetricsDelegate.Clock used to test metrics that rely on the wall
     * time.
     */
    public static class FakeClock implements OfflineIndicatorMetricsDelegate.Clock {
        private long mCurrentTimeMillis;

        public FakeClock() {
            mCurrentTimeMillis = 0;
        }

        @Override
        public long currentTimeMillis() {
            return mCurrentTimeMillis;
        }

        public void setCurrentTimeMillis(long currentTimeMillis) {
            mCurrentTimeMillis = currentTimeMillis;
        }

        public void advanceCurrentTimeMillis(long millis) {
            mCurrentTimeMillis += millis;
        }
    }

    private FakeClock mFakeClock;

    private OfflineIndicatorMetricsDelegate mMetricsDelegate;

    @Before
    public void setUp() {
        mFakeClock = new FakeClock();
        OfflineIndicatorMetricsDelegate.setClockForTesting(mFakeClock);

        UmaRecorderHolder.resetForTesting();

        resetMetricsDelegate(/* isOffline= */ false, /* isForeground= */ true);
    }

    /**
     * Tests that when the offline indicator is shown and hidden, then we correctly track and record
     * the shown duration.
     */
    @Test
    public void testIndicatorStatusChanged() {
        // Make sure that we aren't tracking anything to start.
        assertFalse(mMetricsDelegate.isTrackingShownDuration());

        // Simulate the indicator being shown, then after some time it is hidden. In between these
        // two events, we should be tracking a shown duration.
        mMetricsDelegate.onIndicatorShown();
        assertTrue(mMetricsDelegate.isTrackingShownDuration());
        mFakeClock.advanceCurrentTimeMillis(1000);
        mMetricsDelegate.onIndicatorHidden();

        // Check that we have stopped tracking a shown duration, and we record the expected values
        // to the histograms.
        assertFalse(mMetricsDelegate.isTrackingShownDuration());
        checkUniqueSample(
                OfflineIndicatorMetricsDelegate.OFFLINE_INDICATOR_SHOWN_DURATION_V2, 1000);
    }

    /**
     * Tests that when the application state changes while the offline indicator is shown, then we
     * correctly track and record the shown duration and related metrics.
     */
    @Test
    public void testApplicationStateChanged() {
        // Make sure that we aren't tracking anything to start.
        assertFalse(mMetricsDelegate.isTrackingShownDuration());

        // Simulate the offline indicator being shown, the app being backgrounded, then the app
        // being foregrounded, and finally the offline indicator being hidden.
        mMetricsDelegate.onIndicatorShown();
        assertTrue(mMetricsDelegate.isTrackingShownDuration());
        mFakeClock.advanceCurrentTimeMillis(1000);

        mMetricsDelegate.onAppBackgrounded();
        mFakeClock.advanceCurrentTimeMillis(2000);

        mMetricsDelegate.onAppForegrounded();
        mFakeClock.advanceCurrentTimeMillis(4000);

        mMetricsDelegate.onIndicatorHidden();

        // Check that we have stopped tracking a shown duration, and we record the expected values
        // to the histograms.
        assertFalse(mMetricsDelegate.isTrackingShownDuration());
        checkUniqueSample(
                OfflineIndicatorMetricsDelegate.OFFLINE_INDICATOR_SHOWN_DURATION_V2, 7000);
    }

    /**
     * Tests that when the application state changes multiple times while the offline indicator is
     * shown, then we correctly track and record the shown duration and related metrics.
     */
    @Test
    public void testApplicationStateChanged_RepeatedStateChanges() {
        // Set test constants.
        final int numStateChanges = 10;

        // Make sure that we aren't tracking anything to start.
        assertFalse(mMetricsDelegate.isTrackingShownDuration());

        // Simluate the offline indicator being shown, and then the app switching between the
        // foreground and background multiple times. Finally simulate the offline indicator being
        // hidden.
        mMetricsDelegate.onIndicatorShown();
        assertTrue(mMetricsDelegate.isTrackingShownDuration());
        mFakeClock.advanceCurrentTimeMillis(1000);

        for (int i = 0; i < numStateChanges; i++) {
            mMetricsDelegate.onAppBackgrounded();
            mFakeClock.advanceCurrentTimeMillis(1000);

            mMetricsDelegate.onAppForegrounded();
            mFakeClock.advanceCurrentTimeMillis(1000);
        }

        mMetricsDelegate.onIndicatorHidden();

        // Check that we have stopped tracking a shown duration, and we record the expected values
        // to the histograms.
        assertFalse(mMetricsDelegate.isTrackingShownDuration());
        checkUniqueSample(
                OfflineIndicatorMetricsDelegate.OFFLINE_INDICATOR_SHOWN_DURATION_V2,
                2000 * numStateChanges + 1000);
    }

    /**
     * Tests that we record shown durations correctly even when Chrome is killed in the middle, and
     * we are offline when chrome starts up again. We simulate this by setting |mMetricsDelegate| to
     * null, and then setting it to a new instance of |OfflineIndicatorMetricsDelegate|. The new
     * instance should read the persisted state from prefs, and continue tracking the shown
     * duration.
     */
    @Test
    public void testPersistedMetrics_StartOffline() {
        assertFalse(mMetricsDelegate.isTrackingShownDuration());

        // Simulate the indicator being shown, the app being backgrounded, and then app being
        // killed (by setting |mMetricsDelegate| to null).
        mMetricsDelegate.onIndicatorShown();
        assertTrue(mMetricsDelegate.isTrackingShownDuration());
        mFakeClock.advanceCurrentTimeMillis(1000);

        mMetricsDelegate.onAppBackgrounded();
        mFakeClock.advanceCurrentTimeMillis(2000);

        mMetricsDelegate = null;
        mFakeClock.advanceCurrentTimeMillis(4000);

        // Simulate Chrome starting up, while still offline. Check that we read values from Prefs
        // and are still tracking a shown duration.
        resetMetricsDelegate(/* isOffline= */ true, /* isForeground= */ true);
        assertTrue(mMetricsDelegate.isTrackingShownDuration());

        // Finally simulate the indicator being hidden.
        mFakeClock.advanceCurrentTimeMillis(8000);
        mMetricsDelegate.onIndicatorHidden();

        // Check that we record the shown duration as the total time between when the indicator was
        // shown to when it was hidden.
        assertFalse(mMetricsDelegate.isTrackingShownDuration());
        checkUniqueSample(
                OfflineIndicatorMetricsDelegate.OFFLINE_INDICATOR_SHOWN_DURATION_V2, 15000);
    }

    /**
     * Tests that we record shown durations correctly even when Chrome is killed in the middle, and
     * we are online when chrome starts up again. We simulate this by setting |mMetricsDelegate| to
     * * null, and then setting it to a new instance of |OfflineIndicatorMetricsDelegate|. The new
     * instance should read the persisted state from prefs, and continue tracking the shown
     * duration.
     */
    @Test
    public void testPersistedMetrics_StartOnline() {
        assertFalse(mMetricsDelegate.isTrackingShownDuration());

        // Simulate the indicator being shown, the app being backgrounded, and then the app being
        // killed (by setting |mMetricsDelegate| to null).
        mMetricsDelegate.onIndicatorShown();
        assertTrue(mMetricsDelegate.isTrackingShownDuration());
        mFakeClock.advanceCurrentTimeMillis(1000);

        mMetricsDelegate.onAppBackgrounded();
        mFakeClock.advanceCurrentTimeMillis(2000);

        mMetricsDelegate = null;
        mFakeClock.advanceCurrentTimeMillis(4000);

        // Simulate Chrome starting up, while now online. In this case, we should immediately record
        // the persisted metrics and stop tracking the shown duration..
        resetMetricsDelegate(/* isOffline= */ false, /* isForeground= */ true);
        assertFalse(mMetricsDelegate.isTrackingShownDuration());
        checkUniqueSample(
                OfflineIndicatorMetricsDelegate.OFFLINE_INDICATOR_SHOWN_DURATION_V2, 7000);
    }

    /** Tests that we clear the persisted state from prefs correctly after tracking a shown duration. */
    @Test
    public void testMetricsCleared() {
        assertFalse(mMetricsDelegate.isTrackingShownDuration());

        // Simulate the indicator being shown, then after some being hidden. Check that the expected
        // samples are recorded to the histograms.
        mMetricsDelegate.onIndicatorShown();
        assertTrue(mMetricsDelegate.isTrackingShownDuration());
        mFakeClock.advanceCurrentTimeMillis(1000);
        mMetricsDelegate.onAppBackgrounded();
        mFakeClock.advanceCurrentTimeMillis(2000);
        mMetricsDelegate.onAppForegrounded();
        mFakeClock.advanceCurrentTimeMillis(4000);
        mMetricsDelegate.onIndicatorHidden();
        assertFalse(mMetricsDelegate.isTrackingShownDuration());

        checkUniqueSample(
                OfflineIndicatorMetricsDelegate.OFFLINE_INDICATOR_SHOWN_DURATION_V2, 7000);

        // After checking the histograms, clear them, so our next check only looks at new data.
        UmaRecorderHolder.resetForTesting();

        // Simulate Chrome being killed, then restarted. After restarting, check that we are not be
        // tracking a shown duration.
        mMetricsDelegate = null;
        mFakeClock.advanceCurrentTimeMillis(8000);
        resetMetricsDelegate(/* isOffline= */ false, /* isForeground= */ true);
        mFakeClock.advanceCurrentTimeMillis(16000);
        assertFalse(mMetricsDelegate.isTrackingShownDuration());

        // Simulate the indicator being shown, then hidden again. Check that we record the expected
        // value.
        mMetricsDelegate.onIndicatorShown();
        assertTrue(mMetricsDelegate.isTrackingShownDuration());
        mFakeClock.advanceCurrentTimeMillis(32000);
        mMetricsDelegate.onIndicatorHidden();
        assertFalse(mMetricsDelegate.isTrackingShownDuration());

        checkUniqueSample(
                OfflineIndicatorMetricsDelegate.OFFLINE_INDICATOR_SHOWN_DURATION_V2, 32000);
    }

    /**
     * Creates a new instance of |mMetricsDelegate| for testing, and initializes the state based on
     * the input params.
     * @param isOffline Whether |mMetricsDelegate| should start offline (true) or online (false).
     * @param isForeground Whether |mMetricsDelegate| should start foreground (true) or background
     *         (false).
     */
    private void resetMetricsDelegate(boolean isOffline, boolean isForeground) {
        mMetricsDelegate = new OfflineIndicatorMetricsDelegate();
        mMetricsDelegate.onOfflineStateInitialized(isOffline);
        if (isForeground) mMetricsDelegate.onAppForegrounded();
    }

    /**
     * Checks that the given value is the only value recorded to the given histogram.
     * @param histogramName The histogram to check.
     * @param expectedSample The expected value recorded to the histogram.
     */
    private void checkUniqueSample(String histogramName, int expectedSample) {
        assertEquals(1, RecordHistogram.getHistogramTotalCountForTesting(histogramName));
        assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(histogramName, expectedSample));
    }
}
