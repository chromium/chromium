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
