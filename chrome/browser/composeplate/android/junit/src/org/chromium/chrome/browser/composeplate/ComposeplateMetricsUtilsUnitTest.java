// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.composeplate.ComposeplateMetricsUtils.ComposeplateViewButtonType;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;

/** Unit tests for {@link ComposeplateMetricsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
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
    public void testRecordFakeSearchBoxImpression() {
        String histogramName = "NewTabPage.FakeSearchBox.Impression2";
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(histogramName, true);
        ComposeplateMetricsUtils.recordFakeSearchBoxImpression2();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordComposeplateButtonImpression_aiModeButton() {
        testRecordComposeplateButtonImpressionImpl(
                ComposeplateViewButtonType.AI_MODE, /* expectedSample= */ 0);
    }

    @Test
    public void testRecordComposeplateButtonImpression_incognitoButton() {
        testRecordComposeplateButtonImpressionImpl(
                ComposeplateViewButtonType.INCOGNITO, /* expectedSample= */ 1);
    }

    @Test
    public void testRecordComposeplateButtonImpression_createImageButton() {
        testRecordComposeplateButtonImpressionImpl(
                ComposeplateViewButtonType.CREATE_IMAGE, /* expectedSample= */ 2);
    }

    @Test
    public void testRecordComposeplateButtonImpression_addImageButton() {
        testRecordComposeplateButtonImpressionImpl(
                ComposeplateViewButtonType.ADD_IMAGE, /* expectedSample= */ 3);
    }

    private void testRecordComposeplateButtonImpressionImpl(
            @ComposeplateViewButtonType int buttonType, int expectedSample) {
        // The samples are persisted to logs, so the values of ComposeplateViewButtonType must not
        // change.
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "NewTabPage.Composeplate.Buttons.Impression", expectedSample);
        ComposeplateMetricsUtils.recordComposeplateButtonImpression(buttonType);
        histogramWatcher.assertExpected();
    }
}
