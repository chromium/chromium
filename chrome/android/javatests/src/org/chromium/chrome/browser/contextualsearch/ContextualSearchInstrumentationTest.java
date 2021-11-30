// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;

/**
 * Tests the Contextual Search Manager using instrumentation tests.
 */
// NOTE: Disable online detection so we we'll default to online on test bots with no network.
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContextualSearchFieldTrial.ONLINE_DETECTION_DISABLED,
        "disable-features=" + ChromeFeatureList.CONTEXTUAL_SEARCH_ML_TAP_SUPPRESSION + ","
                + ChromeFeatureList.CONTEXTUAL_SEARCH_THIN_WEB_VIEW_IMPLEMENTATION})
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
@Batch(Batch.PER_CLASS)
public class ContextualSearchInstrumentationTest extends ContextualSearchInstrumentationBase {
    @Override
    @Before
    public void setUp() throws Exception {
        mTestPage = "/chrome/test/data/android/contextualsearch/simple_test.html";
        super.setUp();
    }

    //============================================================================================
    // Test Cases
    //============================================================================================

    /**
     * Tests a non-resolving gesture that peeks the panel followed by close panel.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testNonResolveGesture(@EnabledFeature int enabledFeature) throws Exception {
        simulateNonResolveSearch(SEARCH_NODE);
        assertPeekingPanelNonResolve();
        closePanel();
        assertClosedPanelNonResolve();
        assertPanelNeverOpened();
    }

    /**
     * Tests a resolving gesture that peeks the panel followed by close panel.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testResolveGesture(@EnabledFeature int enabledFeature) throws Exception {
        simulateResolveSearch(SEARCH_NODE);
        assertPeekingPanelResolve();
        closePanel();
        assertClosedPanelResolve();
        assertPanelNeverOpened();
    }
}
