// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import static org.mockito.Mockito.doReturn;

import android.content.Context;
import android.content.res.Resources;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties.SuggestionIcon;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests for {@link BasicSuggestionProcessor}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BasicSuggestionProcessorTest {
    private BasicSuggestionProcessor mProcessor;
    private PropertyModel mModel;
    private static String[] sSuggestionTypes;
    private static String[] sIconTypes;

    /**
     * Initialize static test data.
     */
    static {
        sSuggestionTypes = new String[OmniboxSuggestionType.NUM_TYPES];
        sSuggestionTypes[OmniboxSuggestionType.URL_WHAT_YOU_TYPED] = "URL_WHAT_YOU_TYPED";
        sSuggestionTypes[OmniboxSuggestionType.HISTORY_URL] = "HISTORY_URL";
        sSuggestionTypes[OmniboxSuggestionType.HISTORY_TITLE] = "HISTORY_TITLE";
        sSuggestionTypes[OmniboxSuggestionType.HISTORY_BODY] = "HISTORY_BODY";
        sSuggestionTypes[OmniboxSuggestionType.HISTORY_KEYWORD] = "HISTORY_KEYWORD";
        sSuggestionTypes[OmniboxSuggestionType.NAVSUGGEST] = "NAVSUGGEST";
        sSuggestionTypes[OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED] = "SEARCH_WHAT_YOU_TYPED";
        sSuggestionTypes[OmniboxSuggestionType.SEARCH_HISTORY] = "SEARCH_HISTORY";
        sSuggestionTypes[OmniboxSuggestionType.SEARCH_SUGGEST] = "SEARCH_SUGGEST";
        sSuggestionTypes[OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY] = "SEARCH_SUGGEST_ENTITY";
        sSuggestionTypes[OmniboxSuggestionType.SEARCH_SUGGEST_TAIL] = "SEARCH_SUGGEST_TAIL";
        sSuggestionTypes[OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED] =
                "SEARCH_SUGGEST_PERSONALIZED";
        sSuggestionTypes[OmniboxSuggestionType.SEARCH_SUGGEST_PROFILE] = "SEARCH_SUGGEST_PROFILE";
        sSuggestionTypes[OmniboxSuggestionType.SEARCH_OTHER_ENGINE] = "SEARCH_OTHER_ENGINE";
        sSuggestionTypes[OmniboxSuggestionType.NAVSUGGEST_PERSONALIZED] = "NAVSUGGEST_PERSONALIZED";
        sSuggestionTypes[OmniboxSuggestionType.CLIPBOARD_URL] = "CLIPBOARD_URL";
        sSuggestionTypes[OmniboxSuggestionType.VOICE_SUGGEST] = "VOICE_SUGGEST";
        sSuggestionTypes[OmniboxSuggestionType.DOCUMENT_SUGGESTION] = "DOCUMENT_SUGGESTION";
        sSuggestionTypes[OmniboxSuggestionType.PEDAL] = "PEDAL";
        sSuggestionTypes[OmniboxSuggestionType.CLIPBOARD_TEXT] = "CLIPBOARD_TEXT";
        sSuggestionTypes[OmniboxSuggestionType.CLIPBOARD_IMAGE] = "CLIPBOARD_IMAGE";
        // Note: CALCULATOR suggestions are not handled by basic suggestion processor.
        // These suggestions are now processed by AnswerSuggestionProcessor instead.

        sIconTypes = new String[SuggestionIcon.TOTAL_COUNT];
        sIconTypes[SuggestionIcon.UNSET] = "UNSET";
        sIconTypes[SuggestionIcon.BOOKMARK] = "BOOKMARK";
        sIconTypes[SuggestionIcon.HISTORY] = "HISTORY";
        sIconTypes[SuggestionIcon.GLOBE] = "GLOBE";
        sIconTypes[SuggestionIcon.MAGNIFIER] = "MAGNIFIER";
        sIconTypes[SuggestionIcon.VOICE] = "VOICE";
        sIconTypes[SuggestionIcon.FAVICON] = "FAVICON";
    }

    @Mock
    Context mContext;
    @Mock
    Resources mResources;
    @Mock
    SuggestionHost mSuggestionHost;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mResources).when(mContext).getResources();
        mProcessor = new BasicSuggestionProcessor(mContext, mSuggestionHost, null);
    }

    /*
     * Set of helper functions to create different suggestion types.
     * While bodies are similar, these items are split on purpose to avoid repetitive and confusing
     * repetitions of boolean attributes.
     */
    private OmniboxSuggestion createSearchSuggestion(int nativeType) {
        return new OmniboxSuggestion(nativeType, true, 0, 0, null, null, null, null, null, null,
                null, null, null, false, false);
    }

    private OmniboxSuggestion createUrlSuggestion(int nativeType) {
        return new OmniboxSuggestion(nativeType, false, 0, 0, null, null, null, null, null, null,
                null, null, null, false, false);
    }

    private OmniboxSuggestion createBookmarkSuggestion(int nativeType) {
        return new OmniboxSuggestion(nativeType, false, 0, 0, null, null, null, null, null, null,
                null, null, null, true, false);
    }

    private void assertSuggestionIconTypeIs(
            OmniboxSuggestion suggestion, @SuggestionIcon int wantIcon) {
        int gotIcon = mProcessor.getSuggestionIconType(suggestion);
        Assert.assertEquals(
                String.format("%s: Want %s, Got %s", sSuggestionTypes[suggestion.getType()],
                        sIconTypes[wantIcon], sIconTypes[gotIcon]),
                wantIcon, gotIcon);
    }

    @Test
    public void getSuggestionIconTypeForSearch_Default() {
        int[][] testSuites = {
                {OmniboxSuggestionType.URL_WHAT_YOU_TYPED, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.HISTORY_URL, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.HISTORY_TITLE, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.HISTORY_BODY, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.HISTORY_KEYWORD, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.NAVSUGGEST, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.SEARCH_HISTORY, SuggestionIcon.HISTORY},
                {OmniboxSuggestionType.SEARCH_SUGGEST, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.SEARCH_SUGGEST_TAIL, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED, SuggestionIcon.HISTORY},
                {OmniboxSuggestionType.SEARCH_SUGGEST_PROFILE, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.SEARCH_OTHER_ENGINE, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.NAVSUGGEST_PERSONALIZED, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.CLIPBOARD_URL, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.VOICE_SUGGEST, SuggestionIcon.VOICE},
                {OmniboxSuggestionType.DOCUMENT_SUGGESTION, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.PEDAL, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.CLIPBOARD_TEXT, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.CLIPBOARD_IMAGE, SuggestionIcon.MAGNIFIER},
        };

        for (int[] test : testSuites) {
            assertSuggestionIconTypeIs(createSearchSuggestion(test[0]), test[1]);
        }
    }

    @Test
    public void getSuggestionIconTypeForUrl_Default() {
        int[][] testSuites = {
                {OmniboxSuggestionType.URL_WHAT_YOU_TYPED, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.HISTORY_URL, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.HISTORY_TITLE, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.HISTORY_BODY, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.HISTORY_KEYWORD, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.NAVSUGGEST, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.SEARCH_HISTORY, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.SEARCH_SUGGEST, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.SEARCH_SUGGEST_TAIL, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.SEARCH_SUGGEST_PROFILE, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.SEARCH_OTHER_ENGINE, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.NAVSUGGEST_PERSONALIZED, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.CLIPBOARD_URL, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.VOICE_SUGGEST, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.DOCUMENT_SUGGESTION, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.PEDAL, SuggestionIcon.GLOBE},
                {OmniboxSuggestionType.CLIPBOARD_TEXT, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.CLIPBOARD_IMAGE, SuggestionIcon.MAGNIFIER},
        };

        for (int[] test : testSuites) {
            assertSuggestionIconTypeIs(createUrlSuggestion(test[0]), test[1]);
        }
    }

    @Test
    public void getSuggestionIconTypeForBookmarks_Default() {
        int[][] testSuites = {
                {OmniboxSuggestionType.URL_WHAT_YOU_TYPED, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.HISTORY_URL, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.HISTORY_TITLE, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.HISTORY_BODY, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.HISTORY_KEYWORD, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.NAVSUGGEST, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.SEARCH_HISTORY, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.SEARCH_SUGGEST, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.SEARCH_SUGGEST_TAIL, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.SEARCH_SUGGEST_PROFILE, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.SEARCH_OTHER_ENGINE, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.NAVSUGGEST_PERSONALIZED, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.CLIPBOARD_URL, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.VOICE_SUGGEST, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.DOCUMENT_SUGGESTION, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.PEDAL, SuggestionIcon.BOOKMARK},
                {OmniboxSuggestionType.CLIPBOARD_TEXT, SuggestionIcon.MAGNIFIER},
                {OmniboxSuggestionType.CLIPBOARD_IMAGE, SuggestionIcon.MAGNIFIER},
        };

        for (int[] test : testSuites) {
            assertSuggestionIconTypeIs(createBookmarkSuggestion(test[0]), test[1]);
        }
    }
}
