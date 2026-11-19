// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.actor.background_tabs.BackgroundSession;
import org.chromium.chrome.browser.actor.background_tabs.OffscreenRenderingManager;
import org.chromium.chrome.browser.app.tabmodel.TabCacheManager;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.ui.base.ActivityWindowAndroid;

import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BackgroundSessionTabRestorer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowSystemClock.class})
public class BackgroundSessionTabRestorerTest {
    private static class TestChromeTabbedActivity extends ChromeTabbedActivity {
        private ActivityWindowAndroid mWindowAndroid;

        @Override
        public ActivityWindowAndroid getWindowAndroid() {
            return mWindowAndroid;
        }
    }

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
    @Mock private TabCreator mTabCreator;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private TabDelegateFactory mTabDelegateFactory;
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private TabModelOrchestrator mTabModelOrchestrator;
    @Mock private TabPersistentStore mTabPersistentStore;

    @Before
    public void setUp() {
        MultiWindowTestUtils.ensureInitialized();

        ProfileResolverJni.setInstanceForTesting(mProfileResolverNatives);
        when(mProfileResolverNatives.tokenizeProfile(any())).thenReturn("mock_token");

        OffscreenRenderingManager.setInstanceForTesting(mOffscreenRenderingManager);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);

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
        ApplicationStatus.destroyForJUnitTests();
        TabWindowManagerSingleton.setTabWindowManagerForTesting(null);
        MultiWindowTestUtils.resetInstanceInfo();
    }

    private void stubLiveTabAttach() {
        stubLiveTabAttach(mTab);
    }

    private void stubLiveTabAttach(Tab tab) {
        when(tab.hasParentCollection()).thenReturn(false);
        when(tab.isDestroyed()).thenReturn(false);
        when(tab.isOffTheRecord()).thenReturn(false);
    }

    private void stubPlaceholderTab() {
        when(mTabModel.getTabById(PLACEHOLDER_TAB_ID)).thenReturn(mPlaceholderTab);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
    }

    private void setupWarmActivityMocks(TestChromeTabbedActivity activity, int windowId) {
        activity.mWindowAndroid = mWindowAndroid;
        activity.setTabModelOrchestratorForTesting(mTabModelOrchestrator);
        when(mTabModelOrchestrator.getTabPersistentStore()).thenReturn(mTabPersistentStore);

        when(mTabWindowManager.getIdForWindow(activity)).thenReturn(windowId);
        when(mTabWindowManager.getTabModelSelectorById(windowId)).thenReturn(mTabModelSelector);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.getTabCreatorManager()).thenReturn(mTabCreatorManager);
        when(mTabCreatorManager.getTabCreator(false)).thenReturn(mTabCreator);
        when(mTabCreator.createDefaultTabDelegateFactory()).thenReturn(mTabDelegateFactory);
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
    public void testRestoreSessionsIntoWindow_matchingWindow() {
        stubLiveTabAttach();
        stubPlaceholderTab();
        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.indexOf(mPlaceholderTab)).thenReturn(0);

        BackgroundSession session = new BackgroundSession(mTab, 500);
        BackgroundSession.BackgroundTabData tabData = session.getTabDataList().get(0);
        tabData.setTabWindowId(1);
        tabData.setPlaceholderTabId(PLACEHOLDER_TAB_ID);
        addLiveTabToPool(mTab, PLACEHOLDER_TAB_ID);

        int restoredCount =
                BackgroundSessionTabRestorer.restoreSessionsIntoWindow(
                        List.of(session),
                        mTabModelSelector,
                        1,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        /* waitDurationMs= */ 0,
                        /* isColdStart= */ false);

        assertEquals(1, restoredCount);
        assertTrue(session.getTabDataList().isEmpty());
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
        verify(mTab).updateAttachment(mWindowAndroid, mTabDelegateFactory);
        verify(mTabRemover).removeTab(mPlaceholderTab, false);
    }

    @Test
    public void testRestoreSessionsIntoWindow_nonMatchingWindow() {
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

        int restoredCount =
                BackgroundSessionTabRestorer.restoreSessionsIntoWindow(
                        List.of(session),
                        mTabModelSelector,
                        1,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        /* waitDurationMs= */ 0,
                        /* isColdStart= */ false);

        assertEquals(0, restoredCount);
        assertEquals(1, session.getTabDataList().size());
        assertEquals(mTab, session.getTabDataList().get(0).getTab());
        verify(mTab, never()).updateAttachment(any(), any());
    }

    @Test
    public void testRestoreSessionsIntoWindow_closedWindow_restoresIntoActiveWindow() {
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

        int restoredCount =
                BackgroundSessionTabRestorer.restoreSessionsIntoWindow(
                        List.of(session),
                        mTabModelSelector,
                        1,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        /* waitDurationMs= */ 0,
                        /* isColdStart= */ false);

        assertEquals(1, restoredCount);
        assertTrue(session.getTabDataList().isEmpty());
        verify(mTab).updateAttachment(mWindowAndroid, mTabDelegateFactory);
    }

    @Test
    public void testRestoreSessionsIntoWindow_inactiveWindow_restoresIntoActiveWindow() {
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

        int restoredCount =
                BackgroundSessionTabRestorer.restoreSessionsIntoWindow(
                        List.of(session),
                        mTabModelSelector,
                        1,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        /* waitDurationMs= */ 0,
                        /* isColdStart= */ false);

        assertEquals(1, restoredCount);
        assertTrue(session.getTabDataList().isEmpty());
        verify(mTab).updateAttachment(mWindowAndroid, mTabDelegateFactory);
    }

    @Test
    public void testRestoreSessionsIntoWindow_evictedTabCleanup() {
        stubPlaceholderTab();
        when(mPlaceholderTab.isDestroyed()).thenReturn(false);

        BackgroundSession session = new BackgroundSession(mTab, 500);
        BackgroundSession.BackgroundTabData tabData = session.getTabDataList().get(0);
        tabData.setTabWindowId(1);
        tabData.setPlaceholderTabId(PLACEHOLDER_TAB_ID);

        int restoredCount =
                BackgroundSessionTabRestorer.restoreSessionsIntoWindow(
                        List.of(session),
                        mTabModelSelector,
                        1,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        /* waitDurationMs= */ 0,
                        /* isColdStart= */ false);

        assertEquals(0, restoredCount);
        assertTrue(session.getTabDataList().isEmpty());
        verify(mTab, never()).updateAttachment(any(), any());
        verify(mTabRemover).removeTab(mPlaceholderTab, false);
    }

    @Test
    public void testRestoreSessionsIntoWindow_placeholderFreeTab_appendsToEnd() {
        stubLiveTabAttach();
        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.getCount()).thenReturn(3);

        BackgroundSession session = new BackgroundSession(mTab, 500);
        BackgroundSession.BackgroundTabData tabData = session.getTabDataList().get(0);
        tabData.setTabWindowId(1);
        tabData.setPlaceholderTabId(null);
        addLiveTabToPool(mTab, Tab.INVALID_TAB_ID);

        int restoredCount =
                BackgroundSessionTabRestorer.restoreSessionsIntoWindow(
                        List.of(session),
                        mTabModelSelector,
                        1,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        /* waitDurationMs= */ 0,
                        /* isColdStart= */ false);

        assertEquals(1, restoredCount);
        assertTrue(session.getTabDataList().isEmpty());
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
        verify(mTab).updateAttachment(mWindowAndroid, mTabDelegateFactory);
        verify(mTabModel).addTab(eq(mTab), eq(3), anyInt(), anyInt());
    }

    @Test
    public void testRestoreSession_warmActivity_flushesTabStateAfterRecordingLatency() {
        TestChromeTabbedActivity activity =
                Robolectric.buildActivity(TestChromeTabbedActivity.class).get();
        setupWarmActivityMocks(activity, 42);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.STOPPED);

        stubLiveTabAttach();
        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.getCount()).thenReturn(1);

        BackgroundSession session = new BackgroundSession(mTab, 500);
        session.getTabDataList().get(0).setTabWindowId(42);
        addLiveTabToPool(mTab, Tab.INVALID_TAB_ID);

        doAnswer(
                        inv -> {
                            ShadowSystemClock.advanceBy(25, TimeUnit.MILLISECONDS);
                            return null;
                        })
                .when(mTab)
                .updateAttachment(mWindowAndroid, mTabDelegateFactory);
        doAnswer(
                        inv -> {
                            ShadowSystemClock.advanceBy(60, TimeUnit.MILLISECONDS);
                            return null;
                        })
                .when(mTabPersistentStore)
                .saveState();

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_TOTAL, 25)
                        .expectIntRecord(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_TOTAL_WARM,
                                25)
                        .expectNoRecords(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_TOTAL_COLD)
                        .build();

        assertTrue(BackgroundSessionTabRestorer.restoreSession(session));
        assertTrue(session.getTabDataList().isEmpty());
        verify(mTabPersistentStore).saveState();
        watcher.assertExpected();
    }

    @Test
    public void testRestoreSession_singleSessionDrained_exitsEarlyBeforeCheckingRemainingWindows() {
        TestChromeTabbedActivity firstCreated =
                Robolectric.buildActivity(TestChromeTabbedActivity.class).get();
        TestChromeTabbedActivity secondCreated =
                Robolectric.buildActivity(TestChromeTabbedActivity.class).get();
        ApplicationStatus.onStateChangeForTesting(firstCreated, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(firstCreated, ActivityState.STOPPED);
        ApplicationStatus.onStateChangeForTesting(secondCreated, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(secondCreated, ActivityState.STOPPED);

        // ApplicationStatus tracks activities in a HashMap; bind the first iterated activity as
        // window 1 so we deterministically verify the remaining activity is never inspected.
        List<Activity> runningActivities = ApplicationStatus.getRunningActivities();
        TestChromeTabbedActivity firstActivity =
                (TestChromeTabbedActivity) runningActivities.get(0);
        Activity remainingActivity = runningActivities.get(1);
        setupWarmActivityMocks(firstActivity, 1);

        stubLiveTabAttach();
        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.getCount()).thenReturn(1);

        BackgroundSession targetSession = new BackgroundSession(mTab, 500);
        targetSession.getTabDataList().get(0).setTabWindowId(1);
        addLiveTabToPool(mTab, Tab.INVALID_TAB_ID);

        assertTrue(BackgroundSessionTabRestorer.restoreSession(targetSession));

        assertTrue(targetSession.getTabDataList().isEmpty());
        verify(mTabPersistentStore).saveState();
        verify(mTabWindowManager, never()).getIdForWindow(remainingActivity);
    }

    @Test
    public void testRestoreSessions_multipleSessionsInSameWindow_flushesAndRecordsLatencyOnce() {
        TestChromeTabbedActivity activity =
                Robolectric.buildActivity(TestChromeTabbedActivity.class).get();
        setupWarmActivityMocks(activity, 1);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.STOPPED);

        Tab secondTab = mock(Tab.class);
        when(secondTab.getId()).thenReturn(200);
        stubLiveTabAttach(mTab);
        stubLiveTabAttach(secondTab);
        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.indexOf(secondTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.getCount()).thenReturn(1);

        BackgroundSession firstSession = new BackgroundSession(mTab, 500);
        firstSession.getTabDataList().get(0).setTabWindowId(1);
        addLiveTabToPool(mTab, Tab.INVALID_TAB_ID);

        BackgroundSession secondSession = new BackgroundSession(secondTab, 600);
        secondSession.getTabDataList().get(0).setTabWindowId(1);
        addLiveTabToPool(secondTab, Tab.INVALID_TAB_ID);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_TOTAL)
                        .expectAnyRecord(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_TOTAL_WARM)
                        .build();

        BackgroundSessionTabRestorer.restoreSessions(List.of(firstSession, secondSession));

        assertTrue(firstSession.getTabDataList().isEmpty());
        assertTrue(secondSession.getTabDataList().isEmpty());
        verify(mTab).updateAttachment(mWindowAndroid, mTabDelegateFactory);
        verify(secondTab).updateAttachment(mWindowAndroid, mTabDelegateFactory);
        verify(mTabPersistentStore, times(1)).saveState();
        watcher.assertExpected();
    }

    @Test
    public void testRestoreSession_ineligibleActivities_skipsRestoreAndDiskFlush() {
        TestAsyncInitializationActivity destroyedActivity =
                Robolectric.buildActivity(TestAsyncInitializationActivity.class).get();
        destroyedActivity.setDestroyedForTesting(true);
        Activity plainActivity = Robolectric.buildActivity(Activity.class).get();
        TestChromeTabbedActivity uninitializedActivity =
                Robolectric.buildActivity(TestChromeTabbedActivity.class).get();

        setupWarmActivityMocks(uninitializedActivity, 42);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);

        ApplicationStatus.onStateChangeForTesting(destroyedActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(plainActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(uninitializedActivity, ActivityState.CREATED);

        BackgroundSession session = new BackgroundSession(mTab, 500);

        assertFalse(BackgroundSessionTabRestorer.restoreSession(session));
        assertEquals(1, session.getTabDataList().size());
        verify(mTab, never()).updateAttachment(any(), any());
        verify(mTabPersistentStore, never()).saveState();
    }

    @Test
    public void testRestoreSessionsIntoWindow_coldStart_recordsWaitAndColdLatency() {
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

        int restoredCount =
                BackgroundSessionTabRestorer.restoreSessionsIntoWindow(
                        List.of(session),
                        mTabModelSelector,
                        42,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        /* waitDurationMs= */ 200,
                        /* isColdStart= */ true);

        assertEquals(1, restoredCount);
        assertTrue(session.getTabDataList().isEmpty());
        watcher.assertExpected();
    }
}
