// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;

/** Unit tests for {@link BrowserControlsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BrowserControlsUtilsUnitTest {
    private static final String HISTOGRAM_PERCENTAGE_MAX_HEIGHT =
            "Android.BrowserControls.PercentageOfWindowUsedByBrowserControlsAtMaxHeight";
    private static final String HISTOGRAM_PERCENTAGE_MIN_HEIGHT =
            "Android.BrowserControls.PercentageOfWindowUsedByBrowserControlsAtMinHeight";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BrowserControlsStateProvider mStateProvider;
    @Mock private Context mContext;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Resources mResources;
    @Mock private DisplayAndroid mDisplayAndroid;

    private final Configuration mConfig = new Configuration();

    @Before
    public void setUp() {
        doReturn(mResources).when(mContext).getResources();
        doReturn(mConfig).when(mResources).getConfiguration();
        doReturn(mDisplayAndroid).when(mWindowAndroid).getDisplay();
        doReturn(1.0f).when(mDisplayAndroid).getDipScale();
        mConfig.screenHeightDp = 800;
    }

    @Test
    public void testGetWindowHeight() {
        assertEquals(800, BrowserControlsUtils.getWindowHeight(mContext, mWindowAndroid));

        doReturn(2.0f).when(mDisplayAndroid).getDipScale();
        assertEquals(1600, BrowserControlsUtils.getWindowHeight(mContext, mWindowAndroid));

        doReturn(2.5f).when(mDisplayAndroid).getDipScale();
        assertEquals(2000, BrowserControlsUtils.getWindowHeight(mContext, mWindowAndroid));

        mConfig.screenHeightDp = 1000;
        doReturn(2.0f).when(mDisplayAndroid).getDipScale();
        assertEquals(2000, BrowserControlsUtils.getWindowHeight(mContext, mWindowAndroid));
    }

    @Test
    public void testCalculatePercentageOfWindowUsed() {
        assertEquals(25, BrowserControlsUtils.calculatePercentageOfWindowUsed(50, 200));
        assertEquals(33, BrowserControlsUtils.calculatePercentageOfWindowUsed(1, 3));
        assertEquals(67, BrowserControlsUtils.calculatePercentageOfWindowUsed(2, 3));
        assertEquals(0, BrowserControlsUtils.calculatePercentageOfWindowUsed(0, 200));

        // 0 or negative window height returns 0.
        assertEquals(0, BrowserControlsUtils.calculatePercentageOfWindowUsed(50, 0));
        assertEquals(0, BrowserControlsUtils.calculatePercentageOfWindowUsed(50, -100));

        // Negative controls height returns 0.
        assertEquals(0, BrowserControlsUtils.calculatePercentageOfWindowUsed(-10, 200));

        // Clamped at 100.
        assertEquals(100, BrowserControlsUtils.calculatePercentageOfWindowUsed(250, 200));
    }

    @Test
    public void testRecordCombinedControlsMetrics() {
        doReturn(100).when(mStateProvider).getTopControlsHeight();
        doReturn(60).when(mStateProvider).getBottomControlsHeight();
        doReturn(0).when(mStateProvider).getTopControlsMinHeight();
        doReturn(20).when(mStateProvider).getBottomControlsMinHeight();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(HISTOGRAM_PERCENTAGE_MAX_HEIGHT, 20)
                        .expectIntRecord(HISTOGRAM_PERCENTAGE_MIN_HEIGHT, 3)
                        .build();

        BrowserControlsUtils.recordCombinedControlsMetrics(
                mStateProvider, mContext, mWindowAndroid);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordCombinedControlsMetrics_ZeroOrNegativeWindowHeight() {
        mConfig.screenHeightDp = 0;
        doReturn(100).when(mStateProvider).getTopControlsHeight();
        doReturn(50).when(mStateProvider).getBottomControlsHeight();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(HISTOGRAM_PERCENTAGE_MAX_HEIGHT)
                        .expectNoRecords(HISTOGRAM_PERCENTAGE_MIN_HEIGHT)
                        .build();

        BrowserControlsUtils.recordCombinedControlsMetrics(
                mStateProvider, mContext, mWindowAndroid);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordCombinedControlsMetrics_ClampedAt100Percent() {
        doReturn(500).when(mStateProvider).getTopControlsHeight();
        doReturn(500).when(mStateProvider).getBottomControlsHeight();
        doReturn(450).when(mStateProvider).getTopControlsMinHeight();
        doReturn(450).when(mStateProvider).getBottomControlsMinHeight();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(HISTOGRAM_PERCENTAGE_MAX_HEIGHT, 100)
                        .expectIntRecord(HISTOGRAM_PERCENTAGE_MIN_HEIGHT, 100)
                        .build();

        BrowserControlsUtils.recordCombinedControlsMetrics(
                mStateProvider, mContext, mWindowAndroid);
        histogramWatcher.assertExpected();
    }
}
