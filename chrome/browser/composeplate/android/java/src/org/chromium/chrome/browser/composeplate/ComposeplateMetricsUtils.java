// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The utility class for logging the composeplate's metrics. */
@NullMarked
public class ComposeplateMetricsUtils {

    public static final String HISTOGRAM_COMPOSEPLATE_IMPRESSION =
            "NewTabPage.Composeplate.Impression";
    public static final String HISTOGRAM_NTP_OMNIBOX_IMPRESSION2 =
            "NewTabPage.FakeSearchBox.Impression2";
    public static final String HISTOGRAM_COMPOSEPLATE_BUTTONS_IMPRESSION =
            "NewTabPage.Composeplate.Buttons.Impression";

    /**
     * The buttons on the composeplate. These values are recorded to the
     * NewTabPage.Composeplate.Buttons.Impression histogram. Entries should not be renumbered and
     * numeric values should never be reused.
     */
    // LINT.IfChange(ComposeplateViewButtonType)
    @IntDef({
        ComposeplateViewButtonType.AI_MODE,
        ComposeplateViewButtonType.INCOGNITO,
        ComposeplateViewButtonType.CREATE_IMAGE,
        ComposeplateViewButtonType.ADD_IMAGE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ComposeplateViewButtonType {
        int AI_MODE = 0;
        int INCOGNITO = 1;
        int CREATE_IMAGE = 2;
        int ADD_IMAGE = 3;
        int NUM_ENTRIES = 4;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/new_tab_page/enums.xml:NtpComposeplateViewButtonType)

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
     * Records an impression of the fake search box on the New Tab Page. This logs a baseline
     * impression count for the button in the fake search box. The event is triggered each time the
     * New Tab Page is shown or the button's visibility is updated.
     */
    public static void recordFakeSearchBoxImpression2() {
        RecordHistogram.recordBooleanHistogram(HISTOGRAM_NTP_OMNIBOX_IMPRESSION2, true);
    }

    /**
     * Records an impression of a button on the composeplate in the New Tab Page.
     *
     * @param buttonType The {@link ComposeplateViewButtonType} of the button that was shown.
     */
    public static void recordComposeplateButtonImpression(
            @ComposeplateViewButtonType int buttonType) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_COMPOSEPLATE_BUTTONS_IMPRESSION,
                buttonType,
                ComposeplateViewButtonType.NUM_ENTRIES);
    }
}
