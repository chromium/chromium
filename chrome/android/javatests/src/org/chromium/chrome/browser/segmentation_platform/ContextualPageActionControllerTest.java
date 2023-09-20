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
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.net.test.EmbeddedTestServer;

@RunWith(BaseJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ContextualPageActionControllerTest {
    private static final String CONTEXTUAL_PAGE_ACTION_DEFAULT_MODEL_HISTOGRAM =
            "SegmentationPlatform.ModelExecution.DefaultProvider.Status."
            + "ContextualPageActionPriceTracking";
    private static final String CONTEXTUAL_PAGE_ACTION_SHOWN_BUTTON_HISTOGRAM =
            "Android.AdaptiveToolbarButton.Variant.OnPageLoad";
    private static final String TEST_PAGE = "/chrome/test/data/dom_distiller/simple_article.html";

    @Rule
    public Features.JUnitProcessor mFeaturesProcessor = new Features.JUnitProcessor();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mTestServer;
    private String mReaderModePageUrl;

    @Before
    public void setUp() throws Exception {
        mTestServer = mActivityTestRule.getTestServer();
        mReaderModePageUrl = mTestServer.getURL(TEST_PAGE);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.CONTEXTUAL_PAGE_ACTIONS,
            ChromeFeatureList.CONTEXTUAL_PAGE_ACTION_READER_MODE})
    public void
    testContextualPageModelExecution() {
        LibraryLoader.getInstance().ensureInitialized();

        // Expect the default model to be executed successfully.
        var histogram = HistogramWatcher.newSingleRecordWatcher(
                CONTEXTUAL_PAGE_ACTION_DEFAULT_MODEL_HISTOGRAM,
                /* value= kSuccess*/ 0);

        // Load a blank page, model should execute for every page load.
        mActivityTestRule.startMainActivityOnBlankPage();

        histogram.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.CONTEXTUAL_PAGE_ACTIONS,
            ChromeFeatureList.CONTEXTUAL_PAGE_ACTION_READER_MODE})
    public void
    testContextualPageModelExecution_OnReaderModePage() {
        LibraryLoader.getInstance().ensureInitialized();

        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(CONTEXTUAL_PAGE_ACTION_DEFAULT_MODEL_HISTOGRAM,
                                /* value= kSuccess*/ 0)
                        .expectIntRecord(CONTEXTUAL_PAGE_ACTION_SHOWN_BUTTON_HISTOGRAM, /* value= */
                                AdaptiveToolbarButtonVariant.READER_MODE)
                        .build();

        mActivityTestRule.startMainActivityWithURL(mReaderModePageUrl);

        histograms.pollInstrumentationThreadUntilSatisfied();
    }
}
