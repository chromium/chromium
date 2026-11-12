// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Build.VERSION_CODES;

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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.browser.ui.native_page.NativePage;

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
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
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

    @Test
    @Config(sdk = VERSION_CODES.R)
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TOP_INSET + ":enable_top_edge_to_edge/true")
    public void testRecordPageLoadStats_topToEdge_optedInWebPage() {
        EdgeToEdgeUtils.setAlwaysDrawWebEdgeToEdgeForTesting(true);
        when(mTab.isNativePage()).thenReturn(false);

        mUmaSessionStats.recordPageLoadStats(mTab);

        verify(mUmaSessionStatsJniMock).recordPageLoadedWithTopToEdge();
    }

    @Test
    @Config(sdk = VERSION_CODES.R)
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TOP_INSET + ":enable_top_edge_to_edge/true")
    public void testRecordPageLoadStats_topToEdge_notOptedInWebPage() {
        EdgeToEdgeUtils.setAlwaysDrawWebEdgeToEdgeForTesting(false);
        when(mTab.isNativePage()).thenReturn(false);

        mUmaSessionStats.recordPageLoadStats(mTab);

        verify(mUmaSessionStatsJniMock, never()).recordPageLoadedWithTopToEdge();
    }

    @Test
    @Config(sdk = VERSION_CODES.R)
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TOP_INSET + ":enable_top_edge_to_edge/false")
    public void testRecordPageLoadStats_topToEdge_refactorOnlyNtp_notRecorded() {
        NativePage nativePage = mock(NativePage.class);
        when(nativePage.supportsEdgeToEdgeOnTop()).thenReturn(true);
        when(mTab.isNativePage()).thenReturn(true);
        when(mTab.getNativePage()).thenReturn(nativePage);

        mUmaSessionStats.recordPageLoadStats(mTab);

        verify(mUmaSessionStatsJniMock, never()).recordPageLoadedWithTopToEdge();
    }

    @Test
    @Config(sdk = VERSION_CODES.R)
    @DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TOP_INSET)
    public void testRecordPageLoadStats_topToEdge_flagDisabled_notRecorded() {
        EdgeToEdgeUtils.setAlwaysDrawWebEdgeToEdgeForTesting(true);
        when(mTab.isNativePage()).thenReturn(false);

        mUmaSessionStats.recordPageLoadStats(mTab);

        verify(mUmaSessionStatsJniMock, never()).recordPageLoadedWithTopToEdge();
    }

    @Test
    @Config(sdk = VERSION_CODES.R, qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TOP_INSET + ":enable_top_edge_to_edge/true")
    public void testRecordPageLoadStats_topToEdge_tablet_notRecorded() {
        EdgeToEdgeUtils.setAlwaysDrawWebEdgeToEdgeForTesting(true);
        when(mTab.isNativePage()).thenReturn(false);

        mUmaSessionStats.recordPageLoadStats(mTab);

        verify(mUmaSessionStatsJniMock, never()).recordPageLoadedWithTopToEdge();
    }

    @Test
    @Config(sdk = VERSION_CODES.R)
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TOP_INSET + ":enable_top_edge_to_edge/true")
    public void testRecordPageLoadStats_topToEdge_customTab_notRecorded() {
        EdgeToEdgeUtils.setAlwaysDrawWebEdgeToEdgeForTesting(true);
        when(mTab.isNativePage()).thenReturn(false);
        when(mTab.isCustomTab()).thenReturn(true);

        mUmaSessionStats.recordPageLoadStats(mTab);

        verify(mUmaSessionStatsJniMock, never()).recordPageLoadedWithTopToEdge();
    }
}
