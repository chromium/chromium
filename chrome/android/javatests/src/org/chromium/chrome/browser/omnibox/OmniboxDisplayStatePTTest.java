// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import androidx.test.filters.MediumTest;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.transit.TransitAsserts;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.omnibox.FakeOmniboxSuggestions;
import org.chromium.chrome.test.transit.omnibox.OmniboxDraftingFacility;
import org.chromium.chrome.test.transit.omnibox.OmniboxFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;

/** Public transit integration tests for Omnibox visual display state transitions. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class OmniboxDisplayStatePTTest {
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private static final FakeOmniboxSuggestions sFakeSuggestions = new FakeOmniboxSuggestions();

    private WebPageStation mBlankPage;

    @BeforeClass
    public static void setUpClass() {
        sFakeSuggestions.initMocks();
    }

    @AfterClass
    public static void tearDownClass() {
        sFakeSuggestions.destroy();
    }

    @Before
    public void setUp() {
        mBlankPage = mCtaTestRule.startOnBlankPage();
    }

    @Test
    @MediumTest
    public void testVisualTransitions_drafting() {
        OmniboxFacility omnibox = mBlankPage.openOmnibox(sFakeSuggestions);

        OmniboxDraftingFacility drafting = omnibox.typeText("test");
        drafting.pressBackToExit();

        TransitAsserts.assertFinalDestination(mBlankPage);
    }
}
