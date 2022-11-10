// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.action.OmniboxPedalType;
import org.chromium.chrome.browser.omnibox.suggestions.pedal.PedalSuggestionView;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils.SuggestionInfo;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.action.OmniboxPedal;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests of the Omnibox Pedals feature.
 */
@RunWith(ParameterizedRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@EnableFeatures(ChromeFeatureList.ENABLE_IPH)
public class OmniboxPedalsRenderTest {
    // TODO(crbug.com/1312078): Enable the tests for incognito mode.
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(new ParameterSet().value(false, false).name("LiteMode_RegularTab"),
                    new ParameterSet().value(true, false).name("NightMode_RegularTab"));

    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_OMNIBOX)
                    .build();

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private OmniboxTestUtils mOmniboxUtils;
    private boolean mIncognito;

    public OmniboxPedalsRenderTest(boolean nightMode, boolean incognito) {
        mIncognito = incognito;
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightMode);
        mRenderTestRule.setNightModeEnabled(nightMode);
        mRenderTestRule.setVariantPrefix(incognito ? "IncognitoTab" : "RegularTab");
    }

    @BeforeClass
    public static void beforeClass() {
        sActivityTestRule.startMainActivityOnBlankPage();
        sActivityTestRule.waitForActivityNativeInitializationComplete();
        sActivityTestRule.waitForDeferredStartup();
    }

    @Before
    public void setUp() throws InterruptedException {
        if (mIncognito) {
            sActivityTestRule.newIncognitoTabFromMenu();
        }
        sActivityTestRule.loadUrl("about:blank");
        mOmniboxUtils = new OmniboxTestUtils(sActivityTestRule.getActivity());
        mOmniboxUtils.requestFocus();
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { IncognitoTabHostUtils.closeAllIncognitoTabs(); });
    }

    @AfterClass
    public static void afterClass() {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    /**
     * Create a dummy pedal suggestion.
     * @param name The dummy suggestion name.
     * @param id The Omnibox pedal type to be created.
     *
     * @return a dummy pedal suggestion.
     */
    private AutocompleteMatch createDummyPedalSuggestion(String name, @OmniboxPedalType int id) {
        return AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                .setDisplayText(name)
                .setOmniboxPedal(new OmniboxPedal(id, "hints", "suggestionContents",
                        "accessibilitySuffix", "accessibilityHint", GURL.emptyGURL()))
                .build();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisabledTest(message = "crbug.com/1348691")
    public void testRunChromeSafetyCheckPedal() throws IOException, InterruptedException {
        List<AutocompleteMatch> suggestionsList = new ArrayList<>();
        suggestionsList.add(
                createDummyPedalSuggestion("pedal", OmniboxPedalType.RUN_CHROME_SAFETY_CHECK));
        mOmniboxUtils.setSuggestions(
                AutocompleteResult.fromCache(suggestionsList, null), "Run safety check");
        mOmniboxUtils.checkSuggestionsShown();

        SuggestionInfo<PedalSuggestionView> info =
                mOmniboxUtils.getSuggestionByType(OmniboxSuggestionUiType.PEDAL_SUGGESTION);
        mRenderTestRule.render(info.view, "ClearBrowsingDataPedal");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisabledTest(message = "crbug.com/1313949")
    public void testPlayChromeDinoGamePedal() throws IOException, InterruptedException {
        List<AutocompleteMatch> suggestionsList = new ArrayList<>();
        suggestionsList.add(
                createDummyPedalSuggestion("pedal", OmniboxPedalType.PLAY_CHROME_DINO_GAME));
        mOmniboxUtils.setSuggestions(
                AutocompleteResult.fromCache(suggestionsList, null), "Dino game");
        mOmniboxUtils.checkSuggestionsShown();

        SuggestionInfo<PedalSuggestionView> info =
                mOmniboxUtils.getSuggestionByType(OmniboxSuggestionUiType.PEDAL_SUGGESTION);
        mRenderTestRule.render(info.view, "DinoGamePedal");
    }
}
