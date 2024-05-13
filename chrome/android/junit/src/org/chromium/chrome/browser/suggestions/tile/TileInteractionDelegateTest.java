// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.mockito.Mockito.verify;

import android.view.MotionEvent;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.url.GURL;

/** Tests for {@link TileInteractionDelegateTest}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TileInteractionDelegateTest {
    @Mock Tile mTile;
    @Mock SuggestionsTileView mTileView;
    @Mock SiteSuggestion mData;
    @Mock SuggestionsUiDelegate mSuggestionsUiDelegate;
    @Mock ContextMenuManager mContextMenuManager;
    @Mock TileGroup.Delegate mTileGroupDelegate;
    @Mock OfflinePageBridge mOfflinePageBridge;
    @Mock private Runnable mSnapshotTileGridChangedRunnable;
    @Mock private Runnable mTileCountChangedRunnable;
    @Mock private TileGroup.Observer mTileGroupObserver;
    @Mock private TileRenderer mTileRenderer;

    @Captor
    ArgumentCaptor<View.OnTouchListener> mOnTouchListenerCaptor =
            ArgumentCaptor.forClass(View.OnTouchListener.class);

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testTileInteractionDelegateTaken() {
        HistogramWatcher.Builder histogramWatcherBuilder = HistogramWatcher.newBuilder();

        histogramWatcherBuilder.expectIntRecord(
                "Prerender.Experimental.NewTabPage.TouchDuration.Taken", 0);
        histogramWatcherBuilder.expectNoRecords(
                "Prerender.Experimental.NewTabPage.TouchDuration.NotTaken");

        HistogramWatcher histogramWatcher = histogramWatcherBuilder.build();

        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        mSuggestionsUiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.onIconMadeAvailable(new GURL("https://foo.com"));
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
    public void testTileInteractionDelegateNotTaken() {
        HistogramWatcher.Builder histogramWatcherBuilder = HistogramWatcher.newBuilder();

        histogramWatcherBuilder.expectIntRecord(
                "Prerender.Experimental.NewTabPage.TouchDuration.NotTaken", 0);
        histogramWatcherBuilder.expectNoRecords(
                "Prerender.Experimental.NewTabPage.TouchDuration.Taken");

        HistogramWatcher histogramWatcher = histogramWatcherBuilder.build();

        TileGroup tileGroup =
                new TileGroup(
                        mTileRenderer,
                        mSuggestionsUiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.onIconMadeAvailable(new GURL("https://foo.com"));
        TileGroup.TileSetupDelegate tileSetupCallback = tileGroup.getTileSetupDelegate();
        tileSetupCallback.createInteractionDelegate(mTile, mTileView);
        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        MotionEvent cancelEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_CANCEL, 0, 0, 0);
        verify(mTileView).setOnTouchListener(mOnTouchListenerCaptor.capture());
        mOnTouchListenerCaptor.getValue().onTouch(mTileView, event);
        mOnTouchListenerCaptor.getValue().onTouch(mTileView, cancelEvent);

        histogramWatcher.assertExpected();
    }
}
