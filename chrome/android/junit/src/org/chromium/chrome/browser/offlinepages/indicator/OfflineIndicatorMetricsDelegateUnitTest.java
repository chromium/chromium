// Copyright 2021 The Chromium Authors. All rights reserved.
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

import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for {@link OfflineIndicatorMetricsDelegate}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
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

        ShadowRecordHistogram.reset();

        mMetricsDelegate = new OfflineIndicatorMetricsDelegate();
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
        checkHistograms(1000);
    }

    /**
     * Tests that we record shown durations correctly even when Chrome is killed in the middle. We
     * simulate this by setting |mMetricsDelegate| to null, and then setting it to a new instance of
     * |OfflineIndicatorMetricsDelegate|. The new instance should read the persisted state from
     * prefs, and continue tracking the shown duration.
     */
    @Test
    public void testPersistedMetrics() {
        assertFalse(mMetricsDelegate.isTrackingShownDuration());

        // Simulate the indicator being shown, then Chrome being killed (by setting
        // |mMetricsDelegate| to null).
        mMetricsDelegate.onIndicatorShown();
        assertTrue(mMetricsDelegate.isTrackingShownDuration());
        mFakeClock.advanceCurrentTimeMillis(1000);
        mMetricsDelegate = null;
        mFakeClock.advanceCurrentTimeMillis(2000);

        // Simulate Chrome starting up. Check that we read values from Prefs and are still tracking
        // a shown duration.
        mMetricsDelegate = new OfflineIndicatorMetricsDelegate();
        mMetricsDelegate.onOfflineStateInitialized(/*isOffline=*/true);
        assertTrue(mMetricsDelegate.isTrackingShownDuration());

        // Finally simulate the indicator being hidden.
        mFakeClock.advanceCurrentTimeMillis(4000);
        mMetricsDelegate.onIndicatorHidden();

        // Check that we record the shown duration as the total time between when the indicator was
        // shown to when it was hidden.
        assertFalse(mMetricsDelegate.isTrackingShownDuration());
        checkHistograms(7000);
    }

    /**
     * Tests that we clear the persisted state from prefs correctly after tracking a shown duration.
     */
    @Test
    public void testMetricsCleared() {
        assertFalse(mMetricsDelegate.isTrackingShownDuration());

        // Simulate the indicator being shown, then after some being hidden. Check that the expected
        // samples are recorded to the histograms.
        mMetricsDelegate.onIndicatorShown();
        assertTrue(mMetricsDelegate.isTrackingShownDuration());
        mFakeClock.advanceCurrentTimeMillis(1000);
        mMetricsDelegate.onIndicatorHidden();
        assertFalse(mMetricsDelegate.isTrackingShownDuration());
        checkHistograms(1000);

        // After checking the histograms, clear them, so our next check only looks at new data.
        ShadowRecordHistogram.reset();

        // Simulate Chrome being killed, then restarted. After restarting, check that we are not be
        // tracking a shown duration.
        mMetricsDelegate = null;
        mFakeClock.advanceCurrentTimeMillis(2000);
        mMetricsDelegate = new OfflineIndicatorMetricsDelegate();
        mMetricsDelegate.onOfflineStateInitialized(/*isOffline=*/false);
        mFakeClock.advanceCurrentTimeMillis(4000);
        assertFalse(mMetricsDelegate.isTrackingShownDuration());

        // Simulate the indicator being shown, then hidden again. Check that we record teh expected
        // value.
        mMetricsDelegate.onIndicatorShown();
        assertTrue(mMetricsDelegate.isTrackingShownDuration());
        mFakeClock.advanceCurrentTimeMillis(8000);
        mMetricsDelegate.onIndicatorHidden();
        assertFalse(mMetricsDelegate.isTrackingShownDuration());
        checkHistograms(8000);
    }

    /**
     * Checks that expected value was recorded to OfflineIndicator.ShownDurationV2.
     */
    private void checkHistograms(int expectedShownDurationMs) {
        assertEquals(1,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                        OfflineIndicatorMetricsDelegate.OFFLINE_INDICATOR_SHOWN_DURATION_V2));
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        OfflineIndicatorMetricsDelegate.OFFLINE_INDICATOR_SHOWN_DURATION_V2,
                        expectedShownDurationMs));
    }
}
