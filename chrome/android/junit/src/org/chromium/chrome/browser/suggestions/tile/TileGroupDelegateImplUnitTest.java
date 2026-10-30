// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.SearchEngineService;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSites;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.search_engines.AiModeButtonUiConfig;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link TileGroupDelegateImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TileGroupDelegateImplUnitTest {
    private static final GURL NON_SEARCH_URL = JUnitTestGURLs.URL_1;
    private static final GURL SEARCH_URL = JUnitTestGURLs.URL_2;
    private static final GURL COMPOSEPLATE_URL = JUnitTestGURLs.URL_3;
    private static final GURL THIRD_PARTY_AI_URL = JUnitTestGURLs.BLUE_1;
    private static final String GOOGLE_AI_MODE_TITLE = "AI Mode";
    private static final String GOOGLE_AI_MODE_A11Y_LABEL = "Ask AI Mode in Google Search";
    private static final String THIRD_PARTY_AI_MODE_TITLE = "Red AI";
    private static final String THIRD_PARTY_AI_MODE_A11Y_LABEL = "Ask Red AI";
    private static final int MAX_RESULTS = 8;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @Mock private SuggestionsDependencyFactory mSuggestionsDependencyFactory;
    @Mock private Profile mProfile;
    @Mock private MostVisitedSites mMostVisitedSites;
    @Mock private MostVisitedSites.Observer mMostVisitedSitesObserver;
    @Mock private SuggestionsNavigationDelegate mNavigationDelegate;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private SearchEngineService mSearchEngineService;

    private final SettableNullableObservableSupplier<AiModeButtonUiConfig>
            mAiModeButtonUiConfigSupplier = ObservableSuppliers.createNullable();
    private final SettableNonNullObservableSupplier<StatusIconResource> mAiModeButtonIconSupplier =
            ObservableSuppliers.createNonNull(
                    new StatusIconResource(R.drawable.ic_search_spark_24dp, Resources.ID_NULL));

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

        // Defaults to the 1P Google DSE; AIM3P_ENTRYPOINT-disabled tests never read these.
        SearchEngineService.setInstanceForTesting(mSearchEngineService);
        mAiModeButtonUiConfigSupplier.set(createGoogleAiModeButtonUiConfig());
        lenient()
                .doReturn(mAiModeButtonUiConfigSupplier)
                .when(mSearchEngineService)
                .getAiModeButtonUiConfigSupplier();
        lenient()
                .doReturn(mAiModeButtonIconSupplier)
                .when(mSearchEngineService)
                .getAiModeButtonIconSupplier();

        mTileGroupDelegateImpl =
                new TileGroupDelegateImpl(
                        mContext, mProfile, mNavigationDelegate, mSnackbarManager);

        doReturn(false)
                .when(mTemplateUrlService)
                .isSearchResultsPageFromDefaultSearchProvider(any());
        doReturn(true)
                .when(mTemplateUrlService)
                .isSearchResultsPageFromDefaultSearchProvider(SEARCH_URL);
        lenient().doReturn(COMPOSEPLATE_URL).when(mTemplateUrlService).getComposeplateUrl();

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
    }

    @After
    public void tearDown() {
        SearchEngineService.setInstanceForTesting(null);
        TemplateUrlServiceFactory.setInstanceForTesting(null);
        // testDestroy_RemovesAiModeSupplierObservers destroys the delegate itself and nulls the
        // field.
        if (mTileGroupDelegateImpl != null) mTileGroupDelegateImpl.destroy();
    }

    @Test
    @DisableFeatures({ChromeFeatureList.MOST_VISITED_TILES_RESELECT})
    public void testOpenMostVisitedItem_DisableReselect() {
        GURL url = NON_SEARCH_URL;
        mTileGroupDelegateImpl.openMostVisitedItem(
                WindowOpenDisposition.CURRENT_TAB, makeTile("Foo", url, 0));
        verify(mNavigationDelegate, never()).maybeSelectTabWithUrl(any(GURL.class));
        verify(mNavigationDelegate)
                .navigateToSuggestionUrl(
                        eq(WindowOpenDisposition.CURRENT_TAB), eq(url.getSpec()), eq(false));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.MOST_VISITED_TILES_RESELECT})
    public void testOpenMostVisitedItem_EnableReselectSearchUrl() {
        GURL url = SEARCH_URL;
        // Search `url` used, doesn't attempt to reselect tab.
        mTileGroupDelegateImpl.openMostVisitedItem(
                WindowOpenDisposition.CURRENT_TAB, makeTile("Foo", url, 0));
        verify(mNavigationDelegate, never()).maybeSelectTabWithUrl(any(GURL.class));
        verify(mNavigationDelegate)
                .navigateToSuggestionUrl(
                        eq(WindowOpenDisposition.CURRENT_TAB), eq(url.getSpec()), eq(false));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.MOST_VISITED_TILES_RESELECT})
    public void testOpenMostVisitedItem_EnableReselectTriggered() {
        GURL url = NON_SEARCH_URL;
        // Attempt to reselect tab with `url` but fail.
        doReturn(false).when(mNavigationDelegate).maybeSelectTabWithUrl(any(GURL.class));
        mTileGroupDelegateImpl.openMostVisitedItem(
                WindowOpenDisposition.CURRENT_TAB, makeTile("Foo", url, 0));
        verify(mNavigationDelegate).maybeSelectTabWithUrl(eq(url));
        verify(mNavigationDelegate)
                .navigateToSuggestionUrl(
                        eq(WindowOpenDisposition.CURRENT_TAB), eq(url.getSpec()), eq(false));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.MOST_VISITED_TILES_RESELECT})
    public void testOpenMostVisitedItem_EnableReselectFallback() {
        GURL url = NON_SEARCH_URL;
        // Attempt to reselect tab with `url` and succeed.
        doReturn(true).when(mNavigationDelegate).maybeSelectTabWithUrl(any(GURL.class));
        mTileGroupDelegateImpl.openMostVisitedItem(
                WindowOpenDisposition.CURRENT_TAB, makeTile("Foo", url, 0));
        verify(mNavigationDelegate).maybeSelectTabWithUrl(eq(url));
        verify(mNavigationDelegate, never())
                .navigateToSuggestionUrl(anyInt(), anyString(), anyBoolean());
    }

    @Test
    public void testRemoveMostVisitedItem_ShowsSnackbar() {
        mTileGroupDelegateImpl.removeMostVisitedItem(makeTile("Foo", NON_SEARCH_URL, 0));

        verify(mMostVisitedSites).addBlocklistedUrl(NON_SEARCH_URL);
        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());
        Snackbar snackbar = snackbarCaptor.getValue();
        Assert.assertEquals(
                mContext.getString(R.string.most_visited_item_removed),
                snackbar.getTextForTesting());

        snackbar.getController().onAction(snackbar.getActionData());
        verify(mMostVisitedSites).removeBlocklistedUrl(NON_SEARCH_URL);
    }

    @Test
    public void testShowTileRemovedSnackbar() {
        Runnable undoHandler = mock(Runnable.class);
        mTileGroupDelegateImpl.showTileRemovedSnackbar(undoHandler);

        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());
        Snackbar snackbar = snackbarCaptor.getValue();
        Assert.assertEquals(
                mContext.getString(R.string.most_visited_item_removed),
                snackbar.getTextForTesting());

        snackbar.getController().onAction(snackbar.getActionData());
        verify(undoHandler).run();
    }

    @Test
    public void testShowTileUnpinSnackbar() {
        // Unlike `removeMostVisitedItem()`, `deleteCustomLink()` relies on the caller to show a
        // snackbar. This test is therefore limited to verifying the snackbar logic via
        // `showTileUnpinSnackbar()`, rather than the full unpin flow.
        mTileGroupDelegateImpl.showTileUnpinSnackbar(() -> {});

        ArgumentCaptor<Snackbar> snackbarCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager).showSnackbar(snackbarCaptor.capture());
        Assert.assertEquals(
                mContext.getString(R.string.custom_tile_unpinned),
                snackbarCaptor.getValue().getTextForTesting());
    }

    @Test
    @DisableFeatures({OmniboxFeatureList.AIM3P_ENTRYPOINT})
    public void testAiModeGetters_Aim3pEntrypointDisabled() {
        // Verifies that all AI Mode delegate getters return the built-in 1P Google defaults when
        // AIM3P_ENTRYPOINT is disabled.
        assertEquals(
                mContext.getString(R.string.ai_mode_entrypoint_label),
                mTileGroupDelegateImpl.getAiModeTileTitle());
        assertEquals(COMPOSEPLATE_URL, mTileGroupDelegateImpl.getAiModeTileUrl());
        assertEquals(
                mContext.getString(R.string.accessibility_ai_mode_mvt_tile),
                mTileGroupDelegateImpl.getAiModeTileContentDescription());
        Drawable icon = mTileGroupDelegateImpl.getAiModeTileIconDrawable();
        assertEquals(R.drawable.search_spark_white_24dp, shadowOf(icon).getCreatedFromResId());
    }

    @Test
    @DisableFeatures({OmniboxFeatureList.AIM3P_ENTRYPOINT})
    public void testSetMostVisitedSitesObserver_Aim3pEntrypointDisabled() {
        // Verifies that setMostVisitedSitesObserver passes the observer directly to
        // MostVisitedSites without subscribing to SearchEngineService suppliers when
        // AIM3P_ENTRYPOINT is disabled.
        mTileGroupDelegateImpl.setMostVisitedSitesObserver(mMostVisitedSitesObserver, MAX_RESULTS);

        verify(mMostVisitedSites).setObserver(mMostVisitedSitesObserver, MAX_RESULTS);
        assertFalse(mAiModeButtonUiConfigSupplier.hasObservers());
        assertFalse(mAiModeButtonIconSupplier.hasObservers());
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.AIM3P_ENTRYPOINT})
    public void testAiModeGetters_Aim3pEntrypointEnabled_GoogleDse() {
        // Verifies that with the 1P Google DSE (empty navigationUrlEmpty, resource spark icon) the
        // getters return the config text and a11y label, fall back to the composeplate URL, and
        // pass the spark drawable through from the icon supplier.
        assertEquals(GOOGLE_AI_MODE_TITLE, mTileGroupDelegateImpl.getAiModeTileTitle());
        assertEquals(COMPOSEPLATE_URL, mTileGroupDelegateImpl.getAiModeTileUrl());
        assertEquals(
                GOOGLE_AI_MODE_A11Y_LABEL,
                mTileGroupDelegateImpl.getAiModeTileContentDescription());
        Drawable icon = mTileGroupDelegateImpl.getAiModeTileIconDrawable();
        assertEquals(R.drawable.ic_search_spark_24dp, shadowOf(icon).getCreatedFromResId());
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.AIM3P_ENTRYPOINT})
    public void testAiModeGetters_Aim3pEntrypointEnabled_ThirdPartyDse() {
        // Verifies that with a 3P DSE the getters return the config text, a11y label, and
        // navigationUrlEmpty, and pass the downloaded favicon bitmap through from the icon
        // supplier.
        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        mAiModeButtonUiConfigSupplier.set(createThirdPartyAiModeButtonUiConfig());
        mAiModeButtonIconSupplier.set(createBitmapAiModeButtonIcon(bitmap));

        assertEquals(THIRD_PARTY_AI_MODE_TITLE, mTileGroupDelegateImpl.getAiModeTileTitle());
        assertEquals(THIRD_PARTY_AI_URL, mTileGroupDelegateImpl.getAiModeTileUrl());
        assertEquals(
                THIRD_PARTY_AI_MODE_A11Y_LABEL,
                mTileGroupDelegateImpl.getAiModeTileContentDescription());
        Drawable icon = mTileGroupDelegateImpl.getAiModeTileIconDrawable();
        assertEquals(bitmap, ((BitmapDrawable) icon).getBitmap());
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.AIM3P_ENTRYPOINT})
    public void testAiModeGetters_Aim3pEntrypointEnabled_NoAiModeConfig() {
        // Verifies that when the DSE does not support AI Mode (null config) every getter returns
        // null so TileGroup hides the tile, even though the icon supplier still holds a drawable.
        mAiModeButtonUiConfigSupplier.set(null);

        assertNull(mTileGroupDelegateImpl.getAiModeTileTitle());
        assertNull(mTileGroupDelegateImpl.getAiModeTileUrl());
        assertNull(mTileGroupDelegateImpl.getAiModeTileContentDescription());
        assertNull(mTileGroupDelegateImpl.getAiModeTileIconDrawable());
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.AIM3P_ENTRYPOINT})
    public void testSetMostVisitedSitesObserver_Aim3pEntrypointEnabled_PassesObserverThrough() {
        // Verifies that the TileGroup observer is handed to MostVisitedSites unwrapped, so regular
        // suggestion and favicon callbacks keep flowing directly, while the delegate additionally
        // subscribes to both SearchEngineService suppliers.
        mTileGroupDelegateImpl.setMostVisitedSitesObserver(mMostVisitedSitesObserver, MAX_RESULTS);

        verify(mMostVisitedSites).setObserver(mMostVisitedSitesObserver, MAX_RESULTS);
        assertTrue(mAiModeButtonUiConfigSupplier.hasObservers());
        assertTrue(mAiModeButtonIconSupplier.hasObservers());
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.AIM3P_ENTRYPOINT})
    public void testAiModeConfigObserver_NotifiesConfigChange() {
        // Verifies that every AI Mode config change (3P DSE, then a DSE without AI Mode) notifies
        // TileGroup via onAiModeTileConfigChanged() instead of replaying site suggestions, so
        // TileGroup rebuilds tiles from its own cached list.
        mTileGroupDelegateImpl.setMostVisitedSitesObserver(mMostVisitedSitesObserver, MAX_RESULTS);
        clearInvocations(mMostVisitedSitesObserver);

        mAiModeButtonUiConfigSupplier.set(createThirdPartyAiModeButtonUiConfig());
        verify(mMostVisitedSitesObserver).onAiModeTileConfigChanged();

        mAiModeButtonUiConfigSupplier.set(null);
        verify(mMostVisitedSitesObserver, times(2)).onAiModeTileConfigChanged();
        verify(mMostVisitedSitesObserver, never()).onSiteSuggestionsAvailable(anyBoolean(), any());
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.AIM3P_ENTRYPOINT})
    public void testAiModeIconObserver_NotifiesIconMadeAvailableForValidUrl() {
        // Verifies that updating the icon supplier notifies onIconMadeAvailable with the active
        // AI Mode tile URL, and skips notification when AI Mode is unavailable (null config).
        mTileGroupDelegateImpl.setMostVisitedSitesObserver(mMostVisitedSitesObserver, MAX_RESULTS);
        mAiModeButtonUiConfigSupplier.set(createThirdPartyAiModeButtonUiConfig());

        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        mAiModeButtonIconSupplier.set(createBitmapAiModeButtonIcon(bitmap));
        verify(mMostVisitedSitesObserver).onIconMadeAvailable(THIRD_PARTY_AI_URL);
        clearInvocations(mMostVisitedSitesObserver);

        // When config is null, icon updates must not trigger onIconMadeAvailable.
        mAiModeButtonUiConfigSupplier.set(null);
        mAiModeButtonIconSupplier.set(
                new StatusIconResource(R.drawable.ic_search_24dp, Resources.ID_NULL));
        verify(mMostVisitedSitesObserver, never()).onIconMadeAvailable(any());
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.AIM3P_ENTRYPOINT})
    public void testDestroy_RemovesAiModeSupplierObservers() {
        // Verifies that destroy() unregisters both the UI config and icon observers from
        // SearchEngineService suppliers.
        mTileGroupDelegateImpl.setMostVisitedSitesObserver(mMostVisitedSitesObserver, MAX_RESULTS);
        assertTrue(mAiModeButtonUiConfigSupplier.hasObservers());
        assertTrue(mAiModeButtonIconSupplier.hasObservers());

        mTileGroupDelegateImpl.destroy();
        mTileGroupDelegateImpl = null;

        assertFalse(mAiModeButtonUiConfigSupplier.hasObservers());
        assertFalse(mAiModeButtonIconSupplier.hasObservers());
    }

    private Tile makeTile(String title, GURL url, int index) {
        return new Tile(makeSiteSuggestion(title, url), index);
    }

    private static SiteSuggestion makeSiteSuggestion(String title, GURL url) {
        return new SiteSuggestion(
                title,
                url,
                TileTitleSource.TITLE_TAG,
                TileSource.TOP_SITES,
                TileSectionType.UNKNOWN);
    }

    private static AiModeButtonUiConfig createGoogleAiModeButtonUiConfig() {
        return new AiModeButtonUiConfig(
                /* text= */ GOOGLE_AI_MODE_TITLE,
                /* tooltip= */ "Google AI Mode",
                /* a11yLabel= */ GOOGLE_AI_MODE_A11Y_LABEL,
                /* contextMenuLabel= */ "Always show AI Mode",
                /* placeholderText= */ "Ask AI Mode",
                /* faviconUrl= */ GURL.emptyGURL(),
                /* navigationUrl= */ "",
                /* navigationUrlEmpty= */ GURL.emptyGURL());
    }

    private static AiModeButtonUiConfig createThirdPartyAiModeButtonUiConfig() {
        return new AiModeButtonUiConfig(
                /* text= */ THIRD_PARTY_AI_MODE_TITLE,
                /* tooltip= */ "Red AI assistant",
                /* a11yLabel= */ THIRD_PARTY_AI_MODE_A11Y_LABEL,
                /* contextMenuLabel= */ "Always show Red AI",
                /* placeholderText= */ "Ask Red AI",
                /* faviconUrl= */ JUnitTestGURLs.RED_1,
                /* navigationUrl= */ THIRD_PARTY_AI_URL.getSpec(),
                /* navigationUrlEmpty= */ THIRD_PARTY_AI_URL);
    }

    private static StatusIconResource createBitmapAiModeButtonIcon(Bitmap bitmap) {
        return new StatusIconResource(JUnitTestGURLs.RED_1.getSpec(), bitmap, Resources.ID_NULL);
    }
}
