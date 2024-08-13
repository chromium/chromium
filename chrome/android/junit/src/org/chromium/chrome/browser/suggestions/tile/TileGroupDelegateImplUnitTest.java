// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSites;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link TileGroupDelegateImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TileGroupDelegateImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @Mock private SuggestionsDependencyFactory mSuggestionsDependencyFactory;
    @Mock private Profile mProfile;
    @Mock private MostVisitedSites mMostVisitedSites;
    @Mock private SuggestionsNavigationDelegate mNavigationDelegate;
    @Mock private SnackbarManager mSnackbarManager;

    private Context mContext;
    private TileGroupDelegateImpl mTileGroupDelegateImpl;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);

        lenient()
                .when(mSuggestionsDependencyFactory.createMostVisitedSites(any(Profile.class)))
                .thenReturn(mMostVisitedSites);
        mSuggestionsDeps.getFactory().mostVisitedSites = mMostVisitedSites;

        mTileGroupDelegateImpl =
                new TileGroupDelegateImpl(
                        mContext, mProfile, mNavigationDelegate, mSnackbarManager);
    }

    @After
    public void tearDown() {
        mTileGroupDelegateImpl.destroy();
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.MOST_VISITED_TILES_RESELECT})
    public void testOpenMostVisitedItem_DisableReselect() {
        GURL url = JUnitTestGURLs.URL_1;
        mTileGroupDelegateImpl.openMostVisitedItem(
                WindowOpenDisposition.CURRENT_TAB, makeTile("Foo", url, 0));
        verify(mNavigationDelegate, never()).maybeSelectTabWithUrl(any(GURL.class));
        verify(mNavigationDelegate)
                .navigateToSuggestionUrl(
                        eq(WindowOpenDisposition.CURRENT_TAB), eq(url.getSpec()), eq(false));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.MOST_VISITED_TILES_RESELECT})
    public void testOpenMostVisitedItem_EnableReselectTriggered() {
        GURL url = JUnitTestGURLs.URL_1;
        // Attempt to select tab with `url` but fail.
        doReturn(false).when(mNavigationDelegate).maybeSelectTabWithUrl(any(GURL.class));
        mTileGroupDelegateImpl.openMostVisitedItem(
                WindowOpenDisposition.CURRENT_TAB, makeTile("Foo", url, 0));
        verify(mNavigationDelegate).maybeSelectTabWithUrl(eq(url));
        verify(mNavigationDelegate)
                .navigateToSuggestionUrl(
                        eq(WindowOpenDisposition.CURRENT_TAB), eq(url.getSpec()), eq(false));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.MOST_VISITED_TILES_RESELECT})
    public void testOpenMostVisitedItem_EnableReselectFallback() {
        GURL url = JUnitTestGURLs.URL_1;
        // Attempt to select tab with `url` and succeed.
        doReturn(true).when(mNavigationDelegate).maybeSelectTabWithUrl(any(GURL.class));
        mTileGroupDelegateImpl.openMostVisitedItem(
                WindowOpenDisposition.CURRENT_TAB, makeTile("Foo", url, 0));
        verify(mNavigationDelegate).maybeSelectTabWithUrl(eq(url));
        verify(mNavigationDelegate, never())
                .navigateToSuggestionUrl(anyInt(), anyString(), anyBoolean());
    }

    private Tile makeTile(String title, GURL url, int index) {
        SiteSuggestion siteSuggestion =
                new SiteSuggestion(
                        title,
                        url,
                        TileTitleSource.TITLE_TAG,
                        TileSource.TOP_SITES,
                        TileSectionType.UNKNOWN);
        return new Tile(siteSuggestion, index);
    }
}
