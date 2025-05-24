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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.net.test.EmbeddedTestServer;
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
    public void testContextualPageModelExecution() {
        LibraryLoader.getInstance().ensureInitialized();

        // Expect the default model to be executed successfully.
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        CONTEXTUAL_PAGE_ACTION_DEFAULT_MODEL_HISTOGRAM, /* value= kSuccess*/ 0);

        // Load a blank page, model should execute for every page load.
        mActivityTestRule.startMainActivityOnBlankPage();

        histogram.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE) // Reader mode is only available on phones.
    public void testContextualPageModelExecution_OnReaderModePage() {
        LibraryLoader.getInstance().ensureInitialized();
        mActivityTestRule.startMainActivityFromLauncher();

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

        mActivityTestRule.loadUrl(mReaderModePageUrl);

        histograms.pollInstrumentationThreadUntilSatisfied();
    }
}
