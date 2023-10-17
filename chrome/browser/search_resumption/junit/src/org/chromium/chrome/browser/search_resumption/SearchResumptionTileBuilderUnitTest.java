// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.search_resumption.SearchResumptionTileBuilder.OnSuggestionClickCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link SearchResumptionTileBuilder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SearchResumptionTileBuilderUnitTest {
    // The search suggestions are meant to be shown on any website.

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private Tab mTab;
    @Mock private SearchResumptionTileContainerView mSuggestionTilesContainerView;
    @Mock SearchResumptionTileView mTileView1;
    @Mock SearchResumptionTileView mTileView2;
    @Mock SearchResumptionTileView mTileView3;
    @Mock private AutocompleteMatch mSearchSuggest1;
    @Mock private AutocompleteMatch mSearchSuggest2;
    @Mock private AutocompleteMatch mSearchSuggest3;
    @Mock private AutocompleteMatch mSearchSuggest4;
    @Mock private AutocompleteMatch mNonSearchSuggest1;
    @Mock private AutocompleteResult mAutocompleteResult;

    private SearchResumptionTileBuilder mTileBuilder;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doReturn(OmniboxSuggestionType.SEARCH_SUGGEST).when(mSearchSuggest1).getType();
        doReturn("suggestion 1").when(mSearchSuggest1).getDisplayText();

        doReturn(OmniboxSuggestionType.SEARCH_SUGGEST).when(mSearchSuggest2).getType();
        doReturn("suggestion 2").when(mSearchSuggest2).getDisplayText();

        doReturn(OmniboxSuggestionType.SEARCH_SUGGEST).when(mSearchSuggest3).getType();
        doReturn("suggestion 3").when(mSearchSuggest3).getDisplayText();

        doReturn(OmniboxSuggestionType.SEARCH_SUGGEST).when(mSearchSuggest4).getType();
        doReturn("suggestion 4").when(mSearchSuggest4).getDisplayText();

        doReturn(OmniboxSuggestionType.TILE_NAVSUGGEST).when(mNonSearchSuggest1).getType();
        doReturn("non search suggestion 1").when(mNonSearchSuggest1).getDisplayText();

        createTileBuilder();
    }

    @Test
    @MediumTest
    public void testOnlyBuildTilesForSearchSuggestions() {
        List<AutocompleteMatch> suggestionList = Arrays.asList(mNonSearchSuggest1, mSearchSuggest1);
        when(mSuggestionTilesContainerView.buildTileView()).thenReturn(mTileView1);
        mTileBuilder.buildSuggestionTile(suggestionList, mSuggestionTilesContainerView);

        verify(mSuggestionTilesContainerView, times(1))
                .addView(any(SearchResumptionTileView.class));
    }

    @Test
    @MediumTest
    public void testBuildUpToMaxNumberOfTiles() {
        List<AutocompleteMatch> suggestionList =
                Arrays.asList(
                        mNonSearchSuggest1,
                        mSearchSuggest1,
                        mSearchSuggest2,
                        mSearchSuggest3,
                        mSearchSuggest4);
        doReturn(suggestionList).when(mAutocompleteResult).getSuggestionsList();
        when(mSuggestionTilesContainerView.buildTileView())
                .thenReturn(mTileView1, mTileView2, mTileView3);

        mTileBuilder.buildSuggestionTile(suggestionList, mSuggestionTilesContainerView);
        verify(mSuggestionTilesContainerView, times(3))
                .addView(any(SearchResumptionTileView.class));
    }

    private void createTileBuilder() {
        OnSuggestionClickCallback callback =
                (gUrl) -> {
                    mTab.loadUrl(new LoadUrlParams(gUrl));
                };
        mTileBuilder = new SearchResumptionTileBuilder(callback);
    }
}
