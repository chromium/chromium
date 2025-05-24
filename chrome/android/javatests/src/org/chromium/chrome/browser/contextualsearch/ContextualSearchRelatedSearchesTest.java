// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.chips.ChipProperties;

import java.util.ArrayList;
import java.util.List;

/** Tests the Related Searches Feature of Contextual Search using instrumentation tests. */
@RunWith(ChromeJUnit4ClassRunner.class)
// NOTE: Disable online detection so we we'll default to online on test bots with no network.
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.CONTEXTUAL_SEARCH_DISABLE_ONLINE_DETECTION)
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
@Batch(Batch.PER_CLASS)
public class ContextualSearchRelatedSearchesTest extends ContextualSearchInstrumentationBase {
    @Override
    @Before
    public void setUp() throws Exception {
        mTestPage = "/chrome/test/data/android/contextualsearch/tap_test.html";
        super.setUp();
    }

    // --------------------------------------------------------------------------------------------
    // Related Searches Feature tests: base feature enables requests, UI feature allows results.
    // --------------------------------------------------------------------------------------------

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testRelatedSearchesInBar() throws Exception {
        ContextualSearchFakeServer.FakeResolveSearch fakeSearch =
                simulateResolveSearch("intelligence");
        ResolvedSearchTerm resolvedSearchTerm = fakeSearch.getResolvedSearchTerm();
        Assert.assertTrue(
                "Related Searches results should have been returned but were not!",
                !resolvedSearchTerm.relatedSearchesJson().isEmpty());
        // Select a chip in the Bar, which should expand the panel.
        final int chipToSelect = 1;
        ThreadUtils.runOnUiThreadBlocking(
                () -> mPanel.getRelatedSearchesInBarControl().selectChipForTest(chipToSelect));
        waitForPanelToExpand();

        // Close the panel
        closePanel();
        // TODO(donnd): Validate UMA metrics once we log in-bar selections.
    }

