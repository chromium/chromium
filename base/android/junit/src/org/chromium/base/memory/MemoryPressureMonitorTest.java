// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.memory;

import android.content.ComponentCallbacks2;
import android.os.Looper;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.MemoryPressureLevel;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.TimeUnit;

/** Test for MemoryPressureMonitor. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MemoryPressureMonitorTest {
    private MemoryPressureMonitor mMonitor;

    private static class TestPressureCallback implements MemoryPressureCallback {
        private Integer mReportedPressure;

        public void assertCalledWith(@MemoryPressureLevel int expectedPressure) {
            Assert.assertNotNull("Callback was not called", mReportedPressure);
            Assert.assertEquals(expectedPressure, (int) mReportedPressure);
        }

        public void assertNotCalled() {
            Assert.assertNull(mReportedPressure);
        }

        public void reset() {
            mReportedPressure = null;
        }

        @Override
        public void onPressure(@MemoryPressureLevel int pressure) {
            assertNotCalled();
            mReportedPressure = pressure;
        }
    }

    private static class TestPressureSupplier implements Supplier<Integer> {
        private @MemoryPressureLevel Integer mPressure;
        private boolean mIsCalled;

        public TestPressureSupplier(@MemoryPressureLevel Integer pressure) {
            mPressure = pressure;
        }

        @Override
        public @MemoryPressureLevel Integer get() {
            assertNotCalled();
            mIsCalled = true;
            return mPressure;
        }

        public void assertCalled() {
            Assert.assertTrue(mIsCalled);
        }

        public void assertNotCalled() {
            Assert.assertFalse(mIsCalled);
        }
    }

    private static final int THROTTLING_INTERVAL_MS = 1000;

    @Before
    public void setUp() {
        // Explicitly set main thread as UiThread. Other places rely on that.
        ThreadUtils.setUiThread(Looper.getMainLooper());

        // Pause main thread to get control over when tasks are run (see runUiThreadFor()).
        ShadowLooper.pauseMainLooper();

        mMonitor = new MemoryPressureMonitor(THROTTLING_INTERVAL_MS);
        mMonitor.setCurrentPressureSupplierForTesting(null);
    }

    /** Runs all UiThread tasks posted |delayMs| in the future. */
    private void runUiThreadFor(long delayMs) {
        ShadowLooper.idleMainLooper(delayMs, TimeUnit.MILLISECONDS);
    }

    @Test
    @SmallTest
    public void testTrimLevelTranslation() {
        Integer[][] trimLevelToPressureMap = { //
            // Levels >= TRIM_MEMORY_COMPLETE map to CRITICAL.
            {ComponentCallbacks2.TRIM_MEMORY_COMPLETE + 1, MemoryPressureLevel.CRITICAL},
            {ComponentCallbacks2.TRIM_MEMORY_COMPLETE, MemoryPressureLevel.CRITICAL},

            // TRIM_MEMORY_RUNNING_CRITICAL maps to CRITICAL.
            {ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL, MemoryPressureLevel.CRITICAL},

            // Levels < TRIM_MEMORY_COMPLETE && >= TRIM_MEMORY_BACKGROUND map to MODERATE.
            {ComponentCallbacks2.TRIM_MEMORY_COMPLETE - 1, MemoryPressureLevel.MODERATE},
            {ComponentCallbacks2.TRIM_MEMORY_BACKGROUND + 1, MemoryPressureLevel.MODERATE},
            {ComponentCallbacks2.TRIM_MEMORY_BACKGROUND, MemoryPressureLevel.MODERATE},

            // Other levels are not mapped.
            {ComponentCallbacks2.TRIM_MEMORY_BACKGROUND - 1, null},
            {ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW, null},
            {ComponentCallbacks2.TRIM_MEMORY_RUNNING_MODERATE, null},
            {ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN, null}
        };
        for (Integer[] trimLevelAndPressure : trimLevelToPressureMap) {
            int trimLevel = trimLevelAndPressure[0];
            Integer expectedPressure = trimLevelAndPressure[1];
            Integer actualPressure = MemoryPressureMonitor.memoryPressureFromTrimLevel(trimLevel);
            Assert.assertEquals(expectedPressure, actualPressure);
        }
    }

    @Test
    @SmallTest
    public void testThrottleInterval() {
        TestPressureCallback callback = new TestPressureCallback();
        mMonitor.setReportingCallbackForTesting(callback);

        // First notification should go through.
        mMonitor.notifyPressure(MemoryPressureLevel.NONE);
        callback.assertCalledWith(MemoryPressureLevel.NONE);

        callback.reset();

        // This one should be throttled.
        mMonitor.notifyPressure(MemoryPressureLevel.NONE);
        callback.assertNotCalled();

        runUiThreadFor(THROTTLING_INTERVAL_MS - 1);

        // We're still within the throttling interval, so this notification should
        // still be throttled.
        mMonitor.notifyPressure(MemoryPressureLevel.NONE);
        callback.assertNotCalled();

        runUiThreadFor(1);

        // We're past the throttling interval at this point, so this notification
        // should go through.
        mMonitor.notifyPressure(MemoryPressureLevel.NONE);
        callback.assertCalledWith(MemoryPressureLevel.NONE);
    }

    @Test
    @SmallTest
    public void testChangeNotIgnored() {
        TestPressureCallback callback = new TestPressureCallback();
        mMonitor.setReportingCallbackForTesting(callback);

        mMonitor.notifyPressure(MemoryPressureLevel.NONE);
        callback.assertCalledWith(MemoryPressureLevel.NONE);

        callback.reset();

        // Second notification is throttled, but should be reported after the
        // throttling interval ends.
        mMonitor.notifyPressure(MemoryPressureLevel.MODERATE);
        callback.assertNotCalled();

        runUiThreadFor(THROTTLING_INTERVAL_MS - 1);

        // Shouldn't be reported at this point.
        callback.assertNotCalled();

        runUiThreadFor(1);

        callback.assertCalledWith(MemoryPressureLevel.MODERATE);
    }

    @Test
    @SmallTest
    public void testNoopChangeIgnored() {
        TestPressureCallback callback = new TestPressureCallback();
        mMonitor.setReportingCallbackForTesting(callback);

        mMonitor.notifyPressure(MemoryPressureLevel.NONE);
        callback.assertCalledWith(MemoryPressureLevel.NONE);

        callback.reset();

        // Report MODERATE and then NONE, so that the throttling interval finishes with the
        // same pressure that started it (i.e. NONE). In this case MODERATE should be ignored.
        mMonitor.notifyPressure(MemoryPressureLevel.MODERATE);
        mMonitor.notifyPressure(MemoryPressureLevel.NONE);

        runUiThreadFor(THROTTLING_INTERVAL_MS);

        callback.assertNotCalled();
    }

    @Test
    @SmallTest
    public void testPollingInitiallyDisabled() {
        TestPressureSupplier pressureSupplier =
                new TestPressureSupplier(MemoryPressureLevel.MODERATE);
        mMonitor.setCurrentPressureSupplierForTesting(pressureSupplier);

        mMonitor.notifyPressure(MemoryPressureLevel.CRITICAL);
        runUiThreadFor(THROTTLING_INTERVAL_MS);

        // We finished the interval with CRITICAL, but since polling is disabled, we shouldn't
        // poll the current pressure.
        pressureSupplier.assertNotCalled();
    }

    @Test
    @SmallTest
    public void testEnablePollingPolls() {
        TestPressureCallback callback = new TestPressureCallback();
        mMonitor.setReportingCallbackForTesting(callback);

        TestPressureSupplier pressureSupplier =
                new TestPressureSupplier(MemoryPressureLevel.MODERATE);
        mMonitor.setCurrentPressureSupplierForTesting(pressureSupplier);

        mMonitor.enablePolling(false);

        // When polling is enabled, current pressure should be retrieved and reported.
        pressureSupplier.assertCalled();
        callback.assertCalledWith(MemoryPressureLevel.MODERATE);
    }

    @Test
    @SmallTest
    public void testNullSupplierResultIgnored() {
        TestPressureCallback callback = new TestPressureCallback();
        mMonitor.setReportingCallbackForTesting(callback);

        TestPressureSupplier pressureSupplier = new TestPressureSupplier(null);
        mMonitor.setCurrentPressureSupplierForTesting(pressureSupplier);

        mMonitor.enablePolling(false);

        // The pressure supplier should be called, but its null result should be ignored.
        pressureSupplier.assertCalled();
        callback.assertNotCalled();
    }

    @Test
    @SmallTest
    public void testEnablePollingRespectsThrottling() {
        TestPressureSupplier pressureSupplier =
                new TestPressureSupplier(MemoryPressureLevel.MODERATE);
        mMonitor.setCurrentPressureSupplierForTesting(pressureSupplier);

        mMonitor.notifyPressure(MemoryPressureLevel.NONE);

        // The notification above started a throttling interval, so we shouldn't ask for the
        // current pressure when polling is enabled.
        mMonitor.enablePolling(false);

        pressureSupplier.assertNotCalled();
    }

    @Test
    @SmallTest
    public void testPollingIfCRITICAL() {
        TestPressureCallback callback = new TestPressureCallback();
        mMonitor.setReportingCallbackForTesting(callback);

        TestPressureSupplier pressureSupplier =
                new TestPressureSupplier(MemoryPressureLevel.MODERATE);
        mMonitor.setCurrentPressureSupplierForTesting(pressureSupplier);

        mMonitor.notifyPressure(MemoryPressureLevel.CRITICAL);
        callback.reset();

        mMonitor.enablePolling(false);

        runUiThreadFor(THROTTLING_INTERVAL_MS - 1);

        // Pressure should be polled after the interval ends, not before.
        pressureSupplier.assertNotCalled();

        runUiThreadFor(1);

        // We started and finished the throttling interval with CRITICAL pressure, so
        // we should poll the current pressure at the end of the interval.
        pressureSupplier.assertCalled();
        callback.assertCalledWith(MemoryPressureLevel.MODERATE);
    }

    @Test
    @SmallTest
    public void testNoPollingIfNotCRITICAL() {
        TestPressureSupplier pressureSupplier = new TestPressureSupplier(MemoryPressureLevel.NONE);
        mMonitor.setCurrentPressureSupplierForTesting(pressureSupplier);

        mMonitor.notifyPressure(MemoryPressureLevel.MODERATE);

        mMonitor.enablePolling(false);

        runUiThreadFor(THROTTLING_INTERVAL_MS);

        // We started and finished the throttling interval with non-CRITICAL pressure,
        // so no polling should take place.
        pressureSupplier.assertNotCalled();
    }

    @Test
    @SmallTest
    public void testNoPollingIfChangedToCRITICAL() {
        TestPressureSupplier pressureSupplier = new TestPressureSupplier(MemoryPressureLevel.NONE);
        mMonitor.setCurrentPressureSupplierForTesting(pressureSupplier);

        mMonitor.notifyPressure(MemoryPressureLevel.MODERATE);
        mMonitor.notifyPressure(MemoryPressureLevel.CRITICAL);

        mMonitor.enablePolling(false);

        runUiThreadFor(THROTTLING_INTERVAL_MS);

        // We finished the throttling interval with CRITITCAL, but started with MODERATE,
        // so no polling should take place.
        pressureSupplier.assertNotCalled();
    }

    @Test
    @SmallTest
    public void testDisablePolling() {
        TestPressureSupplier pressureSupplier = new TestPressureSupplier(MemoryPressureLevel.NONE);
        mMonitor.setCurrentPressureSupplierForTesting(pressureSupplier);

        mMonitor.notifyPressure(MemoryPressureLevel.CRITICAL);

        mMonitor.enablePolling(false);

        runUiThreadFor(THROTTLING_INTERVAL_MS - 1);

        // Whether polling is enabled or not should be taken into account only after the interval
        // finishes, so disabling it here should have the same affect as if it was never enabled.
        mMonitor.disablePolling();

        runUiThreadFor(1);

        pressureSupplier.assertNotCalled();
    }
}
