// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
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
import org.robolectric.android.util.concurrent.PausedExecutorService;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.UserDataHost;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.tabmodel.TabCacheManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateAttributes.DirtinessState;
import org.chromium.chrome.browser.tab.TabStateAttributesRegistry;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.nio.ByteBuffer;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link BackgroundTabPool}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BackgroundTabPoolTest {
    private static final String PROFILE_TOKEN = "mock_token";
    private static final @TabId int TAB_ID_1 = 101;
    private static final @TabId int TAB_ID_2 = 102;
    private static final @TabId int PLACEHOLDER_ID = 999;
    private static final @ActorTaskId int TASK_ID = 555;

    public final @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final PausedExecutorService mExecutor = new PausedExecutorService();

    private @Mock TabModel mTabModel;
    private @Mock TabCreator mTabCreator;
    private @Mock Runnable mOnEmptyCallback;

    private BackgroundTabPool mPool;

    @Before
    public void setUp() {
        PostTask.setPrenativeThreadPoolExecutorForTesting(mExecutor);
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        when(mTabModel.getTabById(PLACEHOLDER_ID)).thenReturn(null);
        mPool = new BackgroundTabPool(PROFILE_TOKEN, mOnEmptyCallback);
    }

    @After
    public void tearDown() {
        mExecutor.runAll();
        TabCacheManager.resetForTesting();
        TabStateExtractor.resetTabStatesForTesting();
    }

    @Test
    public void testGetProfileToken() {
        assertEquals(PROFILE_TOKEN, mPool.getProfileToken());
    }

    @Test(expected = AssertionError.class)
    public void testAddDestroyedTabThrowsAssertion() {
        Tab tab = createMockTab(TAB_ID_1);
        when(tab.isDestroyed()).thenReturn(true);
        new LiveBackgroundTab(mPool, tab, PLACEHOLDER_ID, /* taskId= */ null);
    }

    @Test
    public void testAddAndGetLiveTab() {
        Tab tab = createMockTab(TAB_ID_1);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState);

        LiveBackgroundTab liveTab =
                new LiveBackgroundTab(mPool, tab, PLACEHOLDER_ID, /* taskId= */ 42);
        mPool.addLiveTab(liveTab);

        assertFalse(mPool.isEmpty());
        assertEquals(1, mPool.getLiveTabCount());
        assertSame(liveTab, mPool.getLiveTab(TAB_ID_1));

        Set<@TabId Integer> allIds = mPool.getAllTabIds();
        assertEquals(1, allIds.size());
        assertTrue(allIds.contains(TAB_ID_1));
    }

    @Test
    public void testLoadTab_liveTab() {
        Tab tab = createMockTab(TAB_ID_1);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState);

        LiveBackgroundTab liveTab =
                new LiveBackgroundTab(mPool, tab, PLACEHOLDER_ID, /* taskId= */ null);
        mPool.addLiveTab(liveTab);

        BackgroundPoolTab loaded = mPool.loadTab(TAB_ID_1, PLACEHOLDER_ID);
        assertNotNull(loaded);
        assertSame(liveTab, loaded);

        // Attaching the live tab auto-removes it from the pool in CL 2
        loaded.attachTab(mTabModel, 0);
        assertNull(mPool.getLiveTab(TAB_ID_1));
        assertTrue(mPool.isEmpty());

        ShadowLooper.idleMainLooper();
        verify(mOnEmptyCallback).run();
    }

    @Test
    public void testLoadTab_coldTab() {
        Tab tab = createMockTab(TAB_ID_1);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState);

        // Add to pool and drain executor to persist into TabCache
        BackgroundTabPool firstPool = new BackgroundTabPool(PROFILE_TOKEN, mOnEmptyCallback);
        firstPool.addLiveTab(
                new LiveBackgroundTab(firstPool, tab, PLACEHOLDER_ID, /* taskId= */ null));
        firstPool.onTabStateDirtinessChanged(tab, DirtinessState.DIRTY);
        mExecutor.runAll();
        firstPool.destroy();

        BackgroundTabPool secondPool = new BackgroundTabPool(PROFILE_TOKEN, mOnEmptyCallback);
        Tab createdTab = createMockTab(TAB_ID_1);
        when(mTabCreator.createFrozenTab(any(), eq(TAB_ID_1), eq(0))).thenReturn(createdTab);

        BackgroundPoolTab coldLoaded = secondPool.loadTab(TAB_ID_1, PLACEHOLDER_ID);
        assertNotNull(coldLoaded);
        assertTrue(coldLoaded instanceof ColdBackgroundTab);

        coldLoaded.attachTab(mTabModel, 0);
        verify(mTabCreator).createFrozenTab(any(), eq(TAB_ID_1), eq(0));
    }

    @Test
    public void testIsEmpty_withColdTabs() {
        Tab tab = createMockTab(TAB_ID_1);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState);

        BackgroundTabPool firstPool = new BackgroundTabPool(PROFILE_TOKEN, mOnEmptyCallback);
        firstPool.addLiveTab(
                new LiveBackgroundTab(firstPool, tab, PLACEHOLDER_ID, /* taskId= */ null));
        firstPool.onTabStateDirtinessChanged(tab, DirtinessState.DIRTY);
        mExecutor.runAll();
        firstPool.destroy();

        BackgroundTabPool secondPool = new BackgroundTabPool(PROFILE_TOKEN, mOnEmptyCallback);
        assertFalse(secondPool.isEmpty());
        assertEquals(0, secondPool.getLiveTabCount());
        assertEquals(Set.of(TAB_ID_1), secondPool.getAllTabIds());

        secondPool.clearAll();
        assertTrue(secondPool.isEmpty());
        assertTrue(secondPool.getAllTabIds().isEmpty());
    }

    @Test
    public void testAttachLiveBackgroundTabTwice_throwsAssertionError() {
        Tab tab = createMockTab(TAB_ID_1);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState);

        LiveBackgroundTab liveTab =
                new LiveBackgroundTab(mPool, tab, PLACEHOLDER_ID, /* taskId= */ null);
        mPool.addLiveTab(liveTab);

        liveTab.attachTab(mTabModel, 0);
        assertThrows(AssertionError.class, () -> liveTab.attachTab(mTabModel, 0));
    }

    @Test
    public void testRemoveTab_clearsFromTabCache() {
        Tab tab = createMockTab(TAB_ID_1);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState);

        mPool.addLiveTab(new LiveBackgroundTab(mPool, tab, PLACEHOLDER_ID, /* taskId= */ null));
        mPool.onTabStateDirtinessChanged(tab, DirtinessState.DIRTY);
        mExecutor.runAll();

        assertTrue(mPool.getAllTabIds().contains(TAB_ID_1));

        mPool.removeTab(TAB_ID_1);
        mExecutor.runAll();

        assertNull(mPool.getLiveTab(TAB_ID_1));
        assertTrue(mPool.getAllTabIds().isEmpty());
        assertNull(mPool.loadTab(TAB_ID_1, PLACEHOLDER_ID));
    }

    @Test
    public void testLiveBackgroundTab_staticMarkDirtyTriggersSave() {
        Tab tab = createMockTab(TAB_ID_1);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState);
        TabStateAttributesRegistry.createAttributesForTab(
                tab, BackgroundTabPool.class, TabCreationState.FROZEN_ON_RESTORE);

        LiveBackgroundTab liveTab = new LiveBackgroundTab(mPool, tab, PLACEHOLDER_ID, TASK_ID);
        mPool.addLiveTab(liveTab);
        mExecutor.runAll();

        // Calling static LiveBackgroundTab.markDirty(tab) triggers save via TabStateAttributes
        // observer
        LiveBackgroundTab.markDirty(tab);
        mExecutor.runAll();

        // Verify state is persisted by loading cold from a new pool
        mPool.destroy();
        BackgroundTabPool coldPool = new BackgroundTabPool(PROFILE_TOKEN, mOnEmptyCallback);
        Tab createdTab = createMockTab(TAB_ID_1);
        when(mTabCreator.createFrozenTab(any(), eq(TAB_ID_1), eq(0))).thenReturn(createdTab);

        BackgroundPoolTab coldLoaded = coldPool.loadTab(TAB_ID_1, PLACEHOLDER_ID);
        assertNotNull(coldLoaded);
        assertTrue(coldLoaded instanceof ColdBackgroundTab);
    }

    @Test
    public void testLiveBackgroundTab_markDirtyTriggersSave() {
        Tab tab = createMockTab(TAB_ID_1);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState);
        TabStateAttributesRegistry.createAttributesForTab(
                tab, BackgroundTabPool.class, TabCreationState.FROZEN_ON_RESTORE);

        LiveBackgroundTab liveTab = new LiveBackgroundTab(mPool, tab, PLACEHOLDER_ID, TASK_ID);
        mPool.addLiveTab(liveTab);
        mExecutor.runAll();

        // Calling markDirty() directly triggers save via TabStateAttributes observer
        liveTab.markDirty();
        mExecutor.runAll();

        // Verify state is persisted by loading cold from a new pool
        mPool.destroy();
        BackgroundTabPool coldPool = new BackgroundTabPool(PROFILE_TOKEN, mOnEmptyCallback);
        Tab createdTab = createMockTab(TAB_ID_1);
        when(mTabCreator.createFrozenTab(any(), eq(TAB_ID_1), eq(0))).thenReturn(createdTab);

        BackgroundPoolTab coldLoaded = coldPool.loadTab(TAB_ID_1, PLACEHOLDER_ID);
        assertNotNull(coldLoaded);
        assertTrue(coldLoaded instanceof ColdBackgroundTab);
    }

    @Test
    public void testRemoveTabCallsOnEmptyCallback() {
        Tab tab = createMockTab(TAB_ID_1);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState);

        mPool.addLiveTab(new LiveBackgroundTab(mPool, tab, PLACEHOLDER_ID, /* taskId= */ null));
        assertFalse(mPool.isEmpty());

        mPool.removeTab(TAB_ID_1);
        assertTrue(mPool.isEmpty());
        assertEquals(0, mPool.getLiveTabCount());

        ShadowLooper.idleMainLooper();
        verify(mOnEmptyCallback).run();
    }

    @Test
    public void testClearAll() {
        Tab tab1 = createMockTab(TAB_ID_1);
        Tab tab2 = createMockTab(TAB_ID_2);
        TabState tabState1 = createMockTabState();
        TabState tabState2 = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState1);
        TabStateExtractor.setTabStateForTesting(TAB_ID_2, tabState2);

        mPool.addLiveTab(new LiveBackgroundTab(mPool, tab1, PLACEHOLDER_ID, /* taskId= */ null));
        mPool.addLiveTab(new LiveBackgroundTab(mPool, tab2, PLACEHOLDER_ID, /* taskId= */ null));
        mExecutor.runAll();

        mPool.clearAll();
        mExecutor.runAll();

        assertNull(mPool.getLiveTab(TAB_ID_1));
        assertNull(mPool.getLiveTab(TAB_ID_2));
        assertTrue(mPool.getAllTabIds().isEmpty());
        assertNull(mPool.loadTab(TAB_ID_1, PLACEHOLDER_ID));
        assertNull(mPool.loadTab(TAB_ID_2, PLACEHOLDER_ID));
    }

    @Test
    public void testDestroyDoesNotTriggerOnEmptyCallback() {
        Tab tab = createMockTab(TAB_ID_1);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState);

        mPool.addLiveTab(new LiveBackgroundTab(mPool, tab, PLACEHOLDER_ID, /* taskId= */ null));
        mExecutor.runAll();

        mPool.destroy();
        mExecutor.runAll();

        ShadowLooper.idleMainLooper();
        verify(mOnEmptyCallback, never()).run();

        BackgroundTabPool secondPool = new BackgroundTabPool(PROFILE_TOKEN, mOnEmptyCallback);
        Tab createdTab = createMockTab(TAB_ID_1);
        when(mTabCreator.createFrozenTab(any(), eq(TAB_ID_1), eq(0))).thenReturn(createdTab);

        secondPool.prefetchTabs(List.of(TAB_ID_1));
        mExecutor.runAll();

        BackgroundPoolTab coldLoaded = secondPool.loadTab(TAB_ID_1, PLACEHOLDER_ID);
        assertNotNull(coldLoaded);
        assertTrue(coldLoaded instanceof ColdBackgroundTab);
    }

    @Test
    public void testOperationsAfterDestroyThrowAssertionError() {
        Tab tab = createMockTab(TAB_ID_1);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState);

        mPool.addLiveTab(new LiveBackgroundTab(mPool, tab, PLACEHOLDER_ID, /* taskId= */ null));
        mPool.destroy();

        LiveBackgroundTab newLiveTab =
                new LiveBackgroundTab(mPool, tab, PLACEHOLDER_ID, /* taskId= */ null);
        List<@TabId Integer> tabIds = List.of(TAB_ID_1);

        assertThrows(AssertionError.class, () -> mPool.destroy());
        assertThrows(AssertionError.class, () -> mPool.getProfileToken());
        assertThrows(AssertionError.class, () -> mPool.addLiveTab(newLiveTab));
        assertThrows(AssertionError.class, () -> mPool.loadTab(TAB_ID_1, PLACEHOLDER_ID));
        assertThrows(AssertionError.class, () -> mPool.prefetchTabs(tabIds));
        assertThrows(AssertionError.class, () -> mPool.isEmpty());
        assertThrows(AssertionError.class, () -> mPool.getLiveTabCount());
        assertThrows(AssertionError.class, () -> mPool.getLiveTab(TAB_ID_1));
        assertThrows(AssertionError.class, () -> mPool.getAllTabIds());
        assertThrows(AssertionError.class, () -> mPool.removeTab(TAB_ID_1));
        assertThrows(AssertionError.class, () -> mPool.clearAll());
        assertThrows(AssertionError.class, () -> mPool.cleanupPostRestore());
        assertThrows(
                AssertionError.class,
                () -> mPool.onTabStateDirtinessChanged(tab, DirtinessState.DIRTY));
    }

    @Test
    public void testPrefetchTabs() {
        Tab tab1 = createMockTab(TAB_ID_1);
        Tab tab2 = createMockTab(TAB_ID_2);
        TabState tabState1 = createMockTabState();
        TabState tabState2 = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState1);
        TabStateExtractor.setTabStateForTesting(TAB_ID_2, tabState2);

        BackgroundTabPool firstPool = new BackgroundTabPool(PROFILE_TOKEN, mOnEmptyCallback);
        firstPool.addLiveTab(
                new LiveBackgroundTab(firstPool, tab1, PLACEHOLDER_ID, /* taskId= */ null));
        firstPool.addLiveTab(
                new LiveBackgroundTab(firstPool, tab2, PLACEHOLDER_ID, /* taskId= */ null));
        mExecutor.runAll();
        firstPool.destroy();

        BackgroundTabPool secondPool = new BackgroundTabPool(PROFILE_TOKEN, mOnEmptyCallback);
        secondPool.prefetchTabs(List.of(TAB_ID_1, TAB_ID_2));
        mExecutor.runAll();
    }

    @Test
    public void testGetAllTabIds_combinesLiveAndPersisted() {
        Tab tab1 = createMockTab(TAB_ID_1);
        Tab tab2 = createMockTab(TAB_ID_2);
        TabState tabState1 = createMockTabState();
        TabState tabState2 = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState1);
        TabStateExtractor.setTabStateForTesting(TAB_ID_2, tabState2);

        // Pre-populate tab2 as a cold tab in TabCache from an earlier session
        BackgroundTabPool prePool = new BackgroundTabPool(PROFILE_TOKEN, mOnEmptyCallback);
        prePool.addLiveTab(
                new LiveBackgroundTab(prePool, tab2, PLACEHOLDER_ID, /* taskId= */ null));
        prePool.onTabStateDirtinessChanged(tab2, DirtinessState.DIRTY);
        mExecutor.runAll();
        prePool.destroy();

        // Add tab1 as a live tab in the active pool
        mPool.addLiveTab(new LiveBackgroundTab(mPool, tab1, PLACEHOLDER_ID, /* taskId= */ null));
        mExecutor.runAll();

        Set<@TabId Integer> allIds = mPool.getAllTabIds();
        assertEquals(2, allIds.size());
        assertTrue(allIds.contains(TAB_ID_1));
        assertTrue(allIds.contains(TAB_ID_2));
    }

    private Tab createMockTab(@TabId int tabId) {
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(tabId);
        when(tab.isOffTheRecord()).thenReturn(false);
        when(tab.isDestroyed()).thenReturn(false);
        when(tab.hasParentCollection()).thenReturn(false);
        when(tab.getParentId()).thenReturn(Tab.INVALID_TAB_ID);
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());
        return tab;
    }

    private TabState createMockTabState() {
        TabState tabState = new TabState();
        tabState.contentsState = mock(WebContentsState.class);
        ByteBuffer buffer = ByteBuffer.allocateDirect(10);
        when(tabState.contentsState.buffer()).thenReturn(buffer);
        when(tabState.contentsState.version()).thenReturn(2);
        return tabState;
    }
}
