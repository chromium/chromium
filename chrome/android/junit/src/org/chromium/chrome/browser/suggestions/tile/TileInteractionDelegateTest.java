// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.Nullable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.preloading.AndroidPrerenderManager;
import org.chromium.chrome.browser.preloading.AndroidPrerenderManagerJni;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/** Tests for {@link TileInteractionDelegateTest}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_ANDROID_TRIGGER_FOR_PRERENDER2)
public class TileInteractionDelegateTest {

    private static class TileGroupForTest extends TileGroup {
        private Tile mTile;

        public TileGroupForTest(
                TileRenderer tileRenderer,
                SuggestionsUiDelegate uiDelegate,
                ContextMenuManager contextMenuManager,
                Delegate tileGroupDelegate,
                Observer observer,
                OfflinePageBridge offlinePageBridge) {
            super(
                    tileRenderer,
                    uiDelegate,
                    contextMenuManager,
                    tileGroupDelegate,
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

    @Mock Tile mTile;
    @Mock Tile mSearchTile;
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
    @Mock private AndroidPrerenderManager mAndroidPrerenderManager;
    @Mock private AndroidPrerenderManager.Natives mNativeMock;

    @Rule public JniMocker jniMocker = new JniMocker();

    @Captor
    ArgumentCaptor<View.OnTouchListener> mOnTouchListenerCaptor =
            ArgumentCaptor.forClass(View.OnTouchListener.class);

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mTile.getUrl()).thenReturn(new GURL("https://example.com"));
        when(mSearchTile.getUrl()).thenReturn(new GURL("https://www.google.com/search?q=123"));
        when(mTile.getData()).thenReturn(mData);
        when(mAndroidPrerenderManager.startPrerendering(any())).thenReturn(true);
        jniMocker.mock(AndroidPrerenderManagerJni.TEST_HOOKS, mNativeMock);
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
        Mockito.verify(mAndroidPrerenderManager, Mockito.timeout(1000))
                .startPrerendering(ArgumentMatchers.any());
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
    public void testTileInteractionSearchTileNotTriggerPrerendering() {
        AndroidPrerenderManager.setAndroidPrerenderManagerForTesting(mAndroidPrerenderManager);
        TileGroupForTest tileGroup =
                new TileGroupForTest(
                        mTileRenderer,
                        mSuggestionsUiDelegate,
                        mContextMenuManager,
                        mTileGroupDelegate,
                        mTileGroupObserver,
                        mOfflinePageBridge);
        tileGroup.setTileForTesting(mSearchTile);
        tileGroup.onIconMadeAvailable(new GURL("https://www.google.com/search?q=123"));
        TileGroup.TileSetupDelegate tileSetupCallback = tileGroup.getTileSetupDelegate();
        tileSetupCallback.createInteractionDelegate(mSearchTile, mTileView);

        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        verify(mTileView).setOnTouchListener(mOnTouchListenerCaptor.capture());
        mOnTouchListenerCaptor.getValue().onTouch(mTileView, event);
        ShadowLooper.idleMainLooper(200, TimeUnit.MILLISECONDS);
        Mockito.verify(mAndroidPrerenderManager, Mockito.never())
                .startPrerendering(ArgumentMatchers.any());
        AndroidPrerenderManager.clearAndroidPrerenderManagerForTesting();
    }
}
