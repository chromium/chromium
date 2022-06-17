// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import static org.mockito.ArgumentMatchers.endsWith;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;
import android.view.ViewStub;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteControllerJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

/**
 * Unit tests for {@link SearchResumptionModuleMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SearchResumptionModuleMediatorUnitTest {
    // The search suggestions are meant to be shown on any website.
    private static final String URL_TO_TRACK = "/foo.com";

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private Tab mTabToTrack;
    @Mock
    private ViewStub mParent;
    @Mock
    private View mModuleLayoutView;
    @Mock
    private SearchResumptionContainerView mSuggestionTilesContainerView;
    @Mock
    private AutocompleteController mAutocompleteController;
    @Mock
    private AutocompleteController.Natives mControllerJniMock;
    @Mock
    SearchResumptionTileBuilder mTileBuilder;
    @Mock
    private Profile mProfile;
    @Mock
    private TemplateUrlService mTemplateUrlService;
    @Captor
    private ArgumentCaptor<OnSuggestionsReceivedListener> mListener;

    @Mock
    private AutocompleteMatch mSearchSuggest1;
    @Mock
    private AutocompleteMatch mSearchSuggest2;
    @Mock
    private AutocompleteMatch mNonSearchSuggest1;
    @Mock
    private AutocompleteResult mAutocompleteResult;

    private SearchResumptionModuleMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(AutocompleteControllerJni.TEST_HOOKS, mControllerJniMock);
        doReturn(mAutocompleteController).when(mControllerJniMock).getForProfile(any());

        GURL url = createMockGurl(URL_TO_TRACK);
        doReturn(url).when(mTabToTrack).getUrl();
        doReturn(mModuleLayoutView).when(mParent).inflate();
        doReturn(mSuggestionTilesContainerView).when(mModuleLayoutView).findViewById(anyInt());
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        when(mTemplateUrlService.isSearchResultsPageFromDefaultSearchProvider(any()))
                .thenReturn(false);

        doReturn(OmniboxSuggestionType.SEARCH_SUGGEST).when(mSearchSuggest1).getType();
        doReturn("suggestion 1").when(mSearchSuggest1).getDisplayText();
        doReturn(OmniboxSuggestionType.SEARCH_SUGGEST).when(mSearchSuggest2).getType();
        doReturn("suggestion 2").when(mSearchSuggest2).getDisplayText();
        doReturn(OmniboxSuggestionType.TILE_NAVSUGGEST).when(mNonSearchSuggest1).getType();
        doReturn("non search suggestion 1").when(mNonSearchSuggest1).getDisplayText();

        mMediator =
                new SearchResumptionModuleMediator(mParent, mTabToTrack, mProfile, mTileBuilder);
        verify(mAutocompleteController).addOnSuggestionsReceivedListener(mListener.capture());
        verify(mAutocompleteController, times(1))
                .startZeroSuggest(any(), endsWith(URL_TO_TRACK), anyInt(), any());
    }

    @Test
    @MediumTest
    public void testDoNotBuildModuleWithoutEnoughSuggestions() {
        List<AutocompleteMatch> list = Arrays.asList(mNonSearchSuggest1, mNonSearchSuggest1);
        doReturn(list).when(mAutocompleteResult).getSuggestionsList();

        mMediator.onSuggestionsReceived(mAutocompleteResult, "", true);
        verify(mParent, times(0)).inflate();
    }

    @Test
    @MediumTest
    public void testShowModuleWithEnoughResults() {
        List<AutocompleteMatch> list =
                Arrays.asList(mNonSearchSuggest1, mSearchSuggest1, mSearchSuggest2);
        doReturn(list).when(mAutocompleteResult).getSuggestionsList();

        mMediator.onSuggestionsReceived(mAutocompleteResult, "", true);
        verify(mParent, times(1)).inflate();
        Assert.assertEquals(View.VISIBLE, mSuggestionTilesContainerView.getVisibility());
    }

    private static GURL createMockGurl(String url) {
        GURL gurl = Mockito.mock(GURL.class);
        when(gurl.getSpec()).thenReturn(url);
        return gurl;
    }
}
