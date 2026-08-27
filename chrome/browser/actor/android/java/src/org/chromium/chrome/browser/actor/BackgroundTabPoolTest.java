// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateAttributes.DirtinessState;
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
    private static final @TabId int TAB_ID_1 = 101;
    private static final @TabId int TAB_ID_2 = 102;
    private static final @TabId int PLACEHOLDER_ID = 999;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ProfileResolver.Natives mProfileResolverNatives;
    @Mock private Profile mProfile;
    @Mock private TabModel mTabModel;
    @Mock private TabCreator mTabCreator;
    @Mock private Runnable mOnEmptyCallback;

    private final PausedExecutorService mExecutor = new PausedExecutorService();
    private BackgroundTabPool mPool;

    @Before
    public void setUp() {
        PostTask.setPrenativeThreadPoolExecutorForTesting(mExecutor);
        ProfileResolverJni.setInstanceForTesting(mProfileResolverNatives);
        TabCacheManager.resetForTesting();
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mProfile.isNativeInitialized()).thenReturn(true);
        when(mProfileResolverNatives.tokenizeProfile(mProfile)).thenReturn("mock_token");
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        mPool = new BackgroundTabPool(mProfile, mOnEmptyCallback);
    }

    @After
    public void tearDown() {
        mExecutor.runAll();
        TabCacheManager.resetForTesting();
        TabStateExtractor.resetTabStatesForTesting();
    }

    @Test(expected = AssertionError.class)
    public void testOffTheRecordProfileThrowsAssertion() {
        Profile otrProfile = mock(Profile.class);
        when(otrProfile.isOffTheRecord()).thenReturn(true);
        new BackgroundTabPool(otrProfile, mOnEmptyCallback);
    }

    @Test
    public void testGetProfile() {
        assertEquals(mProfile, mPool.getProfile());
    }

    @Test
    public void testAddLiveTabAndLoadTabLive() {
        Tab tab = createMockTab(TAB_ID_1);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState);

        LiveBackgroundTab liveTab = new LiveBackgroundTab(tab, PLACEHOLDER_ID, /* taskId= */ null);
        mPool.addLiveTab(liveTab);

        assertFalse(mPool.isEmpty());
        assertEquals(1, mPool.getLiveTabCount());
        assertEquals(liveTab, mPool.getLiveTab(TAB_ID_1));
        assertEquals(PLACEHOLDER_ID, liveTab.getPlaceholderTabId());
        assertEquals(Set.of(TAB_ID_1), mPool.getAllTabIds());

        BackgroundPoolTab loaded = mPool.loadTab(TAB_ID_1, PLACEHOLDER_ID);
        assertNotNull(loaded);
        assertTrue(loaded instanceof LiveBackgroundTab);
        assertNull(mPool.getLiveTab(TAB_ID_1));
        assertTrue(mPool.isEmpty());
        assertEquals(0, mPool.getLiveTabCount());

        ShadowLooper.idleMainLooper();
        verify(mOnEmptyCallback).run();

        loaded.attachTabImpl(mTabModel, /* index= */ 0);
        verify(mTabModel)
                .addTab(
                        tab,
                        /* index= */ 0,
                        TabLaunchType.FROM_RESTORE,
                        TabCreationState.LIVE_IN_BACKGROUND);
    }

    @Test
    public void testLoadTabColdFromCache() {
        Tab tab = createMockTab(TAB_ID_1);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState);

        mPool.addLiveTab(new LiveBackgroundTab(tab, PLACEHOLDER_ID, /* taskId= */ null));
        mExecutor.runAll();

        // Evict from live memory simulating cold background tab
        BackgroundPoolTab liveLoaded = mPool.loadTab(TAB_ID_1, PLACEHOLDER_ID);
        assertNotNull(liveLoaded);

        Tab createdTab = createMockTab(TAB_ID_1);
        when(mTabCreator.createFrozenTab(any(), eq(TAB_ID_1), eq(0))).thenReturn(createdTab);

        mPool.prefetchTabs(List.of(TAB_ID_1));
        mExecutor.runAll();

        BackgroundPoolTab coldLoaded = mPool.loadTab(TAB_ID_1, PLACEHOLDER_ID);
        assertNotNull(coldLoaded);
        assertTrue(coldLoaded instanceof ColdBackgroundTab);

        Tab attached = coldLoaded.attachTabImpl(mTabModel, /* index= */ 0);
        assertEquals(createdTab, attached);
        verify(mTabCreator).createFrozenTab(any(), eq(TAB_ID_1), eq(0));
    }

    @Test
    public void testTabObserverTriggersSaveOnDirtinessChanged() {
        Tab tab = createMockTab(TAB_ID_1);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState);

        mPool.addLiveTab(new LiveBackgroundTab(tab, PLACEHOLDER_ID, /* taskId= */ null));
        mPool.onTabStateDirtinessChanged(tab, DirtinessState.DIRTY);
        mExecutor.runAll();

        // Verify state is persisted by loading cold after eviction
        mPool.removeTab(TAB_ID_1);
        Tab createdTab = createMockTab(TAB_ID_1);
        when(mTabCreator.createFrozenTab(any(), eq(TAB_ID_1), eq(0))).thenReturn(createdTab);

        BackgroundPoolTab coldLoaded = mPool.loadTab(TAB_ID_1, PLACEHOLDER_ID);
        assertNotNull(coldLoaded);
        assertTrue(coldLoaded instanceof ColdBackgroundTab);
    }

    @Test
    public void testRemoveTabCallsOnEmptyCallback() {
        Tab tab = createMockTab(TAB_ID_1);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState);

        mPool.addLiveTab(new LiveBackgroundTab(tab, PLACEHOLDER_ID, /* taskId= */ null));
        assertFalse(mPool.isEmpty());

        mPool.removeTab(TAB_ID_1);
        assertTrue(mPool.isEmpty());
        assertEquals(0, mPool.getLiveTabCount());

        ShadowLooper.idleMainLooper();
        verify(mOnEmptyCallback).run();
    }

    @Test
    public void testClearTab() {
        Tab tab = createMockTab(TAB_ID_1);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState);

        mPool.addLiveTab(new LiveBackgroundTab(tab, PLACEHOLDER_ID, /* taskId= */ null));
        mExecutor.runAll();

        mPool.clearTab(TAB_ID_1);
        mExecutor.runAll();

        assertNull(mPool.getLiveTab(TAB_ID_1));
        assertTrue(mPool.getAllTabIds().isEmpty());
        assertNull(mPool.loadTab(TAB_ID_1, PLACEHOLDER_ID));
    }

    @Test
    public void testClearAll() {
        Tab tab1 = createMockTab(TAB_ID_1);
        Tab tab2 = createMockTab(TAB_ID_2);
        TabState tabState1 = createMockTabState();
        TabState tabState2 = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState1);
        TabStateExtractor.setTabStateForTesting(TAB_ID_2, tabState2);

        mPool.addLiveTab(new LiveBackgroundTab(tab1, PLACEHOLDER_ID, /* taskId= */ null));
        mPool.addLiveTab(new LiveBackgroundTab(tab2, PLACEHOLDER_ID, /* taskId= */ null));
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

        mPool.addLiveTab(new LiveBackgroundTab(tab, PLACEHOLDER_ID, /* taskId= */ null));
        mExecutor.runAll();

        mPool.destroy();
        mExecutor.runAll();

        ShadowLooper.idleMainLooper();
        verify(mOnEmptyCallback, never()).run();

        // TabCache persisted state remains intact so cold loading succeeds on a second pool.
        BackgroundTabPool secondPool = new BackgroundTabPool(mProfile, mOnEmptyCallback);
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

        mPool.addLiveTab(new LiveBackgroundTab(tab, PLACEHOLDER_ID, /* taskId= */ null));
        mPool.destroy();

        LiveBackgroundTab newLiveTab =
                new LiveBackgroundTab(tab, PLACEHOLDER_ID, /* taskId= */ null);
        List<@TabId Integer> tabIds = List.of(TAB_ID_1);

        assertThrows(AssertionError.class, () -> mPool.destroy());
        assertThrows(AssertionError.class, () -> mPool.getProfile());
        assertThrows(AssertionError.class, () -> mPool.addLiveTab(newLiveTab));
        assertThrows(AssertionError.class, () -> mPool.loadTab(TAB_ID_1, PLACEHOLDER_ID));
        assertThrows(AssertionError.class, () -> mPool.prefetchTabs(tabIds));
        assertThrows(AssertionError.class, () -> mPool.isEmpty());
        assertThrows(AssertionError.class, () -> mPool.getLiveTabCount());
        assertThrows(AssertionError.class, () -> mPool.getLiveTab(TAB_ID_1));
        assertThrows(AssertionError.class, () -> mPool.getAllTabIds());
        assertThrows(AssertionError.class, () -> mPool.removeTab(TAB_ID_1));
        assertThrows(AssertionError.class, () -> mPool.clearTab(TAB_ID_1));
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

        mPool.addLiveTab(new LiveBackgroundTab(tab1, PLACEHOLDER_ID, /* taskId= */ null));
        mPool.addLiveTab(new LiveBackgroundTab(tab2, PLACEHOLDER_ID, /* taskId= */ null));
        mExecutor.runAll();

        // Remove from memory to make cold
        mPool.loadTab(TAB_ID_1, PLACEHOLDER_ID);
        mPool.loadTab(TAB_ID_2, PLACEHOLDER_ID);

        mPool.prefetchTabs(List.of(TAB_ID_1, TAB_ID_2));
        mExecutor.runAll();

        assertNotNull(mPool.loadTab(TAB_ID_1, PLACEHOLDER_ID));
        assertNotNull(mPool.loadTab(TAB_ID_2, PLACEHOLDER_ID));
    }

    @Test
    public void testGetAllTabIds_combinesLiveAndPersisted() {
        Tab tab1 = createMockTab(TAB_ID_1);
        Tab tab2 = createMockTab(TAB_ID_2);
        TabState tabState1 = createMockTabState();
        TabState tabState2 = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState1);
        TabStateExtractor.setTabStateForTesting(TAB_ID_2, tabState2);

        mPool.addLiveTab(new LiveBackgroundTab(tab1, PLACEHOLDER_ID, /* taskId= */ null));
        mPool.addLiveTab(new LiveBackgroundTab(tab2, PLACEHOLDER_ID, /* taskId= */ null));
        mExecutor.runAll();

        // Remove tab2 from live memory without clearing cache -> cold tab in cache
        mPool.removeTab(TAB_ID_2);

        Set<@TabId Integer> allIds = mPool.getAllTabIds();
        assertEquals(2, allIds.size());
        assertTrue(allIds.contains(TAB_ID_1));
        assertTrue(allIds.contains(TAB_ID_2));
    }

    private Tab createMockTab(@TabId int tabId) {
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(tabId);
        when(tab.isOffTheRecord()).thenReturn(false);
        when(tab.hasParentCollection()).thenReturn(false);
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
