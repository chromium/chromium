// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites.createCustomLinkSiteSuggestion;
import static org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites.createSiteSuggestion;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Color;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.TextView;

import org.hamcrest.CoreMatchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.NewTabPageUtils.LayoutType;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.suggestions.ImageFetcher;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig.TileStyle;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSites;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TileGroup}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TileGroupUnitTest {
    private static class FakeImageFetcher extends ImageFetcher {
        private final List<LargeIconCallback> mCallbackList = new ArrayList<>();

        public FakeImageFetcher() {
            super(null);
        }

        @Override
        public void makeLargeIconRequest(GURL url, int size, LargeIconCallback callback) {
            mCallbackList.add(callback);
        }

        public void fulfillLargeIconRequests(Bitmap bitmap, int color, boolean isColorDefault) {
            for (LargeIconCallback callback : mCallbackList) {
                callback.onLargeIconAvailable(bitmap, color, isColorDefault, IconType.INVALID);
            }
            mCallbackList.clear();
        }

        public int getPendingIconCallbackCount() {
            return mCallbackList.size();
        }

        public void fulfillLargeIconRequests() {
            fulfillLargeIconRequests(Bitmap.createBitmap(1, 1, Config.ALPHA_8), Color.BLACK, false);
        }
    }

    private static final int MAX_TILES_TO_FETCH = 4;
    private static final int TILE_TITLE_LINES = 1;
    private static final String[] URLS = {"https://www.google.com/", "https://tellmedadjokes.com/"};
    private static final GURL AI_MODE_TEST_URL = JUnitTestGURLs.URL_1;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TileGroup.Observer mTileGroupObserver;
    @Mock private TileGroup.Delegate mTileGroupDelegate;
    @Mock private TileDragDelegate mTileDragDelegate;
    @Mock private SuggestionsUiDelegate mSuggestionsUiDelegate;
    @Mock private ContextMenuManager mContextMenuManager;
    @Mock private OfflinePageBridge mOfflinePageBridge;
    @Mock private ImageFetcher mMockImageFetcher;

    private Context mContext;
    private FakeMostVisitedSites mMostVisitedSites;
    private FakeImageFetcher mImageFetcher;
    private TileRenderer mTileRenderer;

    @Before
    public void setUp() {

        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mImageFetcher = new FakeImageFetcher();
        mTileRenderer =
                new TileRenderer(mContext, TileStyle.MODERN, TILE_TITLE_LINES, mImageFetcher);
        mMostVisitedSites = new FakeMostVisitedSites();

        doAnswer(
                        invocation -> {
                            mMostVisitedSites.setObserver(
                                    invocation.getArgument(0), invocation.<Integer>getArgument(1));
                            return null;
                        })
                .when(mTileGroupDelegate)
                .setMostVisitedSitesObserver(any(MostVisitedSites.Observer.class), anyInt());
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.NTP_MVT_AI_MODE_TILE_INDEX);
    }

    @Test
    public void testInitialiseWithTileList() {
        mMostVisitedSites.setTileSuggestionsPassive(URLS);

        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        mSuggestionsUiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileDragDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        verify(mTileGroupObserver).onTileCountChanged();
        verify(mTileGroupObserver).onTileDataChanged();

        verify(mTileGroupDelegate, never()).onLoadingComplete(any());
        assertTrue(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
        assertFalse(tileGroup.isTaskPending(TileGroup.TileTask.FETCH_DATA));
    }

    /**
     * Test the special case during initialisation, where we notify TileGroup.Observer of changes
     * event though the data did not change (still empty just like before initialisation).
     */
    @Test
    public void testInitialiseWithEmptyTileList() {
        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        mSuggestionsUiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileDragDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        verify(mTileGroupObserver).onTileCountChanged();
        verify(mTileGroupObserver).onTileDataChanged();

        verify(mTileGroupDelegate, never()).onLoadingComplete(any());
        assertTrue(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
        assertFalse(tileGroup.isTaskPending(TileGroup.TileTask.FETCH_DATA));
    }

    @Test
    // If this flakes again, refer to https://crbug.com/40848015.
    public void testReceiveNewTilesWithoutChanges() {
        TileGroup tileGroup = initialiseTileGroup(URLS);

        // Notify the same thing. No changes so|mTileGroupObserver| should not be notified.
        mMostVisitedSites.setTileSuggestionsPassive(URLS);

        verifyNoMoreInteractions(mTileGroupObserver);
        verifyNoMoreInteractions(mTileGroupDelegate);
        assertFalse(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
    }

    @Test
    @DisabledTest(message = "https://crbug.com/40848015")
    public void testReceiveNewTilesWithoutChanges_TrackLoad() {
        TileGroup tileGroup = initialiseTileGroup(/* deferLoad= */ true, URLS);

        // Notify the same thing. No changes so|mTileGroupObserver| should not be notified.
        mMostVisitedSites.setTileSuggestionsPassive(URLS);
        tileGroup.onSwitchToForeground(/* trackLoadTask= */ true);

        verifyNoMoreInteractions(mTileGroupObserver);
        verifyNoMoreInteractions(mTileGroupDelegate);
        assertFalse(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
    }

    @Test
    public void testReceiveNewTilesWithDataChanges() {
        TileGroup tileGroup = initialiseTileGroup(URLS);

        // Notify the about different URLs, but the same number. #onTileCountChanged() should not be
        // called.
        mMostVisitedSites.setTileSuggestionsPassive("http://foo.com", "http://bar.com");

        verify(mTileGroupObserver, never()).onTileCountChanged(); // Tile count is still 2.
        verify(mTileGroupObserver).onTileDataChanged(); // Data DID change.

        // No load task the second time.
        verify(mTileGroupDelegate, never()).onLoadingComplete(any());
        assertFalse(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
    }

    @Test
    // If this flakes again, refer to https://crbug.com/40227230, https://crbug.com/40819839.
    public void testReceiveNewTilesWithDataChanges_TrackLoad() {
        TileGroup tileGroup = initialiseTileGroup(/* deferLoad= */ true, URLS);

        // Notify the about different URLs, but the same number. #onTileCountChanged() should not be
        // called.
        mMostVisitedSites.setTileSuggestionsPassive("http://foo.com", "http://bar.com");
        tileGroup.onSwitchToForeground(/* trackLoadTask= */ true);

        verify(mTileGroupObserver).onTileDataChanged(); // Now data DID change.
        verify(mTileGroupObserver, never()).onTileCountChanged(); // Tile count is still 2.

        // We should now have a pending task.
        verify(mTileGroupDelegate, never()).onLoadingComplete(any());
        assertTrue(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
    }

    @Test
    public void testReceiveNewTilesWithCountChanges() {
        TileGroup tileGroup = initialiseTileGroup(URLS);

        mMostVisitedSites.setTileSuggestions(URLS[0]);

        verify(mTileGroupObserver).onTileCountChanged(); // Tile count DID change.
        verify(mTileGroupObserver).onTileDataChanged(); // Data DID change.
        verify(mTileGroupDelegate, never())
                .onLoadingComplete(any()); // No load task the second time.
        assertFalse(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
    }

    @Test
    public void testTileLoadingWhenVisibleNotBlockedForInit() {
        SuggestionsUiDelegate uiDelegate = mSuggestionsUiDelegate;
        when(uiDelegate.isVisible()).thenReturn(true);
        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        uiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileDragDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        mMostVisitedSites.setTileSuggestionsPassive(URLS);

        // Because it's the first load, we accept the incoming tiles and refresh the view.
        verify(mTileGroupObserver).onTileDataChanged();
    }

    @Test
    public void testTileLoading_DataChanged_CancelsActiveDragSession() {
        SuggestionsUiDelegate uiDelegate = mSuggestionsUiDelegate;
        when(uiDelegate.isVisible()).thenReturn(true);
        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        uiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileDragDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        mMostVisitedSites.setTileSuggestionsPassive(URLS);

        verify(mTileDragDelegate).cancelActiveSession();
    }

    @Test
    public void testTileLoadingWhenVisibleBlocked() {
        SuggestionsUiDelegate uiDelegate = mSuggestionsUiDelegate;
        when(uiDelegate.isVisible()).thenReturn(true);
        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        uiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileDragDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        mMostVisitedSites.setTileSuggestionsPassive(URLS);
        reset(mTileGroupObserver);

        mMostVisitedSites.setTileSuggestionsPassive(URLS[0]);

        // Even though the data changed, the notification should not happen because we want to not
        // show changes to UI elements currently visible
        verify(mTileGroupObserver, never()).onTileDataChanged();

        // Simulating a switch from background to foreground should force the tilegrid to load the
        // new data.
        tileGroup.onSwitchToForeground(true);
        verify(mTileGroupObserver).onTileDataChanged();
    }

    @Test
    public void testTileLoadingWhenVisibleBlocked_2() {
        TileGroup tileGroup = initialiseTileGroup(true, URLS);

        mMostVisitedSites.setTileSuggestionsPassive(URLS[0]);

        // Even though the data changed, the notification should not happen because we want to not
        // show changes to UI elements currently visible
        verify(mTileGroupObserver, never()).onTileDataChanged();

        // Simulating a switch from background to foreground should force the tilegrid to load the
        // new data.
        tileGroup.onSwitchToForeground(true);
        verify(mTileGroupObserver).onTileDataChanged();
    }

    @Test
    // If this flakes again, refer to https://crbug.com/40227230, https://crbug.com/40819839.
    public void testRenderTileView() {
        SuggestionsUiDelegate uiDelegate = mSuggestionsUiDelegate;
        when(uiDelegate.getImageFetcher()).thenReturn(mImageFetcher);
        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        uiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileDragDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        MostVisitedTilesLayout layout = setupView();

        // Initialise the internal list of tiles
        mMostVisitedSites.setTileSuggestionsPassive(URLS);

        // Render them to the layout.
        refreshData(tileGroup, layout);
        assertThat(layout.getChildCount(), is(3));
        assertThat(((SuggestionsTileView) layout.getChildAt(0)).getUrl().getSpec(), is(URLS[0]));
        assertThat(((SuggestionsTileView) layout.getChildAt(1)).getUrl().getSpec(), is(URLS[1]));
        assertTrue(isAddNewButton(layout.getChildAt(2)));

        // Rerun to test SuggestionsTileView caching.
        refreshData(tileGroup, layout);
    }

    /** Check for https://crbug.com/40511776: don't crash on duplicated URLs. */
    @Test
    public void testRenderTileViewWithDuplicatedUrl() {
        SuggestionsUiDelegate uiDelegate = mSuggestionsUiDelegate;
        when(uiDelegate.getImageFetcher()).thenReturn(mMockImageFetcher);
        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        uiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileDragDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);
        MostVisitedTilesLayout layout = setupView();

        // Initialise the internal list of tiles
        mMostVisitedSites.setTileSuggestionsPassive(URLS[0], URLS[1], URLS[0]);

        // Render them to the layout. The duplicated URL should not trigger an exception.
        refreshData(tileGroup, layout);
        // Rerun to test SuggestionsTileView caching.
        refreshData(tileGroup, layout);
    }

    @Test
    // If this flakes again, refer to https://crbug.com/40815816.
    public void testRenderTileViewReplacing() {
        SuggestionsUiDelegate uiDelegate = mSuggestionsUiDelegate;
        when(uiDelegate.getImageFetcher()).thenReturn(mMockImageFetcher);
        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        uiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileDragDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);
        mMostVisitedSites.setTileSuggestionsPassive(URLS);

        // Initialise the layout with views whose URLs don't match the ones of the new tiles.
        MostVisitedTilesLayout layout = setupView();
        SuggestionsTileView view1 = buildTileView(createSiteSuggestion("https://example.com/1"));
        layout.addView(view1);

        SuggestionsTileView view2 = buildTileView(createSiteSuggestion("https://example.com/2"));
        layout.addView(view2);

        // The tiles should be updated, the old ones removed.
        refreshData(tileGroup, layout);
        assertThat(layout.getChildCount(), is(3));
        assertThat(layout.indexOfChild(view1), is(-1));
        assertThat(layout.indexOfChild(view2), is(-1));
        assertTrue(isAddNewButton(layout.getChildAt(2)));

        // Rerun to test SuggestionsTileView caching.
        refreshData(tileGroup, layout);
    }

    @Test
    public void testRenderTileViewRecycling() {
        mMostVisitedSites.setTileSuggestionsPassive(URLS);
        List<SiteSuggestion> sites = mMostVisitedSites.getCurrentSites();
        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        mSuggestionsUiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileDragDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        // Initialise the layout with views whose URLs match the ones of the new tiles.
        MostVisitedTilesLayout layout = new MostVisitedTilesLayout(mContext, null);
        SuggestionsTileView view1 = buildTileView(sites.get(0));
        layout.addView(view1);

        SuggestionsTileView view2 = buildTileView(sites.get(1));
        layout.addView(view2);

        // The tiles should be updated, the old ones reused.
        refreshData(tileGroup);
        assertThat(layout.getChildCount(), is(2));
        assertThat(layout.getChildAt(0), CoreMatchers.is(view1));
        assertThat(layout.getChildAt(1), CoreMatchers.is(view2));
        // Rerun to test SuggestionsTileView caching.
        refreshData(tileGroup, layout);
    }

    @Test
    @DisabledTest(message = "https://crbug.com/40227230, https://crbug.com/40819839")
    public void testIconLoadingForInit() {
        TileGroup tileGroup = initialiseTileGroup(URLS);
        Tile tile = tileGroup.getTileSections().get(TileSectionType.PERSONALIZED).get(0);

        // Loading complete should be delayed until the icons are done loading.
        verify(mTileGroupDelegate, never()).onLoadingComplete(any());

        mImageFetcher.fulfillLargeIconRequests();

        // The load should now be complete.
        verify(mTileGroupDelegate).onLoadingComplete(any());
        verify(mTileGroupObserver).onTileIconChanged(eq(tile));
        verify(mTileGroupObserver, times(URLS.length)).onTileIconChanged(any(Tile.class));
    }

    @Test
    @DisabledTest(message = "Test is flaky, see crbug.com/40816947")
    public void testIconLoadingWhenTileNotRegistered() {
        TileGroup tileGroup = initialiseTileGroup();
        Tile tile = new Tile(createSiteSuggestion("title", URLS[0]), 0);

        TilesLinearLayout layout = new TilesLinearLayout(mContext, null);
        mTileRenderer.buildTileView(tile, layout, tileGroup.getTileSetupDelegate());

        // Ensure we run the callback for the new tile.
        assertEquals(1, mImageFetcher.getPendingIconCallbackCount());
        mImageFetcher.fulfillLargeIconRequests();

        verify(mTileGroupObserver, never()).onTileIconChanged(tile);
    }

    @Test
    @DisabledTest(message = "https://crbug.com/40227230, https://crbug.com/40819839")
    public void testIconLoading_Sync() {
        TileGroup tileGroup = initialiseTileGroup();
        mImageFetcher.fulfillLargeIconRequests();
        reset(mTileGroupObserver, mTileGroupDelegate);

        // Notify for a second set.
        mMostVisitedSites.setTileSuggestionsPassive(URLS);
        refreshData(tileGroup);
        mImageFetcher.fulfillLargeIconRequests();

        // Data changed but no loading complete event is sent
        verify(mTileGroupObserver).onTileDataChanged();
        verify(mTileGroupObserver, times(URLS.length)).onTileIconChanged(any(Tile.class));
        verify(mTileGroupDelegate, never()).onLoadingComplete(any());
    }

    @Test
    @DisabledTest(message = "https://crbug.com/40227230, https://crbug.com/40819839")
    public void testIconLoading_AsyncNoTrack() {
        TileGroup tileGroup = initialiseTileGroup(/* deferLoad= */ true);
        mImageFetcher.fulfillLargeIconRequests();
        reset(mTileGroupObserver, mTileGroupDelegate);

        // Notify for a second set.
        mMostVisitedSites.setTileSuggestionsPassive(URLS);
        tileGroup.onSwitchToForeground(/* trackLoadTask= */ false);
        refreshData(tileGroup);
        mImageFetcher.fulfillLargeIconRequests();

        // Data changed but no loading complete event is sent (same as sync)
        verify(mTileGroupObserver).onTileDataChanged();
        verify(mTileGroupObserver, times(URLS.length)).onTileIconChanged(any(Tile.class));
        verify(mTileGroupDelegate, never()).onLoadingComplete(any());
    }

    @Test
    @DisabledTest(message = "https://crbug.com/40227230, https://crbug.com/40819839")
    public void testIconLoading_AsyncTrack() {
        TileGroup tileGroup = initialiseTileGroup(/* deferLoad= */ true);
        mImageFetcher.fulfillLargeIconRequests();
        reset(mTileGroupObserver, mTileGroupDelegate);

        // Notify for a second set.
        mMostVisitedSites.setTileSuggestionsPassive(URLS);
        tileGroup.onSwitchToForeground(/* trackLoadTask= */ true);
        refreshData(tileGroup);
        mImageFetcher.fulfillLargeIconRequests();

        // Data changed but no loading complete event is sent
        verify(mTileGroupObserver).onTileDataChanged();
        verify(mTileGroupObserver, times(URLS.length)).onTileIconChanged(any(Tile.class));
        verify(mTileGroupDelegate).onLoadingComplete(any());
    }

    private MostVisitedTilesLayout setupView() {
        return new MostVisitedTilesLayout(mContext, null);
    }

    private SuggestionsTileView buildTileView(SiteSuggestion site) {
        return mTileRenderer.buildTileView(
                new Tile(site, /* index= */ 0), setupView(), /* setupDelegate= */ null);
    }

    private void refreshData(TileGroup tileGroup) {
        MostVisitedTilesLayout layout = setupView();
        refreshData(tileGroup, layout);
    }

    private void refreshData(TileGroup tileGroup, TilesLinearLayout tilesLayout) {
        assertThat(tileGroup.getTileSections().size()).isEqualTo(1);
        List<Tile> tiles = tileGroup.getTileSections().get(TileSectionType.PERSONALIZED);
        assertThat(tiles).isNotNull();
        mTileRenderer.renderTileSection(tiles, tilesLayout, tileGroup.getTileSetupDelegate());
        tileGroup.notifyTilesRendered();
    }

    /** {@link #initialiseTileGroup(boolean, String...)} override that does not defer loads. */
    private TileGroup initialiseTileGroup(String... urls) {
        return initialiseTileGroup(false, urls);
    }

    /**
     * @param deferLoad whether to defer the load until {@link
     *     TileGroup#onSwitchToForeground(boolean)} is called. Works by pretending that the UI is
     *     visible.
     * @param urls URLs used to initialise the tile group.
     */
    private TileGroup initialiseTileGroup(boolean deferLoad, String... urls) {
        when(mSuggestionsUiDelegate.getImageFetcher()).thenReturn(mImageFetcher);
        when(mSuggestionsUiDelegate.isVisible()).thenReturn(deferLoad);

        if (deferLoad) {
            mMostVisitedSites.setTileSuggestionsPassive(urls);
        } else {
            mMostVisitedSites.setTileSuggestions(urls);
        }

        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        mSuggestionsUiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileDragDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);
        refreshData(tileGroup);

        reset(mTileGroupObserver);
        reset(mTileGroupDelegate);
        return tileGroup;
    }

    @Test
    public void testCountCustomLinksBeforeIndex() {
        List<Tile> tiles = new ArrayList<>();
        assertEquals(0, TileGroup.countCustomLinksBeforeIndex(tiles, 0));
        assertEquals(0, TileGroup.countCustomLinksBeforeIndex(tiles, 5));

        Tile customTile1 =
                new Tile(
                        createCustomLinkSiteSuggestion("Custom 1", JUnitTestGURLs.URL_1.getSpec()),
                        0);
        Tile aiModeTile =
                new Tile(createSiteSuggestion("AI Mode", JUnitTestGURLs.URL_2.getSpec()), 1);
        Tile customTile2 =
                new Tile(
                        createCustomLinkSiteSuggestion("Custom 2", JUnitTestGURLs.URL_3.getSpec()),
                        2);
        Tile customTile3 =
                new Tile(
                        createCustomLinkSiteSuggestion("Custom 3", JUnitTestGURLs.RED_1.getSpec()),
                        3);
        Tile topSitesTile =
                new Tile(createSiteSuggestion("Top 1", JUnitTestGURLs.BLUE_1.getSpec()), 4);

        // Mix AI Mode tile between custom tiles: [Custom 1, AI Mode, Custom 2, Custom 3, Top 1].
        tiles.add(customTile1);
        tiles.add(aiModeTile);
        tiles.add(customTile2);
        tiles.add(customTile3);
        tiles.add(topSitesTile);

        // Before index 0: no tiles.
        assertEquals(0, TileGroup.countCustomLinksBeforeIndex(tiles, -1));
        assertEquals(0, TileGroup.countCustomLinksBeforeIndex(tiles, 0));

        // Before index 1 (AI Mode): customTile1 -> 1 custom link.
        assertEquals(1, TileGroup.countCustomLinksBeforeIndex(tiles, 1));

        // Before index 2 (Custom 2, immediately after AI Mode): customTile1 -> still 1 custom link.
        assertEquals(1, TileGroup.countCustomLinksBeforeIndex(tiles, 2));

        // Before index 3 (Custom 3): customTile1, customTile2 -> 2 custom links.
        assertEquals(2, TileGroup.countCustomLinksBeforeIndex(tiles, 3));

        // Before index 4 (Top 1): customTile1, customTile2, customTile3 -> 3 custom links.
        assertEquals(3, TileGroup.countCustomLinksBeforeIndex(tiles, 4));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NTP_AURORA_V2 + ":layout_type/" + LayoutType.INSIDE_MVT})
    public void testAiModeTile_injectedAtSlotZeroByDefault() {
        enableAiModeTileInsideMvt();
        SiteSuggestion[] sites = createSiteSuggestions(8);

        TileGroup tileGroup = initializeTileGroup(SuggestionsConfig.MAX_TILE_COUNT + 1, sites);
        List<Tile> tiles = tileGroup.getTileSections().get(TileSectionType.PERSONALIZED);
        assertThat(tiles).isNotNull();
        // Capacity scales to 9 when injected.
        assertEquals(9, tiles.size());
        assertEquals(TileSource.AI_MODE, tiles.get(0).getSource());
        assertEquals("AI Mode", tiles.get(0).getTitle());
        assertEquals(AI_MODE_TEST_URL, tiles.get(0).getUrl());
        assertEquals(0, tiles.get(0).getIndex());

        // Organic top sites follow.
        for (int i = 0; i < 8; i++) {
            assertEquals("Site " + i, tiles.get(i + 1).getTitle());
            assertEquals(i + 1, tiles.get(i + 1).getIndex());
        }
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NTP_AURORA_V2 + ":layout_type/" + LayoutType.INSIDE_MVT})
    public void testAiModeTile_suppressedWhenSavedIndexNegativeOne() {
        enableAiModeTileInsideMvt();
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.NTP_MVT_AI_MODE_TILE_INDEX, -1);

        assertAiModeTileSuppressed(createSiteSuggestions(8));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NTP_AURORA_V2 + ":layout_type/" + LayoutType.INSIDE_MVT})
    public void testAiModeTile_suppressedWhenUrlNull() {
        enableAiModeTileInsideMvt();
        when(mTileGroupDelegate.getAiModeTileUrl()).thenReturn(null);

        assertAiModeTileSuppressed(createSiteSuggestions(8));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.NTP_AURORA_V2})
    public void testAiModeTile_suppressedWhenFeatureDisabled() {
        enableAiModeTileInsideMvt();

        assertAiModeTileSuppressed(createSiteSuggestions(8));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NTP_AURORA_V2 + ":layout_type/" + LayoutType.INSIDE_MVT})
    public void testAiModeTile_collisionCustomLinkTakesPrecedence() {
        enableAiModeTileInsideMvt();
        // CT > AIT: Custom link has AI Mode URL. Custom link takes precedence; AI Mode tile is
        // suppressed.
        SiteSuggestion[] sites = new SiteSuggestion[3];
        sites[0] =
                createCustomLinkSiteSuggestion(
                        "My Gemini", AI_MODE_TEST_URL.getSpec()); // customized tile
        sites[1] = createSiteSuggestion("Site 1", JUnitTestGURLs.URL_2.getSpec());
        sites[2] = createSiteSuggestion("Site 2", JUnitTestGURLs.URL_3.getSpec());

        TileGroup tileGroup = initializeTileGroup(SuggestionsConfig.MAX_TILE_COUNT, sites);
        List<Tile> tiles = tileGroup.getTileSections().get(TileSectionType.PERSONALIZED);
        assertThat(tiles).isNotNull();
        // AI Mode tile should be suppressed; custom link remains.
        assertEquals(3, tiles.size());
        assertTrue(tiles.get(0).isCustomLink());
        assertEquals("My Gemini", tiles.get(0).getTitle());
        for (Tile tile : tiles) {
            assertTrue(tile.getSource() != TileSource.AI_MODE);
        }
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NTP_AURORA_V2 + ":layout_type/" + LayoutType.INSIDE_MVT})
    public void testAiModeTile_collisionOrganicTopSiteReplaced() {
        enableAiModeTileInsideMvt();
        // AIT > TST: Organic top site has AI Mode URL. AI Mode tile replaces organic top site.
        SiteSuggestion[] sites = new SiteSuggestion[3];
        sites[0] = createSiteSuggestion("Site 0", JUnitTestGURLs.URL_2.getSpec());
        sites[1] = createSiteSuggestion("AI Mode", AI_MODE_TEST_URL.getSpec());
        sites[2] = createSiteSuggestion("Site 2", JUnitTestGURLs.URL_3.getSpec());

        TileGroup tileGroup = initializeTileGroup(SuggestionsConfig.MAX_TILE_COUNT + 1, sites);
        List<Tile> tiles = tileGroup.getTileSections().get(TileSectionType.PERSONALIZED);
        assertThat(tiles).isNotNull();
        // Organic top site replaced; AI Mode tile injected at slot 0.
        assertEquals(3, tiles.size());
        assertEquals(TileSource.AI_MODE, tiles.get(0).getSource());
        assertEquals("AI Mode", tiles.get(0).getTitle());
        assertEquals("Site 0", tiles.get(1).getTitle());
        assertEquals("Site 2", tiles.get(2).getTitle());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NTP_AURORA_V2 + ":layout_type/" + LayoutType.INSIDE_MVT})
    public void testAiModeTile_editIgnored() {
        enableAiModeTileInsideMvt();
        SiteSuggestion[] sites = createSiteSuggestions(2);

        TileGroup tileGroup = initializeTileGroup(SuggestionsConfig.MAX_TILE_COUNT + 1, sites);
        List<Tile> tiles = tileGroup.getTileSections().get(TileSectionType.PERSONALIZED);
        assertThat(tiles).isNotNull();
        Tile aiTile = tiles.get(0);
        assertTrue(aiTile.isAiMode());

        tileGroup.getTileSetupDelegate().getCustomTileModificationDelegate().edit(aiTile.getData());

        verify(mTileGroupDelegate, never()).createCustomTileEditCoordinator(any());
    }

    private boolean isAddNewButton(View view) {
        View tileView = view.findViewById(R.id.tile_view_title);
        return tileView != null
                && (tileView instanceof TextView)
                && ((TextView) tileView).getText().toString().equals("Add new");
    }

    private SiteSuggestion[] createSiteSuggestions(int count) {
        SiteSuggestion[] sites = new SiteSuggestion[count];
        for (int i = 0; i < count; i++) {
            sites[i] = createSiteSuggestion("Site " + i, "https://site" + i + ".com");
        }
        return sites;
    }

    private void enableAiModeTileInsideMvt() {
        when(mTileGroupDelegate.getAiModeTileTitle()).thenReturn("AI Mode");
        when(mTileGroupDelegate.getAiModeTileUrl()).thenReturn(AI_MODE_TEST_URL);
    }

    private TileGroup initializeTileGroup(int maxResults, SiteSuggestion... suggestions) {
        when(mSuggestionsUiDelegate.getImageFetcher()).thenReturn(mImageFetcher);
        when(mSuggestionsUiDelegate.isVisible()).thenReturn(false);

        mMostVisitedSites.setTileSuggestions(suggestions);

        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        mSuggestionsUiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileDragDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.startObserving(maxResults);
        refreshData(tileGroup);

        return tileGroup;
    }

    private List<Tile> assertAiModeTileSuppressed(SiteSuggestion... sites) {
        TileGroup tileGroup = initializeTileGroup(SuggestionsConfig.MAX_TILE_COUNT, sites);
        List<Tile> tiles = tileGroup.getTileSections().get(TileSectionType.PERSONALIZED);
        assertThat(tiles).isNotNull();
        assertEquals(sites.length, tiles.size());
        for (Tile tile : tiles) {
            assertTrue(tile.getSource() != TileSource.AI_MODE);
        }
        return tiles;
    }
}
