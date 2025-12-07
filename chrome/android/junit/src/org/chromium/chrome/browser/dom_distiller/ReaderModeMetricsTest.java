// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager.EntryPoint;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager.EntryPointTabType;
import org.chromium.dom_distiller.mojom.FontFamily;
import org.chromium.dom_distiller.mojom.Theme;

/** Tests for the {@link ReaderModeMetrics} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class ReaderModeMetricsTest {
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() {
        mUserActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        mUserActionTester.tearDown();
    }

    @Test
    @SmallTest
    public void testReportReaderModePrefsOpened() {
        ReaderModeMetrics.reportReaderModePrefsOpened();
        Assert.assertEquals(
                1,
                mUserActionTester.getActionCount("DomDistiller.Android.DistilledPagePrefsOpened"));
    }

    @Test
    @SmallTest
    public void testReportReaderModePrefsFontFamilyChanged() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "DomDistiller.Android.FontFamilySelected", FontFamily.SERIF)
                        .build();
        ReaderModeMetrics.reportReaderModePrefsFontFamilyChanged(FontFamily.SERIF);
        Assert.assertEquals(
                1, mUserActionTester.getActionCount("DomDistiller.Android.FontFamilyChanged"));
        histograms.assertExpected();
    }

    @Test
    @SmallTest
    public void testReportReaderModePrefsFontScalingChanged() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("DomDistiller.Android.FontScalingSelected", 150)
                        .build();
        ReaderModeMetrics.reportReaderModePrefsFontScalingChanged(1.5f);
        Assert.assertEquals(
                1, mUserActionTester.getActionCount("DomDistiller.Android.FontScalingChanged"));
        histograms.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordAnyPageSignalWithinTimeout() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "DomDistiller.Android.AnyPageSignalWithinTimeout", true)
                        .build();
        ReaderModeMetrics.recordAnyPageSignalWithinTimeout(true);
        histograms.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordDistillablePageSignalWithinTimeout() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "DomDistiller.Android.DistillablePageSignalWithinTimeout", true)
                        .build();
        ReaderModeMetrics.recordDistillablePageSignalWithinTimeout(true);
        histograms.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordTimeToProvideResultToAccumulator() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("DomDistiller.Time.TimeToProvideResultToAccumulator")
                        .build();
        ReaderModeMetrics.recordTimeToProvideResultToAccumulator(1L);
        histograms.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordReaderModeEntryPoint_Regular() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "DomDistiller.Android.EntryPoint.Regular", EntryPoint.APP_MENU)
                        .build();
        ReaderModeMetrics.recordReaderModeEntryPoint(
                EntryPoint.APP_MENU, EntryPointTabType.REGULAR_TAB);
        histograms.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordReaderModeEntryPoint_CCT() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("DomDistiller.Android.EntryPoint.CCT", EntryPoint.APP_MENU)
                        .build();
        ReaderModeMetrics.recordReaderModeEntryPoint(
                EntryPoint.APP_MENU, EntryPointTabType.CUSTOM_TAB);
        histograms.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordReaderModeEntryPoint_Incognito() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "DomDistiller.Android.EntryPoint.Incognito", EntryPoint.APP_MENU)
                        .build();
        ReaderModeMetrics.recordReaderModeEntryPoint(
                EntryPoint.APP_MENU, EntryPointTabType.INCOGNITO_TAB);
        histograms.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordReaderModeEntryPoint_IncognitoCCT() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "DomDistiller.Android.EntryPoint.IncognitoCCT", EntryPoint.APP_MENU)
                        .build();
        ReaderModeMetrics.recordReaderModeEntryPoint(
                EntryPoint.APP_MENU, EntryPointTabType.INCOGNITO_CUSTOM_TAB);
        histograms.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordOnStartedReaderMode() {
        ReaderModeMetrics.recordOnStartedReaderMode();
        Assert.assertEquals(
                1, mUserActionTester.getActionCount("DomDistiller.Android.OnStartedReaderMode"));
    }

    @Test
    @SmallTest
    public void testRecordOnStoppedReaderMode() {
        ReaderModeMetrics.recordOnStoppedReaderMode();
        Assert.assertEquals(
                1, mUserActionTester.getActionCount("DomDistiller.Android.OnStoppedReaderMode"));
    }

    @Test
    @SmallTest
    public void testRecordReaderModeViewDuration() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("DomDistiller.Time.ViewingReaderModePage")
                        .build();
        ReaderModeMetrics.recordReaderModeViewDuration(1L);
        histograms.assertExpected();
    }

    @Test
    @SmallTest
    public void testReportReaderModePrefsThemeChanged() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("DomDistiller.Android.ThemeSelected", Theme.DARK)
                        .build();
        ReaderModeMetrics.reportReaderModePrefsThemeChanged(Theme.DARK);
        Assert.assertEquals(
                1, mUserActionTester.getActionCount("DomDistiller.Android.ThemeChanged"));
        histograms.assertExpected();
    }
}
