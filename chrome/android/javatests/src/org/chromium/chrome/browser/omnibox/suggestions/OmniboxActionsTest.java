// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionInSuggest;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ReusedCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils.SuggestionInfo;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.SuggestTemplateInfoProto.SuggestTemplateInfo.TemplateAction;
import org.chromium.components.omnibox.action.OmniboxAction;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests of the Omnibox Actions.
 *
 * <p>The suite intentionally disables Autocomplete subsystem to prevent real autocompletions from
 * overriding Test data.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class OmniboxActionsTest {
    @Rule
    public ReusedCtaTransitTestRule<WebPageStation> mActivityTestRule =
            ChromeTransitTestRules.blankPageStartReusedActivityRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private @Mock AutocompleteController.Natives mAutocompleteControllerJniMock;

    private WebPageStation mStartingPage;
    private OmniboxTestUtils mOmniboxUtils;

    @Before
    public void setUp() throws InterruptedException {
        mStartingPage = mActivityTestRule.start();
        mOmniboxUtils = new OmniboxTestUtils(mActivityTestRule.getActivity());
        AutocompleteControllerJni.setInstanceForTesting(mAutocompleteControllerJniMock);
    }

    @After
    public void tearDown() throws Exception {
        if (mOmniboxUtils.getFocus()) {
            mOmniboxUtils.clearFocus();
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    IncognitoTabHostUtils.closeAllIncognitoTabs();
                });
        AutocompleteControllerJni.setInstanceForTesting(null);
    }

    /**
     * Apply suggestions to the Omnibox. Requires at least one of the suggestions to include at
     * least one OmniboxAction. Verifies that suggestions - and actions - are shown.
     *
     * @param matches the matches to show
     */
    private void setSuggestions(AutocompleteMatch... matches) {
        mOmniboxUtils.requestFocus();
        // Ensure we start from empty suggestions list; don't carry over suggestions from previous
        // run.
        mOmniboxUtils.setSuggestions(AutocompleteResult.fromCache(null, null));

        mOmniboxUtils.setSuggestions(AutocompleteResult.fromCache(Arrays.asList(matches), null));
        mOmniboxUtils.checkSuggestionsShown();
        SuggestionInfo<BaseSuggestionView> info = mOmniboxUtils.findSuggestionWithActionChips();
        Assert.assertNotNull("No suggestions with actions", info);
    }

    /** Returns a fake AutocompleteMatch that features *all* of supplied actions. */
    private AutocompleteMatch createFakeSuggestion(@Nullable List<OmniboxAction> actions) {
        return AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                .setDisplayText("Suggestion")
                .setActions(actions)
                .build();
    }

    private AutocompleteMatch createFakeActionInSuggest(TemplateAction.ActionType... types) {
        var actions = new ArrayList<OmniboxAction>();
        for (var type : types) {
            actions.add(
                    new OmniboxActionInSuggest(
                            type.getNumber(),
                            "hint",
                            "accessibility",
                            type.getNumber(),
                            "https://www.google.com",
                            /* tabId= */ 0,
                            /* showAsActionButton= */ false));
        }

        return createFakeSuggestion(actions);
    }

    @Test
    @MediumTest
    public void testActionInSuggestShown() throws Exception {
        setSuggestions(
                createFakeSuggestion(null),
                createFakeActionInSuggest(TemplateAction.ActionType.CALL),
                createFakeActionInSuggest(TemplateAction.ActionType.DIRECTIONS));

        mOmniboxUtils.clearFocus();
    }

    @Test
    @MediumTest
    public void testActionInSuggestUsed_firstAction() throws Exception {
        // None of these actions have a linked intent, so no action will be taken.
        setSuggestions(
                createFakeSuggestion(null),
                createFakeActionInSuggest(TemplateAction.ActionType.CALL),
                createFakeActionInSuggest(TemplateAction.ActionType.DIRECTIONS));

        mOmniboxUtils.clickOnAction(1, 0);
    }

    @Test
    @MediumTest
    public void testActionInSuggestUsed_nthAction() throws Exception {
        // None of these actions have a linked intent, so no action will be taken.
        setSuggestions(
                createFakeSuggestion(null),
                createFakeActionInSuggest(
                        TemplateAction.ActionType.CALL,
                        TemplateAction.ActionType.DIRECTIONS,
                        TemplateAction.ActionType.REVIEWS));

        mOmniboxUtils.clickOnAction(1, 2);
    }
}
