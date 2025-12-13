// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;

/** Unit tests for {@link ComposeplateMetricsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ComposeplateMetricsUtilsUnitTest {

    @Test
    public void testRecordComposeplateClick() {
        @ModuleTypeOnStartAndNtp
        int sectionType = ModuleTypeOnStartAndNtp.COMPOSEPLATE_VIEW_VOICE_SEARCH_BUTTON;
        String histogramName = "NewTabPage.Module.Click";
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, sectionType);
        ComposeplateMetricsUtils.recordComposeplateClick(sectionType);
        histogramWatcher.assertExpected();

        sectionType = ModuleTypeOnStartAndNtp.COMPOSEPLATE_VIEW_LENS_BUTTON;
        histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, sectionType);
        ComposeplateMetricsUtils.recordComposeplateClick(sectionType);
        histogramWatcher.assertExpected();

        sectionType = ModuleTypeOnStartAndNtp.COMPOSEPLATE_VIEW_INCOGNITO_BUTTON;
        histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, sectionType);
        ComposeplateMetricsUtils.recordComposeplateClick(sectionType);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordComposeplateImpression() {
        String histogramName = "NewTabPage.Composeplate.Impression";
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, true);
        ComposeplateMetricsUtils.recordComposeplateImpression(/* visible= */ true);
        histogramWatcher.assertExpected();

        histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, false);
        ComposeplateMetricsUtils.recordComposeplateImpression(/* visible= */ false);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordFakeSearchBoxComposeplateButtonClick() {
        String histogramName = "NewTabPage.Module.Click";
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        histogramName, ModuleTypeOnStartAndNtp.COMPOSEPLATE_BUTTON);
        ComposeplateMetricsUtils.recordFakeSearchBoxComposeplateButtonClick();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordFakeSearchBoxImpression() {
        String histogramName = "NewTabPage.FakeSearchBox.Impression2";
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, true);
        ComposeplateMetricsUtils.recordFakeSearchBoxImpression2();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordFakeSearchBoxComposeplateButtonImpression() {
        String histogramName = "NewTabPage.FakeSearchBox.ComposeplateButton.Impression2";
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, true);
        ComposeplateMetricsUtils.recordFakeSearchBoxComposeplateButtonImpression2(true);

        histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, false);
        ComposeplateMetricsUtils.recordFakeSearchBoxComposeplateButtonImpression2(false);
        histogramWatcher.assertExpected();
    }
}
