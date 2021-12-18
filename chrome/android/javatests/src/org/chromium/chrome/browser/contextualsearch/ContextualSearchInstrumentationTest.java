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
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;

import java.util.Arrays;

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
    /**
     * Parameter provider for enabling/disabling Features under development.
     */
    public static class FeatureParamProvider implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            return Arrays.asList(new ParameterSet().value(EnabledFeature.NONE).name("default"),
                    new ParameterSet().value(EnabledFeature.LONGPRESS).name("enableLongpress"),
                    new ParameterSet()
                            .value(EnabledFeature.TRANSLATIONS)
                            .name("enableTranslations"),
                    new ParameterSet()
                            .value(EnabledFeature.PRIVACY_NEUTRAL)
                            .name("enablePrivacyNeutralEngagement"),
                    new ParameterSet()
                            .value(EnabledFeature.PRIVACY_NEUTRAL_WITH_RELATED_SEARCHES)
                            .name("enablePrivacyNeutralWithRelatedSearches"),
                    new ParameterSet()
                            .value(EnabledFeature.CONTEXTUAL_TRIGGERS)
                            .name("enableContextualTriggers"));
        }
    }

    //    @ParameterAnnotations.UseMethodParameterBefore(BaseFeatureParamProvider.class)

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
     * TODO(donnd): Convert this test to test non-resolve action controlled through the privacy
     * setting since we are phasing out the non-resolve gesture.
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

    /**
     * Tests a privacy neutral use case with a peek/expand/close panel sequence.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testPrivacyNeutralPeekExpand(@EnabledFeature int enabledFeature) throws Exception {
        mPolicy.overrideDecidedStateForTesting(false);
        longPressNode(SEARCH_NODE);
        assertPeekingPanelNonResolve();
        tapPeekingBarToExpandAndAssert();
        if (enabledFeature == EnabledFeature.PRIVACY_NEUTRAL
                || enabledFeature == EnabledFeature.PRIVACY_NEUTRAL_WITH_RELATED_SEARCHES) {
            // PRIVACY_NEUTRAL feature includes Delayed Intelligence which resolves during the
            // expand.
            fakeResponse(false, 200, SEARCH_NODE_TERM, SEARCH_NODE_TERM, "alternate-term", false);
            assertExpandedPanelResolve(SEARCH_NODE_TERM);
        } else {
            assertExpandedPanelNonResolve();
        }
        closePanel();
    }
}
