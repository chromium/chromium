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

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/** Tests the Contextual Search Manager using instrumentation tests. */
// NOTE: Disable online detection so we we'll default to online on test bots with no network.
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.CONTEXTUAL_SEARCH_DISABLE_ONLINE_DETECTION)
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
@DoNotBatch(reason = "testTapWithLanguage is flaky if it is batched https://crbug.com/1105488")
public class ContextualSearchUnbatchedTest extends ContextualSearchInstrumentationBase {
    @Override
    @Before
    public void setUp() throws Exception {
        mTestPage = "/chrome/test/data/android/contextualsearch/tap_test.html";
        super.setUp();
    }

    // ============================================================================================
    // Test Cases. Many of these tests check histograms, which have issues running batched.
    // ============================================================================================

    /** Tests that a simple Tap with language determination triggers translation. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapWithLanguage() throws Exception {
        // Resolving a German word should trigger translation.
        mFakeServer.setExpectations(
                "german",
                new ResolvedSearchTerm.Builder(false, 200, "Deutsche", "Deutsche")
                        .setContextLanguage("de")
                        .build());
        simulateResolveSearch("german");

        // Make sure we tried to trigger translate.
        Assert.assertTrue(
                "Translation was not forced with the current request URL: "
                        + mManager.getRequest().getSearchUrl(),
                mManager.getRequest().isTranslationForced());
    }
}
