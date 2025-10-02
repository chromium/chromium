// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.base.DeviceFormFactor;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.CONTEXTUAL_PAGE_ACTIONS})
@Batch(Batch.PER_CLASS)
public class ContextualPageActionControllerTest {
    private static final String CONTEXTUAL_PAGE_ACTION_DEFAULT_MODEL_HISTOGRAM =
            "SegmentationPlatform.ModelExecution.DefaultProvider.Status."
                    + "ContextualPageActionPriceTracking";
    private static final String CONTEXTUAL_PAGE_ACTION_SHOWN_BUTTON_HISTOGRAM =
            "Android.AdaptiveToolbarButton.Variant.OnPageLoad";
    private static final String TEST_PAGE = "/chrome/test/data/dom_distiller/simple_article.html";

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private String mReaderModePageUrl;

    @Before
    public void setUp() throws Exception {
        mReaderModePageUrl = mActivityTestRule.getTestServer().getURL(TEST_PAGE);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Flaky on larger form factors crbug.com/422817837
    public void testContextualPageModelExecution() {
        LibraryLoader.getInstance().ensureInitialized();

        // Expect the default model to be executed successfully.
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        CONTEXTUAL_PAGE_ACTION_DEFAULT_MODEL_HISTOGRAM, /* value= kSuccess*/ 0);

        // Load a blank page, model should execute for every page load.
        mActivityTestRule.startOnBlankPage();

        histogram.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Reader mode is only available on phones.
    @DisabledTest(message = "crbug.com/448846307")
    public void testContextualPageModelExecution_OnReaderModePage() {
        LibraryLoader.getInstance().ensureInitialized();
        WebPageStation page = mActivityTestRule.startOnBlankPage();

        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                CONTEXTUAL_PAGE_ACTION_DEFAULT_MODEL_HISTOGRAM,
                                /* value= kSuccess*/ 0)
                        .expectIntRecord(
                                CONTEXTUAL_PAGE_ACTION_SHOWN_BUTTON_HISTOGRAM,
                                /* value= */ AdaptiveToolbarButtonVariant.READER_MODE)
                        .allowExtraRecordsForHistogramsAbove()
                        .build();

        page = page.loadWebPageProgrammatically(mReaderModePageUrl);

        histograms.pollInstrumentationThreadUntilSatisfied();
    }
}