    /**
     * Tests that the offset of the SERP is unaffected by whether we are showing Related Searches in
     * the Bar or not. See https://crbug.com/1250546.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testRelatedSearchesInBarSerpOffset() throws Exception {
        simulateResolveSearch(SEARCH_NODE);
        float plainSearchBarHeight = mPanel.getBarHeight();
        float plainSearchContentY = mPanel.getContentY();
        closePanel();

        // Bring up a panel with Related Searches in order to expand the Bar
        simulateResolveSearch(RELATED_SEARCHES_NODE);
        // Wait for the animation to start growing the Bar.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mPanel.getInBarRelatedSearchesAnimatedHeightDps(),
                            Matchers.greaterThan(0f));
                });
        // We should have a taller Bar, but that should not affect the Y offset of the content.
        Assert.assertNotEquals(
                "Test code failure - unable to open panels with differing Bar heights!",
                plainSearchBarHeight,
                mPanel.getBarHeight(),
                0.1f);
        Assert.assertEquals(
                "SERP content offsets with and without Related Searches should match!",
                plainSearchContentY,
                mPanel.getContentY(),
                0.1f);
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testRelatedSearchesInBarWithDefaultQuery() throws Exception {
        ContextualSearchFakeServer.FakeResolveSearch fakeSearch =
                simulateResolveSearch("intelligence");
        ResolvedSearchTerm resolvedSearchTerm = fakeSearch.getResolvedSearchTerm();
        Assert.assertTrue(
                "Related Searches results should have been returned but were not!",
                !resolvedSearchTerm.relatedSearchesJson().isEmpty());
        // Select a chip in the Bar, which should expand the panel.
        final int chipToSelect = 0;
        ThreadUtils.runOnUiThreadBlocking(
                () -> mPanel.getRelatedSearchesInBarControl().selectChipForTest(chipToSelect));
        waitForPanelToExpand();

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mPanel.getSearchBarControl().getSearchTerm(),
                            Matchers.is("Intelligence"));
                });

        // Close the panel
        closePanel();
        // TODO(donnd): Validate UMA metrics once we log in-bar selections.
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testRelatedSearchesInBarWithDefaultQuery_HighlightDefaultQuery() throws Exception {
        ContextualSearchFakeServer.FakeResolveSearch fakeSearch =
                simulateResolveSearch("intelligence");
        ResolvedSearchTerm resolvedSearchTerm = fakeSearch.getResolvedSearchTerm();
        Assert.assertTrue(
                "Related Searches results should have been returned but were not!",
                !resolvedSearchTerm.relatedSearchesJson().isEmpty());
        // Select a chip in the Bar, which should expand the panel.
        expandPanelAndAssert();

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mPanel.getSearchBarControl().getSearchTerm(),
                            Matchers.is("Intelligence"));
                    Criteria.checkThat(
                            mPanel.getRelatedSearchesInBarControl().getSelectedChipForTest(),
                            Matchers.is(0));
                });

        // Close the panel
        closePanel();
        // TODO(donnd): Validate UMA metrics once we log in-bar selections.
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testRelatedSearchesInBarWithDefaultQuery_Ellipsize() throws Exception {
        ContextualSearchFakeServer.FakeResolveSearch fakeSearch =
                simulateResolveSearch("intelligence");
        ResolvedSearchTerm resolvedSearchTerm = fakeSearch.getResolvedSearchTerm();
        Assert.assertTrue(
                "Related Searches results should have been returned but were not!",
                !resolvedSearchTerm.relatedSearchesJson().isEmpty());
        // Select a chip in the Bar, which should expand the panel.
        expandPanelAndAssert();

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mPanel.getRelatedSearchesInBarControl()
                                    .getChipsForTest()
                                    .get(0)
                                    .model
                                    .get(ChipProperties.TEXT_MAX_WIDTH_PX),
                            Matchers.not(ChipProperties.SHOW_WHOLE_TEXT));
                });

        // Close the panel
        closePanel();
        // TODO(donnd): Validate UMA metrics once we log in-bar selections.
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testRelatedSearchesInBarForDefinitionCard() throws Exception {
        CompositorAnimationHandler.setTestingMode(true);
        // Do a normal search without Related Searches or Definition cards.
        simulateResolveSearch("search");
        float normalHeight = mPanel.getHeight();

        // Simulate a response that includes both a definition and Related Searches
        List<String> inBarSuggestions = new ArrayList<String>();
        inBarSuggestions.add("Related Suggestion 1");
        inBarSuggestions.add("Related Suggestion 2");
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mPanel.onSearchTermResolved(
                                "obscure · əbˈskyo͝or",
                                null,
                                null,
                                QuickActionCategory.NONE,
                                ResolvedSearchTerm.CardTag.CT_DEFINITION,
                                inBarSuggestions));
        boolean didPanelGetTaller = mPanel.getHeight() > normalHeight;
        Assert.assertTrue(
                "Related Searches should show in a taller Bar when there's a definition card, "
                        + "but they did not!",
                didPanelGetTaller);
        // Clean up
        closePanel();
        CompositorAnimationHandler.setTestingMode(false);
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @DisabledTest(message = "https://crbug.com/1255084")
    public void testRelatedSearchesDismissDuringAnimation() throws Exception {
        // Use the "intelligence" node to generate Related Searches suggestions.
        simulateResolveSearch("intelligence");

        // Wait for the animation to start growing the Bar.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mPanel.getInBarRelatedSearchesAnimatedHeightDps(),
                            Matchers.greaterThan(0f));
                });

        // Wait for the animation to change to make sure that doesn't bring the Bar back
        final boolean[] didAnimationChange = {false};
        mPanel.getSearchBarControl()
                .setInBarAnimationTestNotifier(
                        () -> {
                            didAnimationChange[0] = true;
                        });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(didAnimationChange[0], Matchers.is(true));
                });
        // Repeatedly closing the panel should not bring it back even during ongoing animation.
        closePanel();
        Assert.assertFalse("The panel is showing again due to Animation!", mPanel.isShowing());
        // Another scroll might try to close the panel when it thinks it's already closed, which
        // could fail due to inconsistencies in internal logic, so test that too.
        closePanel();
        Assert.assertFalse(
                "Expected the panel to not be showing after a close! "
                        + "Animation of the Bar height is the likely cause.",
                mPanel.isShowing());
    }
}
