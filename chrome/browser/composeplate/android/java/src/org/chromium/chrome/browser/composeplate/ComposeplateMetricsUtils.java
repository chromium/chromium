// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;

/** The utility class for logging the composeplate's metrics. */
@NullMarked
public class ComposeplateMetricsUtils {

    public static final String HISTOGRAM_COMPOSEPLATE_IMPRESSION =
            "NewTabPage.Composeplate.Impression";
    public static final String HISTOGRAM_NTP_OMNIBOX_IMPRESSION2 =
            "NewTabPage.FakeSearchBox.Impression2";
    public static final String HISTOGRAM_NTP_OMNIBOX_COMPOSEPLATE_BUTTON_IMPRESSION2 =
            "NewTabPage.FakeSearchBox.ComposeplateButton.Impression2";

    /**
     * Records a click on a section within the Composeplate row on the New Tab Page.
     *
     * @param sectionType The {@link ModuleTypeOnStartAndNtp} that was clicked.
     */
    public static void recordComposeplateClick(@ModuleTypeOnStartAndNtp int sectionType) {
        BrowserUiUtils.recordModuleClickHistogram(sectionType);
    }

    /** Records that the Composeplate row was displayed on the New Tab Page. */
    public static void recordComposeplateImpression(boolean visible) {
        RecordHistogram.recordBooleanHistogram(HISTOGRAM_COMPOSEPLATE_IMPRESSION, visible);
    }

    /**
     * Records a click on the composeplate button within the fake search box on the New Tab Page.
     */
    public static void recordFakeSearchBoxComposeplateButtonClick() {
        BrowserUiUtils.recordModuleClickHistogram(ModuleTypeOnStartAndNtp.COMPOSEPLATE_BUTTON);
    }

    /**
     * Records an impression of the fake search box on the New Tab Page. This logs a baseline
     * impression count for the button in the fake search box. The event is triggered each time the
     * New Tab Page is shown or the button's visibility is updated.
     */
    public static void recordFakeSearchBoxImpression2() {
        RecordHistogram.recordBooleanHistogram(HISTOGRAM_NTP_OMNIBOX_IMPRESSION2, true);
    }

    /** Records an impression of composeplate button on the fake search box in the New Tab Page. */
    public static void recordFakeSearchBoxComposeplateButtonImpression2(boolean isVisible) {
        RecordHistogram.recordBooleanHistogram(
                HISTOGRAM_NTP_OMNIBOX_COMPOSEPLATE_BUTTON_IMPRESSION2, isVisible);
    }
}
