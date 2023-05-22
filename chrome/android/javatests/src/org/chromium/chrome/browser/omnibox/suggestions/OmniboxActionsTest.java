// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.history.HistoryActivity;
import org.chromium.chrome.browser.omnibox.suggestions.action.HistoryClustersAction;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionInSuggest;
import org.chromium.chrome.browser.omnibox.suggestions.base.ActionChipsAdapter;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils.SuggestionInfo;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.EntityInfoProto.ActionInfo;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.action.ActionInSuggestUmaType;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests of the Omnibox Actions.
 *
 * The suite intentionally disables Autocomplete subsystem to prevent real autocompletions from
 * overriding Test data.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class OmniboxActionsTest {
    public static @ClassRule ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();
    public static @ClassRule DisableAnimationsTestRule sDisableAnimationsRule =
            new DisableAnimationsTestRule();
    public @Rule JniMocker mJniMocker = new JniMocker();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    public @Rule TestRule mFeaturesProcessor = new Features.JUnitProcessor();
    private @Mock AutocompleteController.Natives mAutocompleteControllerJniMock;

    private OmniboxTestUtils mOmniboxUtils;
    private Activity mTargetActivity;

    @BeforeClass
    public static void beforeClass() {
        sActivityTestRule.startMainActivityOnBlankPage();
        sActivityTestRule.waitForActivityNativeInitializationComplete();
        sActivityTestRule.waitForDeferredStartup();
    }

    @Before
    public void setUp() throws InterruptedException {
        sActivityTestRule.loadUrl("about:blank");
        mOmniboxUtils = new OmniboxTestUtils(sActivityTestRule.getActivity());
        mJniMocker.mock(AutocompleteControllerJni.TEST_HOOKS, mAutocompleteControllerJniMock);
    }

    @After
    public void tearDown() throws Exception {
        if (mOmniboxUtils.getFocus()) {
            mOmniboxUtils.clearFocus();
        }
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { IncognitoTabHostUtils.closeAllIncognitoTabs(); });
        if (mTargetActivity != null) {
            ApplicationTestUtils.finishActivity(mTargetActivity);
        }
        mJniMocker.mock(AutocompleteControllerJni.TEST_HOOKS, null);
    }

    /**
     * Click the n-th action.
     *
     * @param actionIndex the index of action to invoke.
     */
    private void clickOnAction(int actionIndex) {
        SuggestionInfo<BaseSuggestionView> info = mOmniboxUtils.findSuggestionWithActionChips();
        Assert.assertNotNull(info);

        CriteriaHelper.pollUiThread(() -> {
            var adapter = (ActionChipsAdapter) info.view.getActionChipsView().getAdapter();
            if (adapter.getItemCount() < actionIndex) return false;
            adapter.setSelectedItem(ActionChipsAdapter.FIRST_CHIP_INDEX + actionIndex);
            return adapter.getSelectedView().performClick();
        });
    }

    /**
     * Apply suggestions to the Omnibox.
     * Requires at least one of the suggestions to include at least one OmniboxAction.
     * Verifies that suggestions - and actions - are shown.
     *
     * @param matches the matches to show
     */
    private void setSuggestions(AutocompleteMatch... matches) {
        mOmniboxUtils.requestFocus();
        mOmniboxUtils.setSuggestions(
                AutocompleteResult.fromCache(Arrays.asList(matches), null), "");
        mOmniboxUtils.checkSuggestionsShown();
        SuggestionInfo<BaseSuggestionView> info = mOmniboxUtils.findSuggestionWithActionChips();
        Assert.assertNotNull("No suggestions with actions", info);
    }

    /** Returns a dummy AutocompleteMatch that features *all* of supplied actions.  */
    private AutocompleteMatch createDummySuggestion(@Nullable List<OmniboxAction> actions) {
        return AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                .setDisplayText("Suggestion")
                .setActions(actions)
                .build();
    }

    private AutocompleteMatch createDummyHistoryClustersAction(String name) {
        return createDummySuggestion(List.of(new HistoryClustersAction("hint", name)));
    }

    private AutocompleteMatch createDummyActionInSuggest(ActionInfo.ActionType... types) {
        var actions = new ArrayList<OmniboxAction>();
        for (var type : types) {
            actions.add(
                    new OmniboxActionInSuggest("hint", type.getNumber(), "https://www.google.com"));
        }

        return createDummySuggestion(actions);
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.OMNIBOX_HISTORY_CLUSTER_PROVIDER})
    @EnableFeatures({ChromeFeatureList.HISTORY_JOURNEYS,
            ChromeFeatureList.OMNIBOX_HISTORY_CLUSTER_ACTION_CHIP})
    public void
    testHistoryClustersAction() throws Exception {
        setSuggestions(createDummyHistoryClustersAction("query"));
        clickOnAction(0);

        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(sActivityTestRule.getActivity())) {
            CriteriaHelper.pollUiThread(() -> {
                Tab tab = sActivityTestRule.getActivity().getActivityTab();
                Criteria.checkThat(tab, Matchers.notNullValue());
                Criteria.checkThat(
                        tab.getUrl().getSpec(), Matchers.startsWith("chrome://history/journeys"));
            });
        } else {
            mTargetActivity = ActivityTestUtils.waitForActivity(
                    InstrumentationRegistry.getInstrumentation(), HistoryActivity.class);
            Assert.assertNotNull("Could not find the history activity", mTargetActivity);
        }
    }

    @Test
    @MediumTest
    public void testActionInSuggestShown() throws Exception {
        setSuggestions(createDummySuggestion(null),
                createDummyActionInSuggest(ActionInfo.ActionType.CALL),
                createDummyActionInSuggest(ActionInfo.ActionType.DIRECTIONS));

        var histogramWatcher = HistogramWatcher.newBuilder()
                                       .expectIntRecord("Omnibox.ActionInSuggest.Shown",
                                               ActionInSuggestUmaType.CALL)
                                       .expectIntRecord("Omnibox.ActionInSuggest.Shown",
                                               ActionInSuggestUmaType.DIRECTIONS)
                                       .build();
        mOmniboxUtils.clearFocus();
        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testActionInSuggestUsed_firstAction() throws Exception {
        // None of these actions have a linked intent, so no action will be taken.
        setSuggestions(createDummySuggestion(null),
                createDummyActionInSuggest(ActionInfo.ActionType.CALL),
                createDummyActionInSuggest(ActionInfo.ActionType.DIRECTIONS));

        var histogramWatcher = HistogramWatcher.newBuilder()
                                       .expectIntRecord("Omnibox.ActionInSuggest.Shown",
                                               ActionInSuggestUmaType.CALL)
                                       .expectIntRecord("Omnibox.ActionInSuggest.Shown",
                                               ActionInSuggestUmaType.DIRECTIONS)
                                       .expectIntRecord("Omnibox.ActionInSuggest.Used",
                                               ActionInSuggestUmaType.CALL)
                                       .build();
        clickOnAction(0);
        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testActionInSuggestUsed_nthAction() throws Exception {
        // None of these actions have a linked intent, so no action will be taken.
        setSuggestions(createDummySuggestion(null),
                createDummyActionInSuggest(ActionInfo.ActionType.CALL,
                        ActionInfo.ActionType.DIRECTIONS, ActionInfo.ActionType.REVIEWS));

        var histogramWatcher = HistogramWatcher.newBuilder()
                                       .expectIntRecord("Omnibox.ActionInSuggest.Shown",
                                               ActionInSuggestUmaType.CALL)
                                       .expectIntRecord("Omnibox.ActionInSuggest.Shown",
                                               ActionInSuggestUmaType.DIRECTIONS)
                                       .expectIntRecord("Omnibox.ActionInSuggest.Used",
                                               ActionInSuggestUmaType.REVIEWS)
                                       .expectIntRecord("Omnibox.ActionInSuggest.Shown",
                                               ActionInSuggestUmaType.REVIEWS)
                                       .build();
        clickOnAction(2);
        histogramWatcher.assertExpected();
    }
}
