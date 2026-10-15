// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ntp.NewTabPageUtils.ActionChips;
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
        testRecordComposeplateButtonImpressionImpl(ActionChips.DEFAULT);
    }

    @Test
    public void testRecordComposeplateButtonImpression_incognitoButton() {
        testRecordComposeplateButtonImpressionImpl(ActionChips.INCOGNITO);
    }

    @Test
    public void testRecordComposeplateButtonImpression_createImageButton() {
        testRecordComposeplateButtonImpressionImpl(ActionChips.CREATE_IMAGE);
    }

    @Test
    public void testRecordComposeplateButtonImpression_galleryButton() {
        testRecordComposeplateButtonImpressionImpl(ActionChips.GALLERY);
    }

    private void testRecordComposeplateButtonImpressionImpl(@ActionChips int buttonType) {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "NewTabPage.Composeplate.Buttons.Impression", buttonType);
        ComposeplateMetricsUtils.recordComposeplateButtonImpression(buttonType);
        histogramWatcher.assertExpected();
    }
}
