// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertNull;
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

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.actor.BackgroundPoolTab;
import org.chromium.chrome.browser.actor.BackgroundTabPool;
import org.chromium.chrome.browser.actor.BackgroundTabPoolManager;
import org.chromium.chrome.browser.app.tabmodel.TabRestorer.TabRestorerDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.ScopedStorageBatch;
import org.chromium.chrome.browser.tab.StorageLoadedData;
import org.chromium.chrome.browser.tab.StorageLoadedData.LoadedTabState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabOrchestratorType;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

import java.nio.ByteBuffer;
import java.util.Collections;
import java.util.Set;

/** Unit tests for {@link TabRestorer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabRestorerUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock TabRestorerDelegate mDelegate;
    private @Mock TabCreator mTabCreator;
    private @Mock TabModelSelector mTabModelSelector;
    private @Mock TabModel mTabModel;
    private @Mock StorageLoadedData mStorageLoadedData;
    private @Mock ScopedStorageBatch mBatch;
    private @Mock Profile mProfile;
    private @Mock BackgroundTabPool mBackgroundTabPool;
    private @Mock BackgroundPoolTab mBackgroundPoolTab;

    private TabRestorer mRestorer;

    @Before
    public void setUp() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        when(mTabModelSelector.getModel(anyBoolean())).thenReturn(mTabModel);
        when(mTabModel.iterator()).thenReturn(Collections.emptyIterator());
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mProfile.isOffTheRecord()).thenReturn(false);

        mRestorer =
                new TabRestorer(
                        TabOrchestratorType.TABBED,
                        /* incognito= */ false,
                        mDelegate,
                        mTabCreator,
                        () -> mBatch,
                        mTabModelSelector,
                        /* isFromRecreating= */ false);
    }

    @After
    public void tearDown() {
        BackgroundTabPoolManager.resetForTesting();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testRestoreTab_interceptedByBackgroundTabPool() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        when(mBackgroundTabPool.getAllPlaceholderTabIds()).thenReturn(Set.of(1));
        when(mBackgroundTabPool.loadTab(1)).thenReturn(mBackgroundPoolTab);

        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(1);
        when(tab.getUrl()).thenReturn(new GURL(UrlConstants.GOOGLE_URL));
        when(mBackgroundPoolTab.attachTab(eq(mTabModel), eq(0))).thenReturn(tab);
        when(mTabModel.indexOf(tab)).thenReturn(0);

        LoadedTabState state = createLoadedTabState(1, UrlConstants.GOOGLE_URL);
        WebContentsState contentsState = state.tabState.contentsState;
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(new LoadedTabState[] {state});
        when(mStorageLoadedData.getActiveTabIndex()).thenReturn(0);

        mRestorer.onDataLoaded(mStorageLoadedData);
        mRestorer.start(/* restoreActiveTabImmediately= */ true);

        verify(mBackgroundTabPool).getAllPlaceholderTabIds();
        verify(mBackgroundTabPool).loadTab(1);
        verify(mBackgroundPoolTab).attachTab(eq(mTabModel), eq(0));
        verify(contentsState).destroy();
        verify(mTabCreator, never()).createFrozenTab(any(), anyInt(), anyInt());
        verify(mTabCreator, never()).createNewTab(any(), anyInt(), any(), anyInt());
        assertTrue(state.isClaimedOrDestroyed());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testRestoreTab_backgroundTabPoolFeatureDisabled() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);

        LoadedTabState state = createLoadedTabState(1, UrlConstants.GOOGLE_URL);
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(new LoadedTabState[] {state});
        when(mStorageLoadedData.getActiveTabIndex()).thenReturn(0);
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(1);
        when(tab.getUrl()).thenReturn(new GURL(UrlConstants.GOOGLE_URL));
        when(mTabCreator.createFrozenTab(any(), eq(1), eq(0))).thenReturn(tab);
        when(mTabModel.indexOf(tab)).thenReturn(0);

        mRestorer.onDataLoaded(mStorageLoadedData);
        mRestorer.start(/* restoreActiveTabImmediately= */ true);

        verify(mBackgroundTabPool, never()).getAllPlaceholderTabIds();
        verify(mBackgroundTabPool, never()).loadTab(anyInt());
        verify(mTabCreator).createFrozenTab(any(), eq(1), eq(0));
        assertTrue(state.isClaimedOrDestroyed());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testRestoreTab_incognito_skipsBackgroundTabPool() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        TabRestorer incognitoRestorer =
                new TabRestorer(
                        TabOrchestratorType.TABBED,
                        /* incognito= */ true,
                        mDelegate,
                        mTabCreator,
                        () -> mBatch,
                        mTabModelSelector,
                        /* isFromRecreating= */ false);

        LoadedTabState state = createLoadedTabState(1, UrlConstants.GOOGLE_URL);
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(new LoadedTabState[] {state});
        when(mStorageLoadedData.getActiveTabIndex()).thenReturn(0);
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(1);
        when(tab.getUrl()).thenReturn(new GURL(UrlConstants.GOOGLE_URL));
        when(mTabCreator.createFrozenTab(any(), eq(1), eq(0))).thenReturn(tab);
        when(mTabModel.indexOf(tab)).thenReturn(0);

        incognitoRestorer.onDataLoaded(mStorageLoadedData);
        incognitoRestorer.start(/* restoreActiveTabImmediately= */ true);

        verify(mBackgroundTabPool, never()).getAllPlaceholderTabIds();
        verify(mBackgroundTabPool, never()).loadTab(anyInt());
        verify(mTabCreator).createFrozenTab(any(), eq(1), eq(0));
        assertTrue(state.isClaimedOrDestroyed());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testRestoreTab_nonTabbedOrchestrator_skipsBackgroundTabPool() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        TabRestorer nonTabbedRestorer =
                new TabRestorer(
                        TabOrchestratorType.CUSTOM,
                        /* incognito= */ false,
                        mDelegate,
                        mTabCreator,
                        () -> mBatch,
                        mTabModelSelector,
                        /* isFromRecreating= */ false);
        when(mBackgroundTabPool.getAllPlaceholderTabIds()).thenReturn(Set.of(1));

        LoadedTabState state = createLoadedTabState(1, UrlConstants.GOOGLE_URL);
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(new LoadedTabState[] {state});
        when(mStorageLoadedData.getActiveTabIndex()).thenReturn(0);
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(1);
        when(tab.getUrl()).thenReturn(new GURL(UrlConstants.GOOGLE_URL));
        when(mTabCreator.createFrozenTab(any(), eq(1), eq(0))).thenReturn(tab);
        when(mTabModel.indexOf(tab)).thenReturn(0);

        nonTabbedRestorer.onDataLoaded(mStorageLoadedData);
        nonTabbedRestorer.start(/* restoreActiveTabImmediately= */ true);

        verify(mBackgroundTabPool, never()).getAllPlaceholderTabIds();
        verify(mBackgroundTabPool, never()).loadTab(anyInt());
        verify(mTabCreator).createFrozenTab(any(), eq(1), eq(0));
        assertTrue(state.isClaimedOrDestroyed());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testRestoreTab_poolLoadFailure_fallsBackToCreateFrozenTab() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        when(mBackgroundTabPool.getAllPlaceholderTabIds()).thenReturn(Set.of(1));
        when(mBackgroundTabPool.loadTab(1)).thenReturn(null);

        LoadedTabState state = createLoadedTabState(1, UrlConstants.GOOGLE_URL);
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(new LoadedTabState[] {state});
        when(mStorageLoadedData.getActiveTabIndex()).thenReturn(0);
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(1);
        when(tab.getUrl()).thenReturn(new GURL(UrlConstants.GOOGLE_URL));
        when(mTabCreator.createFrozenTab(any(), eq(1), eq(0))).thenReturn(tab);
        when(mTabModel.indexOf(tab)).thenReturn(0);

        mRestorer.onDataLoaded(mStorageLoadedData);
        mRestorer.start(/* restoreActiveTabImmediately= */ true);

        verify(mBackgroundTabPool).getAllPlaceholderTabIds();
        verify(mBackgroundTabPool).loadTab(1);
        verify(mTabCreator).createFrozenTab(any(), eq(1), eq(0));
        assertTrue(state.isClaimedOrDestroyed());
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
        WebContentsState contentsState = state.tabState.contentsState;
        when(mTabCreator.isReparenting(eq(1))).thenReturn(true);

        mRestorer.onCachedActiveTabLoaded(state);

        // createFrozenTab is called because isReparenting is true.
        verify(mTabCreator).createFrozenTab(any(), eq(1), anyInt());
        verify(mDelegate, never()).onActiveTabRestored(anyBoolean());

        // WebContentsState from TabState is destroyed because we use the reparented tab instead.
        verify(contentsState).destroy();
        assertNull(state.tabState.contentsState);
        assertTrue(state.isClaimedOrDestroyed());
    }

    @Test
    public void testStartWithActiveTabImmediately_Reparenting() {
        LoadedTabState[] states = new LoadedTabState[1];
        states[0] = createLoadedTabState(1, UrlConstants.GOOGLE_URL);
        WebContentsState contentsState = states[0].tabState.contentsState;
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(states);
        when(mStorageLoadedData.getActiveTabIndex()).thenReturn(0);
        when(mTabCreator.isReparenting(eq(1))).thenReturn(true);

        mRestorer.onDataLoaded(mStorageLoadedData);
        mRestorer.start(/* restoreActiveTabImmediately= */ true);

        // Active tab should be restored synchronously.
        verify(mTabCreator).createFrozenTab(any(), eq(1), eq(0));
        verify(mDelegate, never()).onActiveTabRestored(anyBoolean());
        verify(contentsState).destroy();
        assertNull(states[0].tabState.contentsState);
        assertTrue(states[0].isClaimedOrDestroyed());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mDelegate).onFinished(eq(false));
    }

    @Test
    public void testRestoreTab_Reparenting() {
        LoadedTabState[] states = new LoadedTabState[1];
        states[0] = createLoadedTabState(1, UrlConstants.GOOGLE_URL);
        WebContentsState contentsState = states[0].tabState.contentsState;
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
        verify(contentsState).destroy();
        assertNull(states[0].tabState.contentsState);
        assertTrue(states[0].isClaimedOrDestroyed());
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
                        TabOrchestratorType.TABBED,
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

    @Test
    public void testRestoreTab_ClaimsLoadedTabState() {
        LoadedTabState state = createLoadedTabState(1, UrlConstants.GOOGLE_URL);
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(new LoadedTabState[] {state});
        when(mStorageLoadedData.getActiveTabIndex()).thenReturn(0);
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(1);
        when(tab.getUrl()).thenReturn(new GURL(UrlConstants.GOOGLE_URL));
        when(mTabCreator.createFrozenTab(any(), eq(1), eq(0))).thenReturn(tab);

        mRestorer.onDataLoaded(mStorageLoadedData);
        mRestorer.start(/* restoreActiveTabImmediately= */ true);

        assertTrue(state.isClaimedOrDestroyed());
    }

    @Test
    public void testRestoreTab_SkippedTabDestroysContentsState() {
        TabState tabState = new TabState();
        tabState.url = new GURL(UrlConstants.GOOGLE_URL);
        WebContentsState contentsState = mock(WebContentsState.class);
        ByteBuffer emptyBuffer = ByteBuffer.allocate(0);
        when(contentsState.buffer()).thenReturn(emptyBuffer);
        when(contentsState.getVirtualUrlFromState()).thenReturn(UrlConstants.GOOGLE_URL);
        tabState.contentsState = contentsState;
        LoadedTabState state = new LoadedTabState(1, tabState);

        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(new LoadedTabState[] {state});
        when(mStorageLoadedData.getActiveTabIndex()).thenReturn(-1);

        mRestorer.onDataLoaded(mStorageLoadedData);
        mRestorer.start(/* restoreActiveTabImmediately= */ false);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(contentsState).destroy();
        assertNull(tabState.contentsState);
        assertTrue(state.isClaimedOrDestroyed());
    }

    @Test
    public void testOnCachedActiveTabLoaded_lateArrivingAfterLoaded() {
        when(mStorageLoadedData.getLoadedTabStates()).thenReturn(new LoadedTabState[0]);
        mRestorer.onDataLoaded(mStorageLoadedData);

        LoadedTabState cachedState = createLoadedTabState(1, UrlConstants.GOOGLE_URL);
        WebContentsState contentsState = cachedState.tabState.contentsState;
        mRestorer.onCachedActiveTabLoaded(cachedState);

        verify(contentsState).destroy();
        assertNull(cachedState.tabState.contentsState);
        assertTrue(cachedState.isClaimedOrDestroyed());
        verify(mTabCreator, never()).createFrozenTab(any(), anyInt(), anyInt());
        verify(mDelegate, never()).onActiveTabRestored(anyBoolean());
    }

    @Test
    public void testRestoreTab_CachedActiveTabWithNegativeActiveTabIndex() {
        LoadedTabState cachedState = createLoadedTabState(1, UrlConstants.GOOGLE_URL);
        mRestorer.onCachedActiveTabLoaded(cachedState);

        LoadedTabState dbState1 = createLoadedTabState(1, UrlConstants.GOOGLE_URL);
        LoadedTabState dbState2 = createLoadedTabState(2, UrlConstants.GOOGLE_URL);
        when(mStorageLoadedData.getLoadedTabStates())
                .thenReturn(new LoadedTabState[] {dbState1, dbState2});
        when(mStorageLoadedData.getActiveTabIndex()).thenReturn(-1);

        Tab tab2 = mock(Tab.class);
        when(tab2.getId()).thenReturn(2);
        when(tab2.getUrl()).thenReturn(new GURL(UrlConstants.GOOGLE_URL));
        when(mTabCreator.createFrozenTab(any(), eq(2), eq(1))).thenReturn(tab2);

        mRestorer.onDataLoaded(mStorageLoadedData);
        mRestorer.start(/* restoreActiveTabImmediately= */ false);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertTrue(cachedState.isClaimedOrDestroyed());
        assertTrue(dbState1.isClaimedOrDestroyed());
        assertTrue(dbState2.isClaimedOrDestroyed());
        verify(mDelegate).onFinished(eq(false));
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
