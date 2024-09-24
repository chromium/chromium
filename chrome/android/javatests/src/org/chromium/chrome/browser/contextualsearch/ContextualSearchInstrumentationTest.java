// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests the Contextual Search Manager using instrumentation tests. */
// NOTE: Disable online detection so we we'll default to online on test bots with no network.
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.CONTEXTUAL_SEARCH_DISABLE_ONLINE_DETECTION)
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
@Batch(Batch.PER_CLASS)
public class ContextualSearchInstrumentationTest extends ContextualSearchInstrumentationBase {
    @Override
    @Before
    public void setUp() throws Exception {
        mTestPage = "/chrome/test/data/android/contextualsearch/simple_test.html";
        super.setUp();
    }

    // ============================================================================================
    // Test Cases
    // ============================================================================================

    /**
     * Tests a non-resolving gesture that peeks the panel followed by close panel. TODO(donnd):
     * Convert this test to test non-resolve action controlled through the privacy setting since we
     * are phasing out the non-resolve gesture.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testNonResolveGesture() throws Exception {
        simulateNonResolveSearch(SEARCH_NODE);
        assertPeekingPanelNonResolve();
        closePanel();
        assertClosedPanelNonResolve();
        assertPanelNeverOpened();
    }

    /** Tests a resolving gesture that peeks the panel followed by close panel. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testResolveGesture() throws Exception {
        simulateResolveSearch(SEARCH_NODE);
        assertPeekingPanelResolve();
        closePanel();
        assertClosedPanelResolve();
        assertPanelNeverOpened();
    }

    /** Tests a privacy neutral use case with a peek/expand/close panel sequence. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testPrivacyNeutralPeekExpandMaximize() throws Exception {
        mPolicy.overrideAllowSendingPageUrlForTesting(true);
        mPolicy.overrideDecidedStateForTesting(false);
        longPressNode(SEARCH_NODE);
        assertPeekingPanelNonResolve();
        fakeResponse(mFakeServer.buildResolvedSearchTermWithRelatedSearches(SEARCH_NODE_TERM));
        expandPanelAndAssert();
        mPanel.updatePanelToStateForTest(OverlayPanel.PanelState.EXPANDED);
        assertExpandedPanelNonResolve();
        maximizePanel();
        // TODO(donnd): consider asserting that no caption or other intelligent UI is showing.
        closePanel();
    }

    // --------------------------------------------------------------------------------------------
    // Forced Caption Feature tests.
    // --------------------------------------------------------------------------------------------

    /**
     * Tests that a caption is shown on a non intelligent search when the force-caption feature is
     * enabled.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testNonResolveCaption() throws Exception {
        // Simulate a non-resolve search and make sure a Caption is shown if appropriate.
        simulateNonResolveSearch(SEARCH_NODE);
        Assert.assertTrue(mPanel.getSearchBarControl().getCaptionVisible());
        closePanel();
    }
}
