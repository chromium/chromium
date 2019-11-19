// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites.createSiteSuggestion;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.hamcrest.CoreMatchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.favicon.IconType;
import org.chromium.chrome.browser.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.cards.CardsVariationParameters;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.suggestions.ImageFetcher;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig.TileStyle;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSites;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/**
 * Unit tests for {@link TileGroup}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TileGroupUnitTest {
    private static final int MAX_TILES_TO_FETCH = 4;
    private static final int TILE_TITLE_LINES = 1;
    private static final String[] URLS = {"https://www.google.com", "https://tellmedadjokes.com"};

    @Rule
    public TestRule mFeaturesProcessor = new Features.JUnitProcessor();

    @Mock
    private TileGroup.Observer mTileGroupObserver;
    @Mock
    private TileGroup.Delegate mTileGroupDelegate;

    private FakeMostVisitedSites mMostVisitedSites;
    private FakeImageFetcher mImageFetcher;
    private TileRenderer mTileRenderer;

    @Before
    public void setUp() {
        CardsVariationParameters.setTestVariationParams(new HashMap<>());
        MockitoAnnotations.initMocks(this);

        mImageFetcher = new FakeImageFetcher();
        mTileRenderer = new TileRenderer(
                RuntimeEnvironment.application, TileStyle.MODERN, TILE_TITLE_LINES, mImageFetcher);
        mMostVisitedSites = new FakeMostVisitedSites();

        doAnswer(invocation -> {
            mMostVisitedSites.setObserver(
                    invocation.getArgument(0), invocation.<Integer>getArgument(1));
            return null;
        })
                .when(mTileGroupDelegate)
                .setMostVisitedSitesObserver(any(MostVisitedSites.Observer.class), anyInt());
    }

    @After
    public void tearDown() {
        CardsVariationParameters.setTestVariationParams(null);
    }

    @Test
    public void testInitialiseWithTileList() {
        mMostVisitedSites.setTileSuggestions(URLS);

        TileGroup tileGroup = new TileGroup(mTileRenderer, mock(SuggestionsUiDelegate.class),
                mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                mock(OfflinePageBridge.class));
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
        TileGroup tileGroup = new TileGroup(mTileRenderer, mock(SuggestionsUiDelegate.class),
                mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                mock(OfflinePageBridge.class));
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        verify(mTileGroupObserver).onTileCountChanged();
        verify(mTileGroupObserver).onTileDataChanged();

        verify(mTileGroupDelegate, never()).onLoadingComplete(any());
        assertTrue(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
        assertFalse(tileGroup.isTaskPending(TileGroup.TileTask.FETCH_DATA));
    }

    @Test
    public void testReceiveNewTilesWithoutChanges() {
        TileGroup tileGroup = initialiseTileGroup(URLS);

        // Notify the same thing. No changes so|mTileGroupObserver| should not be notified.
        mMostVisitedSites.setTileSuggestions(URLS);

        verifyNoMoreInteractions(mTileGroupObserver);
        verifyNoMoreInteractions(mTileGroupDelegate);
        assertFalse(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
    }

    @Test
    public void testReceiveNewTilesWithoutChanges_TrackLoad() {
        TileGroup tileGroup = initialiseTileGroup(/* deferLoad: */ true, URLS);

        // Notify the same thing. No changes so|mTileGroupObserver| should not be notified.
        mMostVisitedSites.setTileSuggestions(URLS);
        tileGroup.onSwitchToForeground(/* trackLoadTask: */ true);

        verifyNoMoreInteractions(mTileGroupObserver);
        verifyNoMoreInteractions(mTileGroupDelegate);
        assertFalse(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
    }

    @Test
    public void testReceiveNewTilesWithDataChanges() {
        TileGroup tileGroup = initialiseTileGroup(URLS);

        // Notify the about different URLs, but the same number. #onTileCountChanged() should not be
        // called.
        mMostVisitedSites.setTileSuggestions("foo", "bar");

        verify(mTileGroupObserver, never()).onTileCountChanged(); // Tile count is still 2.
        verify(mTileGroupObserver).onTileDataChanged(); // Data DID change.

        // No load task the second time.
        verify(mTileGroupDelegate, never()).onLoadingComplete(any());
        assertFalse(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
    }

    @Test
    public void testReceiveNewTilesWithDataChanges_TrackLoad() {
        TileGroup tileGroup = initialiseTileGroup(/* deferLoad: */ true, URLS);

        // Notify the about different URLs, but the same number. #onTileCountChanged() should not be
        // called.
        mMostVisitedSites.setTileSuggestions("foo", "bar");
        tileGroup.onSwitchToForeground(/* trackLoadTask: */ true);

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
        SuggestionsUiDelegate uiDelegate = mock(SuggestionsUiDelegate.class);
        when(uiDelegate.isVisible()).thenReturn(true);
        TileGroup tileGroup =
                new TileGroup(mTileRenderer, uiDelegate, mock(ContextMenuManager.class),
                        mTileGroupDelegate, mTileGroupObserver, mock(OfflinePageBridge.class));
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        mMostVisitedSites.setTileSuggestions(URLS);

        // Because it's the first load, we accept the incoming tiles and refresh the view.
        verify(mTileGroupObserver).onTileDataChanged();
    }

    @Test
    public void testTileLoadingWhenVisibleBlocked() {
        SuggestionsUiDelegate uiDelegate = mock(SuggestionsUiDelegate.class);
        when(uiDelegate.isVisible()).thenReturn(true);
        TileGroup tileGroup =
                new TileGroup(mTileRenderer, uiDelegate, mock(ContextMenuManager.class),
                        mTileGroupDelegate, mTileGroupObserver, mock(OfflinePageBridge.class));
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        mMostVisitedSites.setTileSuggestions(URLS);
        reset(mTileGroupObserver);

        mMostVisitedSites.setTileSuggestions(URLS[0]);

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

        mMostVisitedSites.setTileSuggestions(URLS[0]);

        // Even though the data changed, the notification should not happen because we want to not
        // show changes to UI elements currently visible
        verify(mTileGroupObserver, never()).onTileDataChanged();

        // Simulating a switch from background to foreground should force the tilegrid to load the
        // new data.
        tileGroup.onSwitchToForeground(true);
        verify(mTileGroupObserver).onTileDataChanged();
    }

    @Test
    public void testRenderTileView() {
        SuggestionsUiDelegate uiDelegate = mock(SuggestionsUiDelegate.class);
        when(uiDelegate.getImageFetcher()).thenReturn(mImageFetcher);
        TileGroup tileGroup =
                new TileGroup(mTileRenderer, uiDelegate, mock(ContextMenuManager.class),
                        mTileGroupDelegate, mTileGroupObserver, mock(OfflinePageBridge.class));
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        TileGridViewHolder tileGrid = setupView(tileGroup);
        TileGridLayout layout = (TileGridLayout) tileGrid.itemView;

        // Initialise the internal list of tiles
        mMostVisitedSites.setTileSuggestions(URLS);

        // Render them to the layout.
        tileGrid.refreshData();
        assertThat(layout.getChildCount(), is(2));
        assertThat(((SuggestionsTileView) layout.getChildAt(0)).getUrl(), is(URLS[0]));
        assertThat(((SuggestionsTileView) layout.getChildAt(1)).getUrl(), is(URLS[1]));
    }

    /** Check for https://crbug.com/703628: don't crash on duplicated URLs. */
    @Test
    public void testRenderTileViewWithDuplicatedUrl() {
        SuggestionsUiDelegate uiDelegate = mock(SuggestionsUiDelegate.class);
        when(uiDelegate.getImageFetcher()).thenReturn(mock(ImageFetcher.class));
        TileGroup tileGroup =
                new TileGroup(mTileRenderer, uiDelegate, mock(ContextMenuManager.class),
                        mTileGroupDelegate, mTileGroupObserver, mock(OfflinePageBridge.class));
        tileGroup.startObserving(MAX_TILES_TO_FETCH);
        TileGridViewHolder tileGrid = setupView(tileGroup);

        // Initialise the internal list of tiles
        mMostVisitedSites.setTileSuggestions(URLS[0], URLS[1], URLS[0]);

        // Render them to the layout. The duplicated URL should not trigger an exception.
        tileGrid.refreshData();
    }

    @Test
    public void testRenderTileViewReplacing() {
        SuggestionsUiDelegate uiDelegate = mock(SuggestionsUiDelegate.class);
        when(uiDelegate.getImageFetcher()).thenReturn(mock(ImageFetcher.class));
        TileGroup tileGroup =
                new TileGroup(mTileRenderer, uiDelegate, mock(ContextMenuManager.class),
                        mTileGroupDelegate, mTileGroupObserver, mock(OfflinePageBridge.class));
        tileGroup.startObserving(MAX_TILES_TO_FETCH);
        mMostVisitedSites.setTileSuggestions(URLS);

        // Initialise the layout with views whose URLs don't match the ones of the new tiles.
        TileGridViewHolder tileGrid = setupView(tileGroup);
        TileGridLayout layout = (TileGridLayout) tileGrid.itemView;
        SuggestionsTileView view1 = mock(SuggestionsTileView.class);
        layout.addView(view1);

        SuggestionsTileView view2 = mock(SuggestionsTileView.class);
        layout.addView(view2);

        // The tiles should be updated, the old ones removed.
        tileGrid.refreshData();
        assertThat(layout.getChildCount(), is(2));
        assertThat(layout.indexOfChild(view1), is(-1));
        assertThat(layout.indexOfChild(view2), is(-1));
    }

    @Test
    public void testRenderTileViewRecycling() {
        mMostVisitedSites.setTileSuggestions(URLS);
        List<SiteSuggestion> sites = mMostVisitedSites.getCurrentSites();
        TileGroup tileGroup = new TileGroup(mTileRenderer, mock(SuggestionsUiDelegate.class),
                mock(ContextMenuManager.class), mTileGroupDelegate, mTileGroupObserver,
                mock(OfflinePageBridge.class));
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        // Initialise the layout with views whose URLs match the ones of the new tiles.
        TileGridLayout layout = new TileGridLayout(RuntimeEnvironment.application, null);
        SuggestionsTileView view1 = mock(SuggestionsTileView.class);
        when(view1.getData()).thenReturn(sites.get(0));
        layout.addView(view1);

        SuggestionsTileView view2 = mock(SuggestionsTileView.class);
        when(view2.getData()).thenReturn(sites.get(1));
        layout.addView(view2);

        // The tiles should be updated, the old ones reused.
        setupView(tileGroup).refreshData();
        assertThat(layout.getChildCount(), is(2));
        assertThat(layout.getChildAt(0), CoreMatchers.is(view1));
        assertThat(layout.getChildAt(1), CoreMatchers.is(view2));
    }

    @Test
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
    public void testIconLoadingWhenTileNotRegistered() {
        TileGroup tileGroup = initialiseTileGroup();
        Tile tile = new Tile(createSiteSuggestion("title", URLS[0]), 0);

        ViewGroup layout = new FrameLayout(RuntimeEnvironment.application, null);
        mTileRenderer.buildTileView(tile, layout, tileGroup.getTileSetupDelegate());

        // Ensure we run the callback for the new tile.
        assertEquals(1, mImageFetcher.getPendingIconCallbackCount());
        mImageFetcher.fulfillLargeIconRequests();

        verify(mTileGroupObserver, never()).onTileIconChanged(tile);
    }

    private TileGridViewHolder setupView(TileGroup tileGroup) {
        TileGridLayout layout = new TileGridLayout(RuntimeEnvironment.application, null);
        TileGridViewHolder tileGrid = new TileGridViewHolder(layout, 4, 2);
        tileGrid.bindDataSource(tileGroup, mTileRenderer);
        return tileGrid;
    }

    @Test
    public void testIconLoading_Sync() {
        TileGroup tileGroup = initialiseTileGroup();
        mImageFetcher.fulfillLargeIconRequests();
        reset(mTileGroupObserver, mTileGroupDelegate);

        // Notify for a second set.
        mMostVisitedSites.setTileSuggestions(URLS);
        setupView(tileGroup).refreshData();
        mImageFetcher.fulfillLargeIconRequests();

        // Data changed but no loading complete event is sent
        verify(mTileGroupObserver).onTileDataChanged();
        verify(mTileGroupObserver, times(URLS.length)).onTileIconChanged(any(Tile.class));
        verify(mTileGroupDelegate, never()).onLoadingComplete(any());
    }

    @Test
    public void testIconLoading_AsyncNoTrack() {
        TileGroup tileGroup = initialiseTileGroup(/* deferLoad: */ true);
        mImageFetcher.fulfillLargeIconRequests();
        reset(mTileGroupObserver, mTileGroupDelegate);

        // Notify for a second set.
        mMostVisitedSites.setTileSuggestions(URLS);
        tileGroup.onSwitchToForeground(/* trackLoadTask: */ false);
        setupView(tileGroup).refreshData();
        mImageFetcher.fulfillLargeIconRequests();

        // Data changed but no loading complete event is sent (same as sync)
        verify(mTileGroupObserver).onTileDataChanged();
        verify(mTileGroupObserver, times(URLS.length)).onTileIconChanged(any(Tile.class));
        verify(mTileGroupDelegate, never()).onLoadingComplete(any());
    }

    @Test
    public void testIconLoading_AsyncTrack() {
        TileGroup tileGroup = initialiseTileGroup(/* deferLoad: */ true);
        mImageFetcher.fulfillLargeIconRequests();
        reset(mTileGroupObserver, mTileGroupDelegate);

        // Notify for a second set.
        mMostVisitedSites.setTileSuggestions(URLS);
        tileGroup.onSwitchToForeground(/* trackLoadTask: */ true);
        setupView(tileGroup).refreshData();
        mImageFetcher.fulfillLargeIconRequests();

        // Data changed but no loading complete event is sent
        verify(mTileGroupObserver).onTileDataChanged();
        verify(mTileGroupObserver, times(URLS.length)).onTileIconChanged(any(Tile.class));
        verify(mTileGroupDelegate).onLoadingComplete(any());
    }

    /** {@link #initialiseTileGroup(boolean, String...)} override that does not defer loads. */
    private TileGroup initialiseTileGroup(String... urls) {
        return initialiseTileGroup(false, urls);
    }

    /**
     * @param deferLoad whether to defer the load until
     *                  {@link TileGroup#onSwitchToForeground(boolean)} is called. Works by
     *                  pretending that the UI is visible.
     * @param urls URLs used to initialise the tile group.
     */
    private TileGroup initialiseTileGroup(boolean deferLoad, String... urls) {
        SuggestionsUiDelegate uiDelegate = mock(SuggestionsUiDelegate.class);
        when(uiDelegate.getImageFetcher()).thenReturn(mImageFetcher);
        when(uiDelegate.isVisible()).thenReturn(deferLoad);

        mMostVisitedSites.setTileSuggestions(urls);

        TileGroup tileGroup =
                new TileGroup(mTileRenderer, uiDelegate, mock(ContextMenuManager.class),
                        mTileGroupDelegate, mTileGroupObserver, mock(OfflinePageBridge.class));
        tileGroup.startObserving(MAX_TILES_TO_FETCH);
        setupView(tileGroup).refreshData();

        reset(mTileGroupObserver);
        reset(mTileGroupDelegate);
        return tileGroup;
    }

    private class FakeImageFetcher extends ImageFetcher {
        private final List<LargeIconCallback> mCallbackList = new ArrayList<>();

        public FakeImageFetcher() {
            super(null, null, null);
        }

        @Override
        public void makeLargeIconRequest(String url, int size, LargeIconCallback callback) {
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
            fulfillLargeIconRequests(mock(Bitmap.class), Color.BLACK, false);
        }
    }
}
