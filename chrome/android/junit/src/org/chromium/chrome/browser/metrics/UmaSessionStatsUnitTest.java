// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.tab.Tab;

/** Unit tests for {@link UmaSessionStats}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UmaSessionStatsUnitTest {
    private static final String HISTOGRAM_PAGE_LOADED_IN_TABLET_MODE =
            "Android.Foldable.PageLoadedInTabletMode";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private UmaSessionStats.Natives mUmaSessionStatsJniMock;
    @Mock private Tab mTab;

    private UmaSessionStats mUmaSessionStats;

    @Before
    public void setUp() {
        UmaSessionStatsJni.setInstanceForTesting(mUmaSessionStatsJniMock);
        Activity activity = Robolectric.setupActivity(Activity.class);
        mUmaSessionStats = new UmaSessionStats(activity);

        when(mTab.getContext()).thenReturn(activity);
        when(mTab.getUserDataHost()).thenReturn(new UserDataHost());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testRecordPageLoadStats_foldable_tabletMode() {
        DeviceInfo.setIsFoldableForTesting(true);

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(HISTOGRAM_PAGE_LOADED_IN_TABLET_MODE, true);

        mUmaSessionStats.recordPageLoadStats(mTab);

        histogramWatcher.assertExpected();
    }

    @Test
    @Config(qualifiers = "sw400dp")
    public void testRecordPageLoadStats_foldable_phoneMode() {
        DeviceInfo.setIsFoldableForTesting(true);

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_PAGE_LOADED_IN_TABLET_MODE, false);

        mUmaSessionStats.recordPageLoadStats(mTab);

        histogramWatcher.assertExpected();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testRecordPageLoadStats_notFoldable() {
        DeviceInfo.setIsFoldableForTesting(false);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(HISTOGRAM_PAGE_LOADED_IN_TABLET_MODE)
                        .build();

        mUmaSessionStats.recordPageLoadStats(mTab);

        histogramWatcher.assertExpected();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testRecordPageLoadStats_foldable_noActivity() {
        DeviceInfo.setIsFoldableForTesting(true);
        when(mTab.getContext()).thenReturn(ContextUtils.getApplicationContext());

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(HISTOGRAM_PAGE_LOADED_IN_TABLET_MODE)
                        .build();

        mUmaSessionStats.recordPageLoadStats(mTab);

        histogramWatcher.assertExpected();
    }
}
