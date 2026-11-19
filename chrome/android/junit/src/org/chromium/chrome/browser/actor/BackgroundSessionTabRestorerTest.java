// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
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
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.actor.background_tabs.BackgroundSession;
import org.chromium.chrome.browser.actor.background_tabs.OffscreenRenderingManager;
import org.chromium.chrome.browser.app.tabmodel.TabCacheManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BackgroundSessionTabRestorer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowSystemClock.class})
public class BackgroundSessionTabRestorerTest {
    private static final int TAB_ID = 100;
    private static final int PLACEHOLDER_TAB_ID = 101;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ProfileResolver.Natives mProfileResolverNatives;
    @Mock private Profile mProfile;
    @Mock private OffscreenRenderingManager mOffscreenRenderingManager;
    @Mock private ActivityWindowAndroid mWindowAndroid;
    @Mock private Tab mTab;
    @Mock private Tab mPlaceholderTab;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private TabRemover mTabRemover;
    @Mock private TabDelegateFactory mTabDelegateFactory;

    @Before
    public void setUp() {
        MultiWindowTestUtils.ensureInitialized();

        ProfileResolverJni.setInstanceForTesting(mProfileResolverNatives);
        when(mProfileResolverNatives.tokenizeProfile(any())).thenReturn("mock_token");

        OffscreenRenderingManager.setInstanceForTesting(mOffscreenRenderingManager);

        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mProfile.isOffTheRecord()).thenReturn(false);
    }

    @After
    public void tearDown() {
        ActorMetrics.resetForTesting();
        ProfileResolverJni.setInstanceForTesting(null);
        BackgroundTabPoolManager.resetForTesting();
        TabCacheManager.resetForTesting();
        MultiWindowTestUtils.resetInstanceInfo();
    }

    private void stubLiveTabAttach() {
        when(mTab.hasParentCollection()).thenReturn(false);
        when(mTab.isDestroyed()).thenReturn(false);
        when(mTab.isOffTheRecord()).thenReturn(false);
    }

    private void stubPlaceholderTab() {
        when(mTabModel.getTabById(PLACEHOLDER_TAB_ID)).thenReturn(mPlaceholderTab);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
    }

    private void addLiveTabToPool(Tab tab, int placeholderTabId) {
        BackgroundTabPool pool = BackgroundTabPoolManager.acquire(mProfile);
        try {
            pool.addLiveTab(
                    new LiveBackgroundTab(
                            pool,
                            tab,
                            placeholderTabId,
                            /* originalTabIndex= */ TabModel.INVALID_TAB_INDEX));
        } finally {
            BackgroundTabPoolManager.release(pool);
        }
    }

    @Test
    public void testRestoreTabsForWindow_matchingWindow() {
        stubLiveTabAttach();
        stubPlaceholderTab();
        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.indexOf(mPlaceholderTab)).thenReturn(0);

        BackgroundSession session = new BackgroundSession(mTab, 500);
        BackgroundSession.BackgroundTabData tabData = session.getTabDataList().get(0);
        tabData.setTabWindowId(1);
        tabData.setPlaceholderTabId(PLACEHOLDER_TAB_ID);
        addLiveTabToPool(mTab, PLACEHOLDER_TAB_ID);

        List<BackgroundSession> removed = new ArrayList<>();
        int restoredCount =
                BackgroundSessionTabRestorer.restoreTabsForWindow(
                        mTabModelSelector,
                        1,
                        mWindowAndroid,
                        List.of(session),
                        mTabDelegateFactory,
                        /* waitDurationMs= */ 0,
                        /* isColdStart= */ false,
                        removed::addAll);

        assertEquals(1, restoredCount);
        assertEquals(List.of(session), removed);
        assertTrue(session.getTabDataList().isEmpty());
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
        verify(mTab).updateAttachment(mWindowAndroid, mTabDelegateFactory);
        verify(mTabRemover).removeTab(mPlaceholderTab, false);
    }

    @Test
    public void testRestoreTabsForWindow_nonMatchingWindow() {
        MultiWindowTestUtils.enableMultiInstance();
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 2,
                /* url= */ "https://www.example.com",
                /* tabCount= */ 1,
                /* taskId= */ 57);

        BackgroundSession session = new BackgroundSession(mTab, 500);
        BackgroundSession.BackgroundTabData tabData = session.getTabDataList().get(0);
        tabData.setTabWindowId(2);
        tabData.setPlaceholderTabId(PLACEHOLDER_TAB_ID);
        addLiveTabToPool(mTab, PLACEHOLDER_TAB_ID);

        List<BackgroundSession> removed = new ArrayList<>();
        int restoredCount =
                BackgroundSessionTabRestorer.restoreTabsForWindow(
                        mTabModelSelector,
                        1,
                        mWindowAndroid,
                        List.of(session),
                        mTabDelegateFactory,
                        /* waitDurationMs= */ 0,
                        /* isColdStart= */ false,
                        removed::addAll);

        assertEquals(0, restoredCount);
        assertTrue(removed.isEmpty());
        assertEquals(1, session.getTabDataList().size());
        assertEquals(mTab, session.getTabDataList().get(0).getTab());
        verify(mTab, never()).updateAttachment(any(), any());
    }

    @Test
    public void testRestoreTabsForWindow_closedWindow_restoresIntoActiveWindow() {
        MultiWindowTestUtils.enableMultiInstance();
        stubLiveTabAttach();
        stubPlaceholderTab();
        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.indexOf(mPlaceholderTab)).thenReturn(0);

        BackgroundSession session = new BackgroundSession(mTab, 500);
        BackgroundSession.BackgroundTabData tabData = session.getTabDataList().get(0);
        tabData.setTabWindowId(2);
        tabData.setPlaceholderTabId(PLACEHOLDER_TAB_ID);
        addLiveTabToPool(mTab, PLACEHOLDER_TAB_ID);

        List<BackgroundSession> removed = new ArrayList<>();
        int restoredCount =
                BackgroundSessionTabRestorer.restoreTabsForWindow(
                        mTabModelSelector,
                        1,
                        mWindowAndroid,
                        List.of(session),
                        mTabDelegateFactory,
                        /* waitDurationMs= */ 0,
                        /* isColdStart= */ false,
                        removed::addAll);

        assertEquals(1, restoredCount);
        assertEquals(List.of(session), removed);
        assertTrue(session.getTabDataList().isEmpty());
        verify(mTab).updateAttachment(mWindowAndroid, mTabDelegateFactory);
    }

    @Test
    public void testRestoreTabsForWindow_inactiveWindow_restoresIntoActiveWindow() {
        MultiWindowTestUtils.enableMultiInstance();
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 2,
                /* url= */ "https://www.example.com",
                /* tabCount= */ 1,
                /* taskId= */ -1);
        stubLiveTabAttach();
        stubPlaceholderTab();
        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.indexOf(mPlaceholderTab)).thenReturn(0);

        BackgroundSession session = new BackgroundSession(mTab, 500);
        BackgroundSession.BackgroundTabData tabData = session.getTabDataList().get(0);
        tabData.setTabWindowId(2);
        tabData.setPlaceholderTabId(PLACEHOLDER_TAB_ID);
        addLiveTabToPool(mTab, PLACEHOLDER_TAB_ID);

        List<BackgroundSession> removed = new ArrayList<>();
        int restoredCount =
                BackgroundSessionTabRestorer.restoreTabsForWindow(
                        mTabModelSelector,
                        1,
                        mWindowAndroid,
                        List.of(session),
                        mTabDelegateFactory,
                        /* waitDurationMs= */ 0,
                        /* isColdStart= */ false,
                        removed::addAll);

        assertEquals(1, restoredCount);
        assertEquals(List.of(session), removed);
        assertTrue(session.getTabDataList().isEmpty());
        verify(mTab).updateAttachment(mWindowAndroid, mTabDelegateFactory);
    }

    @Test
    public void testRestoreTabsForWindow_evictedTabCleanup() {
        stubPlaceholderTab();
        when(mPlaceholderTab.isDestroyed()).thenReturn(false);

        BackgroundSession session = new BackgroundSession(mTab, 500);
        BackgroundSession.BackgroundTabData tabData = session.getTabDataList().get(0);
        tabData.setTabWindowId(1);
        tabData.setPlaceholderTabId(PLACEHOLDER_TAB_ID);

        List<BackgroundSession> removed = new ArrayList<>();
        int restoredCount =
                BackgroundSessionTabRestorer.restoreTabsForWindow(
                        mTabModelSelector,
                        1,
                        mWindowAndroid,
                        List.of(session),
                        mTabDelegateFactory,
                        /* waitDurationMs= */ 0,
                        /* isColdStart= */ false,
                        removed::addAll);

        assertEquals(0, restoredCount);
        assertEquals(List.of(session), removed);
        assertTrue(session.getTabDataList().isEmpty());
        verify(mTab, never()).updateAttachment(any(), any());
        verify(mTabRemover).removeTab(mPlaceholderTab, false);
    }

    @Test
    public void testRestoreTabsForWindow_placeholderFreeTab_appendsToEnd() {
        stubLiveTabAttach();
        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.getCount()).thenReturn(3);

        BackgroundSession session = new BackgroundSession(mTab, 500);
        BackgroundSession.BackgroundTabData tabData = session.getTabDataList().get(0);
        tabData.setTabWindowId(1);
        tabData.setPlaceholderTabId(null);
        addLiveTabToPool(mTab, Tab.INVALID_TAB_ID);

        List<BackgroundSession> removed = new ArrayList<>();
        int restoredCount =
                BackgroundSessionTabRestorer.restoreTabsForWindow(
                        mTabModelSelector,
                        1,
                        mWindowAndroid,
                        List.of(session),
                        mTabDelegateFactory,
                        /* waitDurationMs= */ 0,
                        /* isColdStart= */ false,
                        removed::addAll);

        assertEquals(1, restoredCount);
        assertEquals(List.of(session), removed);
        assertTrue(session.getTabDataList().isEmpty());
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
        verify(mTab).updateAttachment(mWindowAndroid, mTabDelegateFactory);
        verify(mTabModel).addTab(eq(mTab), eq(3), anyInt(), anyInt());
    }

    @Test
    public void testRestoreTabsForWindow_coldStart_recordsWaitAndColdLatency() {
        stubLiveTabAttach();
        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.getCount()).thenReturn(1);

        BackgroundSession session = new BackgroundSession(mTab, 500);
        session.getTabDataList().get(0).setTabWindowId(42);
        addLiveTabToPool(mTab, Tab.INVALID_TAB_ID);

        doAnswer(
                        inv -> {
                            ShadowSystemClock.advanceBy(35, TimeUnit.MILLISECONDS);
                            return null;
                        })
                .when(mTab)
                .updateAttachment(mWindowAndroid, mTabDelegateFactory);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                ActorMetrics
                                        .ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_WAIT_FOR_TAB_STATE_INITIALIZED,
                                200)
                        .expectIntRecord(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_TOTAL, 235)
                        .expectIntRecord(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_TOTAL_COLD,
                                235)
                        .expectNoRecords(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_TOTAL_WARM)
                        .build();

        List<BackgroundSession> removed = new ArrayList<>();
        int restoredCount =
                BackgroundSessionTabRestorer.restoreTabsForWindow(
                        mTabModelSelector,
                        42,
                        mWindowAndroid,
                        List.of(session),
                        mTabDelegateFactory,
                        /* waitDurationMs= */ 200,
                        /* isColdStart= */ true,
                        removed::addAll);

        assertEquals(1, restoredCount);
        assertEquals(List.of(session), removed);
        assertTrue(session.getTabDataList().isEmpty());
        watcher.assertExpected();
    }
}
