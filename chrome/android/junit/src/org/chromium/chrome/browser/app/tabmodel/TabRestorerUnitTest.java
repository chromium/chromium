// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.tabmodel.TabRestorer.TabRestorerDelegate;
import org.chromium.chrome.browser.tab.ScopedStorageBatch;
import org.chromium.chrome.browser.tab.StorageLoadedData;
import org.chromium.chrome.browser.tab.StorageLoadedData.LoadedTabState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

import java.nio.ByteBuffer;
import java.util.Collections;

/** Unit tests for {@link TabRestorer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabRestorerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabRestorerDelegate mDelegate;
    @Mock private TabCreator mTabCreator;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private StorageLoadedData mStorageLoadedData;
    @Mock private ScopedStorageBatch mBatch;

    private TabRestorer mRestorer;

    @Before
    public void setUp() {
        when(mTabModelSelector.getModel(anyBoolean())).thenReturn(mTabModel);
        when(mTabModel.iterator()).thenReturn(Collections.emptyIterator());

        mRestorer =
                new TabRestorer(
                        /* incognito= */ false,
                        mDelegate,
                        mTabCreator,
                        () -> mBatch,
                        mTabModelSelector,
                        /* isFromRecreating= */ false);
    }

    @Test
    public void testOnDataLoaded() {
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(new LoadedTabState[0]);
        mRestorer.onDataLoaded(mStorageLoadedData);
        verify(mDelegate).onDataLoaded(eq(false), eq(0));
    }

    @Test
    public void testStartNoTabs() {
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(new LoadedTabState[0]);
        mRestorer.onDataLoaded(mStorageLoadedData);
        mRestorer.start(/* restoreActiveTabImmediately= */ true);

        verify(mDelegate).onFinished(eq(false));
    }

    @Test
    public void testStartWithTabsBatchRestore() {
        LoadedTabState[] states = new LoadedTabState[2];
        states[0] = createLoadedTabState(1, UrlConstants.GOOGLE_URL);
        states[1] = createLoadedTabState(2, UrlConstants.CHROME_DINO_URL);
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(states);
        when(mStorageLoadedData.getActiveTabIndex()).thenReturn(0);

        mRestorer.onDataLoaded(mStorageLoadedData);
        mRestorer.start(/* restoreActiveTabImmediately= */ false);

        // Batch restore is posted.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabCreator, times(2)).createFrozenTab(any(), anyInt(), anyInt());
        verify(mDelegate).onFinished(eq(false));
    }

    @Test
    public void testStartWithActiveTabImmediately() {
        LoadedTabState[] states = new LoadedTabState[1];
        states[0] = createLoadedTabState(1, UrlConstants.GOOGLE_URL);
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(states);
        when(mStorageLoadedData.getActiveTabIndex()).thenReturn(0);
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(1);
        when(tab.getUrl()).thenReturn(new GURL(UrlConstants.GOOGLE_URL));
        when(mTabCreator.createFrozenTab(any(), eq(1), eq(0))).thenReturn(tab);

        mRestorer.onDataLoaded(mStorageLoadedData);
        mRestorer.start(/* restoreActiveTabImmediately= */ true);

        // Active tab should be restored synchronously.
        verify(mTabCreator).createFrozenTab(any(), eq(1), eq(0));
        verify(mDelegate).onActiveTabRestored(eq(false));

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mDelegate).onFinished(eq(false));
    }

    @Test
    public void testCancelBeforeLoaded() {
        mRestorer.cancel();
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(new LoadedTabState[0]);
        mRestorer.onDataLoaded(mStorageLoadedData);

        verify(mDelegate).onCancelled(eq(false));
        verify(mStorageLoadedData).destroy();
    }

    @Test
    public void testCancelAfterLoaded() {
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(new LoadedTabState[0]);
        mRestorer.onDataLoaded(mStorageLoadedData);
        mRestorer.cancel();

        verify(mDelegate).onCancelled(eq(false));
        verify(mStorageLoadedData).destroy();
    }

    @Test
    public void testRestoreTabStateForId() {
        LoadedTabState[] states = new LoadedTabState[2];
        states[0] = createLoadedTabState(1, UrlConstants.GOOGLE_URL);
        states[1] = createLoadedTabState(2, UrlConstants.CHROME_DINO_URL);
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(states);
        when(mStorageLoadedData.getActiveTabIndex()).thenReturn(0);
        Tab tab = mock(Tab.class);
        when(tab.getUrl()).thenReturn(new GURL(UrlConstants.CHROME_DINO_URL));
        when(mTabCreator.createFrozenTab(any(), eq(2), eq(1))).thenReturn(tab);

        mRestorer.onDataLoaded(mStorageLoadedData);
        mRestorer.start(/* restoreActiveTabImmediately= */ false);

        assertTrue(mRestorer.restoreTabStateForId(2));
        verify(mTabCreator).createFrozenTab(any(), eq(2), eq(1));

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mTabCreator, times(1)).createFrozenTab(any(), eq(2), anyInt());
    }

    @Test
    public void testRestoreTabStateForUrl() {
        LoadedTabState[] states = new LoadedTabState[2];
        states[0] = createLoadedTabState(1, UrlConstants.GOOGLE_URL);
        states[1] = createLoadedTabState(2, UrlConstants.CHROME_DINO_URL);
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(states);
        when(mStorageLoadedData.getActiveTabIndex()).thenReturn(0);
        Tab tab = mock(Tab.class);
        when(tab.getUrl()).thenReturn(new GURL(UrlConstants.CHROME_DINO_URL));
        when(mTabCreator.createFrozenTab(any(), eq(2), eq(1))).thenReturn(tab);

        mRestorer.onDataLoaded(mStorageLoadedData);
        mRestorer.start(/* restoreActiveTabImmediately= */ false);

        assertTrue(mRestorer.restoreTabStateForUrl(UrlConstants.CHROME_DINO_URL));
        verify(mTabCreator).createFrozenTab(any(), eq(2), eq(1));
    }

    @Test
    public void testOnCachedActiveTabLoaded() {
        LoadedTabState state = createLoadedTabState(1, UrlConstants.GOOGLE_URL);
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(1);
        when(tab.getUrl()).thenReturn(new GURL(UrlConstants.GOOGLE_URL));
        when(mTabCreator.createFrozenTab(any(), eq(1), eq(0))).thenReturn(tab);
        when(mTabModel.getCount()).thenReturn(0);

        mRestorer.onCachedActiveTabLoaded(state);

        verify(mTabCreator).createFrozenTab(any(), eq(1), eq(0));
        verify(mDelegate).onActiveTabRestored(eq(false));
    }

    @Test
    public void testMaybeRestoreTab_isReparenting() {
        LoadedTabState state = createLoadedTabState(1, UrlConstants.GOOGLE_URL);
        when(mTabCreator.isReparenting(eq(1))).thenReturn(true);

        mRestorer.onCachedActiveTabLoaded(state);

        // createFrozenTab is called because isReparenting is true.
        verify(mTabCreator).createFrozenTab(any(), eq(1), anyInt());
        verify(mDelegate, never()).onActiveTabRestored(anyBoolean());

        // WebContentsState from TabState is destroyed because we use the reparented tab instead.
        verify(state.tabState.contentsState).destroy();
    }

    @Test
    public void testStartWithActiveTabImmediately_Reparenting() {
        LoadedTabState[] states = new LoadedTabState[1];
        states[0] = createLoadedTabState(1, UrlConstants.GOOGLE_URL);
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(states);
        when(mStorageLoadedData.getActiveTabIndex()).thenReturn(0);
        when(mTabCreator.isReparenting(eq(1))).thenReturn(true);

        mRestorer.onDataLoaded(mStorageLoadedData);
        mRestorer.start(/* restoreActiveTabImmediately= */ true);

        // Active tab should be restored synchronously.
        verify(mTabCreator).createFrozenTab(any(), eq(1), eq(0));
        verify(mDelegate, never()).onActiveTabRestored(anyBoolean());
        verify(states[0].tabState.contentsState).destroy();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mDelegate).onFinished(eq(false));
    }

    @Test
    public void testRestoreTab_Reparenting() {
        LoadedTabState[] states = new LoadedTabState[1];
        states[0] = createLoadedTabState(1, UrlConstants.GOOGLE_URL);
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(states);
        when(mStorageLoadedData.getActiveTabIndex()).thenReturn(-1);
        when(mTabCreator.isReparenting(eq(1))).thenReturn(true);

        mRestorer.onDataLoaded(mStorageLoadedData);
        mRestorer.start(/* restoreActiveTabImmediately= */ false);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabCreator).createFrozenTab(any(), eq(1), eq(0));

        // onDetailsRead should NOT be called because maybeRestoreTab returns null.
        verify(mDelegate, never())
                .onDetailsRead(
                        anyInt(),
                        anyInt(),
                        any(),
                        anyBoolean(),
                        anyBoolean(),
                        anyBoolean(),
                        anyBoolean());
        verify(states[0].tabState.contentsState).destroy();
        verify(mDelegate).onFinished(eq(false));
    }

    @Test
    public void testMaybeRestoreTab_noContentsState() {
        TabState tabState = new TabState();
        tabState.url = new GURL(UrlConstants.GOOGLE_URL);
        LoadedTabState state = new LoadedTabState(1, tabState);

        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(1);
        when(mTabCreator.createNewTab(any(), anyInt(), any(), anyInt())).thenReturn(tab);

        mRestorer.onCachedActiveTabLoaded(state);

        verify(mTabCreator).createNewTab(any(), anyInt(), any(), anyInt());
        verify(mDelegate).onActiveTabRestored(eq(false));
    }

    @Test
    public void testMaybeRestoreTab_skippedTab() {
        TabState tabState = new TabState();
        tabState.url = null;

        LoadedTabState[] states = new LoadedTabState[] {new LoadedTabState(1, tabState)};
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(states);
        when(mStorageLoadedData.getActiveTabIndex()).thenReturn(-1);

        mRestorer.onDataLoaded(mStorageLoadedData);
        mRestorer.start(/* restoreActiveTabImmediately= */ false);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabCreator, never()).createFrozenTab(any(), anyInt(), anyInt());
        verify(mTabCreator, never()).createNewTab(any(), anyInt(), any(), anyInt());
    }

    @Test
    public void testMaybeRestoreTab_isRecreatingNtp() {
        mRestorer =
                new TabRestorer(
                        /* incognito= */ false,
                        mDelegate,
                        mTabCreator,
                        () -> mBatch,
                        mTabModelSelector,
                        /* isFromRecreating= */ true);

        TabState tabState = new TabState();
        tabState.url = new GURL(UrlConstants.NTP_URL);
        LoadedTabState[] states = new LoadedTabState[] {new LoadedTabState(1, tabState)};
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(states);
        when(mStorageLoadedData.getActiveTabIndex()).thenReturn(-1);

        mRestorer.onDataLoaded(mStorageLoadedData);
        mRestorer.start(/* restoreActiveTabImmediately= */ false);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabCreator).createNewTab(any(), anyInt(), any(), anyInt());
    }

    @Test
    public void testMaybeRestoreTab_noContentsState_NonActiveTab() {
        TabState tabState = new TabState();
        tabState.url = new GURL(UrlConstants.GOOGLE_URL);

        LoadedTabState[] states = new LoadedTabState[] {new LoadedTabState(1, tabState)};
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(states);
        when(mStorageLoadedData.getActiveTabIndex()).thenReturn(-1);

        mRestorer.onDataLoaded(mStorageLoadedData);
        mRestorer.start(/* restoreActiveTabImmediately= */ false);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Non-active tab without contentsState is restored as fallback GURL.
        verify(mTabCreator).createNewTab(any(), anyInt(), any(), anyInt());
        verify(mTabCreator, never()).createFrozenTab(any(), anyInt(), anyInt());
    }

    @Test
    public void testMaybeRestoreTab_emptyBuffer() {
        TabState tabState = new TabState();
        tabState.url = new GURL(UrlConstants.GOOGLE_URL);
        tabState.contentsState = mock(WebContentsState.class);
        when(tabState.contentsState.getVirtualUrlFromState()).thenReturn(UrlConstants.GOOGLE_URL);
        ByteBuffer buffer = ByteBuffer.allocate(0);
        when(tabState.contentsState.buffer()).thenReturn(buffer);

        LoadedTabState[] states = new LoadedTabState[] {new LoadedTabState(1, tabState)};
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(states);
        when(mStorageLoadedData.getActiveTabIndex()).thenReturn(-1);

        mRestorer.onDataLoaded(mStorageLoadedData);
        mRestorer.start(/* restoreActiveTabImmediately= */ false);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Empty contentsState buffer limit == 0 falls back to createTabWithoutContentsState.
        verify(mTabCreator).createNewTab(any(), anyInt(), any(), anyInt());
        verify(mTabCreator, never()).createFrozenTab(any(), anyInt(), anyInt());
    }

    private LoadedTabState createLoadedTabState(int id, String url) {
        TabState tabState = new TabState();
        tabState.url = new GURL(url);
        tabState.contentsState = mock(WebContentsState.class);
        when(tabState.contentsState.getVirtualUrlFromState()).thenReturn(url);
        ByteBuffer buffer = ByteBuffer.allocate(1);
        when(tabState.contentsState.buffer()).thenReturn(buffer);
        return new LoadedTabState(id, tabState);
    }
}
