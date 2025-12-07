// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Build;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.Nullable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.ContextMenuManager.ContextMenuItemId;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.preloading.AndroidPrerenderManager;
import org.chromium.chrome.browser.preloading.AndroidPrerenderManagerJni;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.ui.base.MotionEventTestUtils;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/** Tests for {@link TileInteractionDelegateTest}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TileInteractionDelegateTest {

    private static class TileGroupForTest extends TileGroup {
        private Tile mTile;

        public TileGroupForTest(
                TileRenderer tileRenderer,
                SuggestionsUiDelegate uiDelegate,
                ContextMenuManager contextMenuManager,
                Delegate tileGroupDelegate,
                TileDragDelegate tileDragDelegate,
                Observer observer,
                OfflinePageBridge offlinePageBridge) {
            super(
                    tileRenderer,
                    uiDelegate,
                    contextMenuManager,
                    tileGroupDelegate,
                    tileDragDelegate,
                    observer,
                    offlinePageBridge);
        }

        public void setTileForTesting(Tile tile) {
            mTile = tile;
        }

        @Override
        protected @Nullable Tile findTile(SiteSuggestion suggestion) {
            return mTile;
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Tile mTile;
    @Mock private SuggestionsTileView mTileView;
    @Mock private SiteSuggestion mData;
    @Mock private SuggestionsUiDelegate mSuggestionsUiDelegate;
    @Mock private ContextMenuManager mContextMenuManager;
    @Mock private TileGroup.Delegate mTileGroupDelegate;
    @Mock private TileDragDelegate mTileDragDelegate;
    @Mock private OfflinePageBridge mOfflinePageBridge;
    @Mock private TileGroup.Observer mTileGroupObserver;
    @Mock private TileRenderer mTileRenderer;
    @Mock private AndroidPrerenderManager mAndroidPrerenderManager;
    @Mock private AndroidPrerenderManager.Natives mNativeMock;
    @Mock private TileGroup.CustomTileModificationDelegate mCustomTileModificationDelegate;

    @Captor
    ArgumentCaptor<View.OnTouchListener> mOnTouchListenerCaptor =
            ArgumentCaptor.forClass(View.OnTouchListener.class);

    private TileInteractionDelegateImpl mDelegate;

    @Before
    public void setUp() {
        when(mTile.getUrl()).thenReturn(new GURL("https://example.com"));
        when(mTile.getData()).thenReturn(mData);
        AndroidPrerenderManagerJni.setInstanceForTesting(mNativeMock);
    }

    private void setupForCustomTileTests() {
        when(mTile.getSource()).thenReturn(TileSource.CUSTOM_LINKS);
        when(mTile.getSectionType()).thenReturn(TileSectionType.PERSONALIZED);
        mDelegate =
                new TileInteractionDelegateImpl(
                        mock(ContextMenuManager.class),
                        mTileGroupDelegate,
                        mTileDragDelegate,
                        mCustomTileModificationDelegate,
                        mTile,
                        mTileView);
    }

    @Test
    public void testTileInteractionDelegateTaken() {
        HistogramWatcher.Builder histogramWatcherBuilder = HistogramWatcher.newBuilder();
        HistogramWatcher histogramWatcher = histogramWatcherBuilder.build();

        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        mSuggestionsUiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileDragDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.onIconMadeAvailable(new GURL("https://example.com"));
        TileGroup.TileSetupDelegate tileSetupCallback = tileGroup.getTileSetupDelegate();
        TileGroup.TileInteractionDelegate tileInteractionDelegate =
                tileSetupCallback.createInteractionDelegate(mTile, mTileView);

        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        verify(mTileView).setOnTouchListener(mOnTouchListenerCaptor.capture());
        mOnTouchListenerCaptor.getValue().onTouch(mTileView, event);
        tileInteractionDelegate.onClick(mTileView);

        histogramWatcher.assertExpected();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R, manifest = Config.NONE)
    public void testTileInteractionDelegate_longClick() {
        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        mSuggestionsUiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileDragDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.onIconMadeAvailable(new GURL("https://example.com"));
        TileGroup.TileSetupDelegate tileSetupCallback = tileGroup.getTileSetupDelegate();
        TileGroup.TileInteractionDelegate tileInteractionDelegate =
                tileSetupCallback.createInteractionDelegate(mTile, mTileView);
        // Verify long click event shows menu.
        tileInteractionDelegate.onLongClick(mTileView);
        verify(mContextMenuManager).showListContextMenu(eq(mTileView), any());

        // Verify secondary click event is handled as long click.
        when(mTileView.hasOnLongClickListeners()).thenReturn(true);
        MotionEvent secondaryClickEvent = MotionEventTestUtils.getTrackRightClickEvent();
        tileInteractionDelegate.onGenericMotion(mTileView, secondaryClickEvent);
        verify(mTileView).performLongClick();
        verify(mContextMenuManager).showListContextMenu(eq(mTileView), any());
    }

    @Test
    public void testTileInteractionDelegateNotTaken() {
        HistogramWatcher.Builder histogramWatcherBuilder = HistogramWatcher.newBuilder();
        HistogramWatcher histogramWatcher = histogramWatcherBuilder.build();

        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        mSuggestionsUiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileDragDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.onIconMadeAvailable(new GURL("https://example.com"));
        TileGroup.TileSetupDelegate tileSetupCallback = tileGroup.getTileSetupDelegate();
        tileSetupCallback.createInteractionDelegate(mTile, mTileView);
        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        MotionEvent cancelEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_CANCEL, 0, 0, 0);
        verify(mTileView).setOnTouchListener(mOnTouchListenerCaptor.capture());
        mOnTouchListenerCaptor.getValue().onTouch(mTileView, event);
        mOnTouchListenerCaptor.getValue().onTouch(mTileView, cancelEvent);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testTileInteractionTriggerPrerendering() {
        AndroidPrerenderManager.setAndroidPrerenderManagerForTesting(mAndroidPrerenderManager);
        TileGroupForTest tileGroup =
                new TileGroupForTest(
                        mTileRenderer,
                        mSuggestionsUiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileDragDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.setTileForTesting(mTile);
        tileGroup.onIconMadeAvailable(new GURL("https://example.com"));
        TileGroup.TileSetupDelegate tileSetupCallback = tileGroup.getTileSetupDelegate();
        tileSetupCallback.createInteractionDelegate(mTile, mTileView);

        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        MotionEvent cancelEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_CANCEL, 0, 0, 0);
        verify(mTileView).setOnTouchListener(mOnTouchListenerCaptor.capture());
        mOnTouchListenerCaptor.getValue().onTouch(mTileView, event);
        ShadowLooper.idleMainLooper(200, TimeUnit.MILLISECONDS);
        Mockito.verify(mAndroidPrerenderManager, Mockito.timeout(1000)).startPrerendering(any());
        // The second onTouch with the same tile should be considered to be duplicate and should be
        // skipped by maybePrerender, and this should not cause any error.
        mOnTouchListenerCaptor.getValue().onTouch(mTileView, event);

        mOnTouchListenerCaptor.getValue().onTouch(mTileView, cancelEvent);
        // mPrerenderStarted in TileInteractionDelegateImpl is true, stopPrerendering should be
        // called.
        Mockito.verify(mAndroidPrerenderManager).stopPrerendering();
        AndroidPrerenderManager.clearAndroidPrerenderManagerForTesting();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION})
    public void testIsItemSupported_MoveUp_FirstTile() {
        setupForCustomTileTests();
        when(mTileDragDelegate.isFirstDraggableTile(mTileView)).thenReturn(true);
        assertFalse(mDelegate.isItemSupported(ContextMenuItemId.MOVE_UP));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION})
    public void testIsItemSupported_MoveUp_NotFirstTile() {
        setupForCustomTileTests();
        when(mTileDragDelegate.isFirstDraggableTile(mTileView)).thenReturn(false);
        assertTrue(mDelegate.isItemSupported(ContextMenuItemId.MOVE_UP));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION})
    public void testIsItemSupported_MoveDown_LastTile() {
        setupForCustomTileTests();
        when(mTileDragDelegate.isLastDraggableTile(mTileView)).thenReturn(true);
        assertFalse(mDelegate.isItemSupported(ContextMenuItemId.MOVE_DOWN));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION})
    public void testIsItemSupported_MoveDown_NotLastTile() {
        setupForCustomTileTests();
        when(mTileDragDelegate.isLastDraggableTile(mTileView)).thenReturn(false);
        assertTrue(mDelegate.isItemSupported(ContextMenuItemId.MOVE_DOWN));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION})
    public void testMoveItemUp() {
        setupForCustomTileTests();
        mDelegate.moveItemUp();
        verify(mTileDragDelegate).swapTiles(mTileView, -1, mDelegate);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION})
    public void testMoveItemDown() {
        setupForCustomTileTests();
        mDelegate.moveItemDown();
        verify(mTileDragDelegate).swapTiles(mTileView, 1, mDelegate);
    }
}
