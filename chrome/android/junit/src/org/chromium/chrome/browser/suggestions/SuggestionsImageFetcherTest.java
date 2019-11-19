// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.test.util.browser.suggestions.ContentSuggestionsTestUtils.createDummySuggestion;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.metrics.test.DisableHistogramsRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.chrome.browser.ntp.cards.CardsVariationParameters;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.widget.ThumbnailProvider;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;

import java.util.HashMap;

/**
 * Unit tests for {@link ImageFetcher}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SuggestionsImageFetcherTest {
    public static final int IMAGE_SIZE_PX = 100;
    public static final String URL_STRING = "http://www.test.com";
    @Rule
    public DisableHistogramsRule disableHistogramsRule = new DisableHistogramsRule();

    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    private DiscardableReferencePool mReferencePool = new DiscardableReferencePool();

    @Mock
    private ThumbnailProvider mThumbnailProvider;
    @Mock
    private SuggestionsSource mSuggestionsSource;
    @Mock
    private LargeIconBridge mLargeIconBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        CardsVariationParameters.setTestVariationParams(new HashMap<>());

        mSuggestionsDeps.getFactory().largeIconBridge = mLargeIconBridge;
        mSuggestionsDeps.getFactory().thumbnailProvider = mThumbnailProvider;
        mSuggestionsDeps.getFactory().suggestionsSource = mSuggestionsSource;
    }

    @SuppressWarnings("unchecked")
    @Test
    public void testFaviconFetch() {
        ImageFetcher imageFetcher =
                new ImageFetcher(mSuggestionsSource, mock(Profile.class), mReferencePool);

        Callback mockCallback = mock(Callback.class);
        @KnownCategories
        int[] categoriesWithIcon = new int[] {KnownCategories.ARTICLES};
        for (@KnownCategories int category : categoriesWithIcon) {
            SnippetArticle suggestion = createDummySuggestion(category);
            imageFetcher.makeFaviconRequest(suggestion, mockCallback);

            verify(mSuggestionsSource)
                    .fetchSuggestionFavicon(eq(suggestion),
                            eq(ImageFetcher.PUBLISHER_FAVICON_MINIMUM_SIZE_PX),
                            eq(ImageFetcher.PUBLISHER_FAVICON_DESIRED_SIZE_PX),
                            any(Callback.class));
        }

        @KnownCategories
        int[] categoriesThatDontFetch = new int[] {KnownCategories.READING_LIST};
        for (@KnownCategories int category : categoriesThatDontFetch) {
            SnippetArticle suggestion = createDummySuggestion(category);
            imageFetcher.makeFaviconRequest(suggestion, mockCallback);

            verify(mSuggestionsSource, never())
                    .fetchSuggestionFavicon(
                            eq(suggestion), anyInt(), anyInt(), any(Callback.class));
        }
    }

    @SuppressWarnings("unchecked")
    @Test
    public void testArticleThumbnailFetch() {
        ImageFetcher imageFetcher =
                new ImageFetcher(mSuggestionsSource, mock(Profile.class), mReferencePool);

        SnippetArticle suggestion = createDummySuggestion(KnownCategories.ARTICLES);
        imageFetcher.makeArticleThumbnailRequest(suggestion, mock(Callback.class));

        verify(mSuggestionsSource).fetchSuggestionImage(eq(suggestion), any(Callback.class));
    }

    @Test
    public void testLargeIconFetch() {
        ImageFetcher imageFetcher =
                new ImageFetcher(mSuggestionsSource, mock(Profile.class), mReferencePool);

        imageFetcher.makeLargeIconRequest(URL_STRING, IMAGE_SIZE_PX, mock(LargeIconCallback.class));

        verify(mLargeIconBridge)
                .getLargeIconForUrl(
                        eq(URL_STRING), eq(IMAGE_SIZE_PX), any(LargeIconCallback.class));
    }
}
