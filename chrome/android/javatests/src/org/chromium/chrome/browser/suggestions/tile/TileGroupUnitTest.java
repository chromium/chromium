// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

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

import static org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites.createSiteSuggestion;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Color;
import android.view.ContextThemeWrapper;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.CoreMatchers;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.suggestions.ImageFetcher;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig.TileStyle;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSites;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TileGroup}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class TileGroupUnitTest {
    private static final int MAX_TILES_TO_FETCH = 4;
    private static final int TILE_TITLE_LINES = 1;
    private static final String[] URLS = {"https://www.google.com/", "https://tellmedadjokes.com/"};

    @Mock private TileGroup.Observer mTileGroupObserver;
    @Mock private TileGroup.Delegate mTileGroupDelegate;
    @Mock private SuggestionsUiDelegate mSuggestionsUiDelegate;
    @Mock private ContextMenuManager mContextMenuManager;
    @Mock private OfflinePageBridge mOfflinePageBridge;
    @Mock private ImageFetcher mMockImageFetcher;
    @Mock private SuggestionsTileView mSuggestionsTileView1;
    @Mock private SuggestionsTileView mSuggestionsTileView2;

    private Context mContext;
    private FakeMostVisitedSites mMostVisitedSites;
    private FakeImageFetcher mImageFetcher;
    private TileRenderer mTileRenderer;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

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

        FeatureList.TestValues testValues = new TestValues();
        // testValues is set to avoid the FeatureListJni assertion check in tests.
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.NEW_TAB_PAGE_ANDROID_TRIGGER_FOR_PRERENDER2,
                "prerender_new_tab_page_on_touch_trigger",
                "0");
        FeatureList.setTestValues(testValues);
    }

    @Test
    @UiThreadTest
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1292469")
    public void testInitialiseWithTileList() {
        mMostVisitedSites.setTileSuggestions(URLS);

        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        mSuggestionsUiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
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
    @UiThreadTest
    @SmallTest
    public void testInitialiseWithEmptyTileList() {
        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        mSuggestionsUiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
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
    @UiThreadTest
    @SmallTest
    // If this flakes again, refer to https://crbug.com/1336867.
    public void testReceiveNewTilesWithoutChanges() {
        TileGroup tileGroup = initialiseTileGroup(URLS);

        // Notify the same thing. No changes so|mTileGroupObserver| should not be notified.
        mMostVisitedSites.setTileSuggestions(URLS);

        verifyNoMoreInteractions(mTileGroupObserver);
        verifyNoMoreInteractions(mTileGroupDelegate);
        assertFalse(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
    }

    @Test
    @UiThreadTest
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1336867")
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
    @UiThreadTest
    @SmallTest
    public void testReceiveNewTilesWithDataChanges() {
        TileGroup tileGroup = initialiseTileGroup(URLS);

        // Notify the about different URLs, but the same number. #onTileCountChanged() should not be
        // called.
        mMostVisitedSites.setTileSuggestions("http://foo.com", "http://bar.com");

        verify(mTileGroupObserver, never()).onTileCountChanged(); // Tile count is still 2.
        verify(mTileGroupObserver).onTileDataChanged(); // Data DID change.

        // No load task the second time.
        verify(mTileGroupDelegate, never()).onLoadingComplete(any());
        assertFalse(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
    }

    @Test
    @UiThreadTest
    @SmallTest
    // If this flakes again, refer to https://crbug.com/1330627, https://crbug.com/1293208.
    public void testReceiveNewTilesWithDataChanges_TrackLoad() {
        TileGroup tileGroup = initialiseTileGroup(/* deferLoad: */ true, URLS);

        // Notify the about different URLs, but the same number. #onTileCountChanged() should not be
        // called.
        mMostVisitedSites.setTileSuggestions("http://foo.com", "http://bar.com");
        tileGroup.onSwitchToForeground(/* trackLoadTask: */ true);

        verify(mTileGroupObserver).onTileDataChanged(); // Now data DID change.
        verify(mTileGroupObserver, never()).onTileCountChanged(); // Tile count is still 2.

        // We should now have a pending task.
        verify(mTileGroupDelegate, never()).onLoadingComplete(any());
        assertTrue(tileGroup.isTaskPending(TileGroup.TileTask.SCHEDULE_ICON_FETCH));
    }

    @Test
    @UiThreadTest
    @SmallTest
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
    @UiThreadTest
    @SmallTest
    public void testTileLoadingWhenVisibleNotBlockedForInit() {
        SuggestionsUiDelegate uiDelegate = mSuggestionsUiDelegate;
        when(uiDelegate.isVisible()).thenReturn(true);
        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        uiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        mMostVisitedSites.setTileSuggestions(URLS);

        // Because it's the first load, we accept the incoming tiles and refresh the view.
        verify(mTileGroupObserver).onTileDataChanged();
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testTileLoadingWhenVisibleBlocked() {
        SuggestionsUiDelegate uiDelegate = mSuggestionsUiDelegate;
        when(uiDelegate.isVisible()).thenReturn(true);
        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        uiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
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
    @UiThreadTest
    @SmallTest
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
    @UiThreadTest
    @SmallTest
    // If this flakes again, refer to https://crbug.com/1330627, https://crbug.com/1293208.
    public void testRenderTileView() {
        SuggestionsUiDelegate uiDelegate = mSuggestionsUiDelegate;
        when(uiDelegate.getImageFetcher()).thenReturn(mImageFetcher);
        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        uiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        MostVisitedTilesLayout layout = setupView();

        // Initialise the internal list of tiles
        mMostVisitedSites.setTileSuggestions(URLS);

        // Render them to the layout.
        refreshData(tileGroup, layout);
        assertThat(layout.getChildCount(), is(2));
        assertThat(((SuggestionsTileView) layout.getChildAt(0)).getUrl().getSpec(), is(URLS[0]));
        assertThat(((SuggestionsTileView) layout.getChildAt(1)).getUrl().getSpec(), is(URLS[1]));
    }

    /** Check for https://crbug.com/703628: don't crash on duplicated URLs. */
    @Test
    @UiThreadTest
    @SmallTest
    public void testRenderTileViewWithDuplicatedUrl() {
        SuggestionsUiDelegate uiDelegate = mSuggestionsUiDelegate;
        when(uiDelegate.getImageFetcher()).thenReturn(mMockImageFetcher);
        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        uiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);
        MostVisitedTilesLayout layout = setupView();

        // Initialise the internal list of tiles
        mMostVisitedSites.setTileSuggestions(URLS[0], URLS[1], URLS[0]);

        // Render them to the layout. The duplicated URL should not trigger an exception.
        refreshData(tileGroup, layout);
    }

    @Test
    @UiThreadTest
    @SmallTest
    // If this flakes again, refer to https://crbug.com/1286755.
    public void testRenderTileViewReplacing() {
        SuggestionsUiDelegate uiDelegate = mSuggestionsUiDelegate;
        when(uiDelegate.getImageFetcher()).thenReturn(mMockImageFetcher);
        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        uiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);
        mMostVisitedSites.setTileSuggestions(URLS);

        // Initialise the layout with views whose URLs don't match the ones of the new tiles.
        MostVisitedTilesLayout layout = setupView();
        SuggestionsTileView view1 = mSuggestionsTileView1;
        layout.addView(view1);

        SuggestionsTileView view2 = mSuggestionsTileView2;
        layout.addView(view2);

        // The tiles should be updated, the old ones removed.
        refreshData(tileGroup, layout);
        assertThat(layout.getChildCount(), is(2));
        assertThat(layout.indexOfChild(view1), is(-1));
        assertThat(layout.indexOfChild(view2), is(-1));
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testRenderTileViewRecycling() {
        mMostVisitedSites.setTileSuggestions(URLS);
        List<SiteSuggestion> sites = mMostVisitedSites.getCurrentSites();
        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        mSuggestionsUiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);

        // Initialise the layout with views whose URLs match the ones of the new tiles.
        MostVisitedTilesLayout layout = new MostVisitedTilesLayout(mContext, null);
        SuggestionsTileView view1 = mSuggestionsTileView1;
        when(view1.getData()).thenReturn(sites.get(0));
        layout.addView(view1);

        SuggestionsTileView view2 = mSuggestionsTileView2;
        when(view2.getData()).thenReturn(sites.get(1));
        layout.addView(view2);

        // The tiles should be updated, the old ones reused.
        refreshData(tileGroup);
        assertThat(layout.getChildCount(), is(2));
        assertThat(layout.getChildAt(0), CoreMatchers.is(view1));
        assertThat(layout.getChildAt(1), CoreMatchers.is(view2));
    }

    @Test
    @UiThreadTest
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1330627, https://crbug.com/1293208")
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
    @UiThreadTest
    @SmallTest
    @DisabledTest(message = "Test is flaky, see crbug.com/1288425")
    public void testIconLoadingWhenTileNotRegistered() {
        TileGroup tileGroup = initialiseTileGroup();
        Tile tile = new Tile(createSiteSuggestion("title", URLS[0]), 0);

        ViewGroup layout = new FrameLayout(mContext, null);
        mTileRenderer.buildTileView(tile, layout, tileGroup.getTileSetupDelegate());

        // Ensure we run the callback for the new tile.
        assertEquals(1, mImageFetcher.getPendingIconCallbackCount());
        mImageFetcher.fulfillLargeIconRequests();

        verify(mTileGroupObserver, never()).onTileIconChanged(tile);
    }

    @Test
    @UiThreadTest
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1330627, https://crbug.com/1293208")
    public void testIconLoading_Sync() {
        TileGroup tileGroup = initialiseTileGroup();
        mImageFetcher.fulfillLargeIconRequests();
        reset(mTileGroupObserver, mTileGroupDelegate);

        // Notify for a second set.
        mMostVisitedSites.setTileSuggestions(URLS);
        refreshData(tileGroup);
        mImageFetcher.fulfillLargeIconRequests();

        // Data changed but no loading complete event is sent
        verify(mTileGroupObserver).onTileDataChanged();
        verify(mTileGroupObserver, times(URLS.length)).onTileIconChanged(any(Tile.class));
        verify(mTileGroupDelegate, never()).onLoadingComplete(any());
    }

    @Test
    @UiThreadTest
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1330627, https://crbug.com/1293208")
    public void testIconLoading_AsyncNoTrack() {
        TileGroup tileGroup = initialiseTileGroup(/* deferLoad: */ true);
        mImageFetcher.fulfillLargeIconRequests();
        reset(mTileGroupObserver, mTileGroupDelegate);

        // Notify for a second set.
        mMostVisitedSites.setTileSuggestions(URLS);
        tileGroup.onSwitchToForeground(/* trackLoadTask: */ false);
        refreshData(tileGroup);
        mImageFetcher.fulfillLargeIconRequests();

        // Data changed but no loading complete event is sent (same as sync)
        verify(mTileGroupObserver).onTileDataChanged();
        verify(mTileGroupObserver, times(URLS.length)).onTileIconChanged(any(Tile.class));
        verify(mTileGroupDelegate, never()).onLoadingComplete(any());
    }

    @Test
    @UiThreadTest
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1330627, https://crbug.com/1293208")
    public void testIconLoading_AsyncTrack() {
        TileGroup tileGroup = initialiseTileGroup(/* deferLoad: */ true);
        mImageFetcher.fulfillLargeIconRequests();
        reset(mTileGroupObserver, mTileGroupDelegate);

        // Notify for a second set.
        mMostVisitedSites.setTileSuggestions(URLS);
        tileGroup.onSwitchToForeground(/* trackLoadTask: */ true);
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

    private void refreshData(TileGroup tileGroup) {
        MostVisitedTilesLayout layout = setupView();
        refreshData(tileGroup, layout);
    }

    private void refreshData(TileGroup tileGroup, ViewGroup tilesLayout) {
        assert tileGroup.getTileSections().size() == 1;
        List<Tile> tiles = tileGroup.getTileSections().get(TileSectionType.PERSONALIZED);
        assert tiles != null;
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

        mMostVisitedSites.setTileSuggestions(urls);

        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        mSuggestionsUiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.startObserving(MAX_TILES_TO_FETCH);
        refreshData(tileGroup);

        reset(mTileGroupObserver);
        reset(mTileGroupDelegate);
        return tileGroup;
    }

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
}
