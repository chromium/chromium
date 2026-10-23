// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.actor.background_tabs.BackgroundSession;
import org.chromium.chrome.browser.actor.background_tabs.OffscreenRenderingManager;
import org.chromium.chrome.browser.actor.foreground_service.ActorForegroundServiceController;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link ActorBackgroundActuationManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowSystemClock.class})
public class ActorBackgroundActuationManagerTest {
    // Subclass to supply a mock ActivityWindowAndroid without running
    // ChromeTabbedActivity#onCreate.
    private static class TestChromeTabbedActivity extends ChromeTabbedActivity {
        private ActivityWindowAndroid mWindowAndroid;

        @Override
        public ActivityWindowAndroid getWindowAndroid() {
            return mWindowAndroid;
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String MESSAGE_ID_SUCCESS = "message_id_success";
    private static final String MESSAGE_ID_FAIL = "message_id_fail";
    private static final String MESSAGE_ID_CRASH = "message_id_crash";
    private static final String MESSAGE_ID_CANCELLED = "message_id_cancelled";
    private static final String TEST_URL = "about:blank";

    private static final int TAB_ID = 100;

    @Mock private ProfileResolver.Natives mProfileResolverNatives;
    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorKeyedService;
    @Mock private OffscreenRenderingManager mOffscreenRenderingManager;
    @Mock private ActivityWindowAndroid mWindowAndroid;
    @Mock private Tab mTab;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private TabRemover mTabRemover;
    @Mock private TabCreator mTabCreator;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private TabDelegateFactory mTabDelegateFactory;
    @Mock private Tab mPlaceholderTab;
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private TabModelOrchestrator mTabModelOrchestrator;
    @Mock private TabPersistentStore mTabPersistentStore;

    private TestChromeTabbedActivity mActivity;
    private ActorBackgroundActuationManager mManager;

    @Before
    public void setUp() {
        MultiWindowTestUtils.ensureInitialized();
        mActivity = Robolectric.buildActivity(TestChromeTabbedActivity.class).get();
        ProfileResolverJni.setInstanceForTesting(mProfileResolverNatives);
        when(mProfileResolverNatives.tokenizeProfile(any())).thenReturn("mock_token");

        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);
        OffscreenRenderingManager.setInstanceForTesting(mOffscreenRenderingManager);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);

        when(mOffscreenRenderingManager.getOffscreenWindow()).thenReturn(mWindowAndroid);
        TabBuilder.setTabForTesting(mTab);
        when(mTab.getId()).thenReturn(TAB_ID);

        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);

        ProfileManager.setLastUsedProfileForTesting(mProfile);
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        mManager = new ActorBackgroundActuationManager();
    }

    @After
    public void tearDown() {
        ActorMetrics.resetForTesting();
        ActorForegroundServiceController.setInstanceForTesting(null);
        ProfileResolverJni.setInstanceForTesting(null);
        ProfileManager.resetForTesting();
        BackgroundTabPoolManager.resetForTesting();
        ActorKeyedServiceFactory.setForTesting(null);
        OffscreenRenderingManager.setInstanceForTesting(null);
        ApplicationStatus.destroyForJUnitTests();
        TabBuilder.setTabForTesting(null);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(null);
        TabStateExtractor.resetTabStatesForTesting();
        MultiWindowTestUtils.resetInstanceInfo();
    }

    private void setupWarmActivityMocks() {
        mActivity.mWindowAndroid = mWindowAndroid;
        mActivity.setTabModelOrchestratorForTesting(mTabModelOrchestrator);
        when(mTabModelOrchestrator.getTabPersistentStore()).thenReturn(mTabPersistentStore);

        when(mTabWindowManager.getIdForWindow(mActivity)).thenReturn(42);
        when(mTabWindowManager.getTabModelSelectorById(42)).thenReturn(mTabModelSelector);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.getTabCreatorManager()).thenReturn(mTabCreatorManager);
        when(mTabCreatorManager.getTabCreator(false)).thenReturn(mTabCreator);
        when(mTabCreator.createDefaultTabDelegateFactory()).thenReturn(mTabDelegateFactory);
    }

    private void setupTransitionMocks() {
        when(mTabWindowManager.getWindowIdForSelector(mTabModelSelector)).thenReturn(42);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        when(mTabModel.indexOf(mTab)).thenReturn(0);
        when(mTabModel.iterator()).thenAnswer(inv -> Collections.singletonList(mTab).iterator());

        when(mPlaceholderTab.getId()).thenReturn(101);
        when(mTabCreator.createFrozenTab(any(), anyInt(), anyInt())).thenReturn(mPlaceholderTab);
        TabStateExtractor.setTabStateForTesting(TAB_ID, new TabState());
    }

    private void triggerPageLoadFinished() {
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab, atLeastOnce()).addObserver(captor.capture());
        captor.getValue().onPageLoadFinished(mTab, new GURL(TEST_URL));
    }

    @Test
    public void testStartBackgroundActuation_Success() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Actor.BackgroundActuation.Trigger",
                                ActorMetrics.ActorBackgroundActuationTrigger.TASK_INIT_BY_FCM)
                        .expectIntRecord(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_TAB_CREATED_IN_BACKGROUND,
                                ActorMetrics.ActorBackgroundTabCreationStatus
                                        .TASK_INIT_BY_FCM_SUCCESS)
                        .build();
        mManager.startBackgroundActuation(mProfile, MESSAGE_ID_SUCCESS);

        // Verify offscreen rendering started
        verify(mOffscreenRenderingManager).startOffscreenRendering(eq(mTab), anyInt(), anyInt());

        // Capture and trigger page load success
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        observer.onPageLoadFinished(mTab, new GURL(TEST_URL));
        watcher.assertExpected();

        // Verify the tab was prepared and set on ActorKeyedService
        verify(mActorKeyedService).setTabReady(mTab, MESSAGE_ID_SUCCESS);
        verify(mActorKeyedService, never()).notifyBackgroundSetupFailed(any());

        // Verify the provisioned background tab was registered in BackgroundTabPool
        BackgroundTabPool pool = BackgroundTabPoolManager.acquire(mProfile);
        try {
            assertNotNull(pool.getLiveTab(TAB_ID));
        } finally {
            BackgroundTabPoolManager.release(pool);
        }
    }

    @Test
    public void testStartBackgroundActuation_PageLoadFailed() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Actor.BackgroundActuation.Trigger")
                        .expectIntRecord(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_TAB_CREATED_IN_BACKGROUND,
                                ActorMetrics.ActorBackgroundTabCreationStatus
                                        .TASK_INIT_BY_FCM_FAILURE)
                        .build();
        mManager.startBackgroundActuation(mProfile, MESSAGE_ID_FAIL);

        // Capture observer and trigger page load failure
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        observer.onPageLoadFailed(mTab, 404);
        watcher.assertExpected();

        // Verify setup failed notification was sent to native
        verify(mActorKeyedService).notifyBackgroundSetupFailed(MESSAGE_ID_FAIL);
        // Verify cleanup stopped offscreen rendering and destroyed tab
        verify(mTab).removeObserver(observer);
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
        verify(mTab).destroy();
    }

    @Test
    public void testStartBackgroundActuation_Crash() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Actor.BackgroundActuation.Trigger")
                        .expectIntRecord(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_TAB_CREATED_IN_BACKGROUND,
                                ActorMetrics.ActorBackgroundTabCreationStatus
                                        .TASK_INIT_BY_FCM_FAILURE)
                        .build();
        mManager.startBackgroundActuation(mProfile, MESSAGE_ID_CRASH);

        // Capture observer and trigger renderer crash
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        observer.onCrash(mTab);
        watcher.assertExpected();

        // Verify setup failed and cleanup was executed
        verify(mActorKeyedService).notifyBackgroundSetupFailed(MESSAGE_ID_CRASH);
        verify(mTab).removeObserver(observer);
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
        verify(mTab).destroy();
    }

    @Test
    public void testStartBackgroundActuation_CancelledBeforeLoadFinished() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Actor.BackgroundActuation.Trigger")
                        .expectNoRecords(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_TAB_CREATED_IN_BACKGROUND)
                        .build();
        mManager.startBackgroundActuation(mProfile, MESSAGE_ID_CANCELLED);

        // Capture observer
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        // Cancel/Cleanup before load finished
        mManager.cleanupContext(MESSAGE_ID_CANCELLED);

        // Verify cleanup stopped offscreen rendering and destroyed tab directly
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
        verify(mTab).destroy();
        assertEquals(0, mManager.getBackgroundSessions().size());

        // Now trigger page load finish
        observer.onPageLoadFinished(mTab, new GURL(TEST_URL));
        watcher.assertExpected();

        // Verify setTabReady was NOT called because session was cleaned up
        verify(mActorKeyedService, never()).setTabReady(any(), any());
    }

    @Test
    public void testTransitionActiveTasksToBackground() {
        setupTransitionMocks();

        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(123);

        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(123);
        when(task.isUnderActorControl()).thenReturn(true);
        when(task.getTabs()).thenReturn(Collections.singleton(100));
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(task));
        when(mActorKeyedService.getTask(123)).thenReturn(task);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.BackgroundActuation.Trigger",
                        ActorMetrics.ActorBackgroundActuationTrigger.TASK_INIT_BY_CHROME);
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        mManager.transitionActiveTasksToBackground(mTabModelSelector);
        watcher.assertExpected();

        // Verify offscreen rendering was started for the transitioned tab
        verify(mOffscreenRenderingManager).startOffscreenRendering(eq(mTab), anyInt(), anyInt());

        // Verify the transitioned tab was ingested into BackgroundTabPool
        BackgroundTabPool pool = BackgroundTabPoolManager.acquire(mProfile);
        try {
            assertNotNull(pool.getLiveTab(100));
            assertTrue(pool.hasPlaceholder(101));
        } finally {
            BackgroundTabPoolManager.release(pool);
        }

        // Verify the transitioned session is tracked
        mManager.destroy();
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);

        TabStateExtractor.resetTabStatesForTesting();
    }

    @Test
    public void testTransitionActiveTasksToBackground_DuplicateTransition_OnlyRecordsOnce() {
        setupTransitionMocks();
        TabStateExtractor.setTabStateForTesting(100, new TabState());

        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(mTab);
        when(mTabModel.iterator()).thenAnswer(inv -> Collections.singletonList(mTab).iterator());

        when(mTab.getId()).thenReturn(100);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(123);

        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(123);
        when(task.getTabs()).thenReturn(Collections.singleton(100));
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(task));
        when(mActorKeyedService.getTask(123)).thenReturn(task);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.BackgroundActuation.Trigger",
                        ActorMetrics.ActorBackgroundActuationTrigger.TASK_INIT_BY_CHROME);

        // First transition records the metric.
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        mManager.transitionActiveTasksToBackground(mTabModelSelector);

        // Second transition for the same task should be deduplicated and not record again.
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        mManager.transitionActiveTasksToBackground(mTabModelSelector);

        watcher.assertExpected();

        TabStateExtractor.resetTabStatesForTesting();
    }

    @Test
    public void
            testTransitionActiveTasksToBackground_FcmInitiatedTask_DoesNotRecordChromeTrigger() {
        setupTransitionMocks();
        TabStateExtractor.setTabStateForTesting(100, new TabState());
        when(mTab.getId()).thenReturn(100);
        when(mTab.hasParentCollection()).thenReturn(false);
        when(mTab.isDestroyed()).thenReturn(false);
        when(mTab.isOffTheRecord()).thenReturn(false);

        // 1. Start background actuation via FCM and complete page load.
        var fcmWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.BackgroundActuation.Trigger",
                        ActorMetrics.ActorBackgroundActuationTrigger.TASK_INIT_BY_FCM);
        mManager.startBackgroundActuation(mProfile, MESSAGE_ID_SUCCESS);
        triggerPageLoadFinished();
        fcmWatcher.assertExpected();
        assertEquals(1, mManager.getBackgroundSessions().size());

        // 2. Restore the FCM-initiated tab to the foreground window.
        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        List<BackgroundSession> restored =
                ActorBackgroundActuationManager.restoreActiveWindowBackgroundTabsForTesting(
                        mTabModelSelector,
                        42,
                        mWindowAndroid,
                        mManager.getBackgroundSessions(),
                        mTabDelegateFactory);
        mManager.removeBackgroundSessions(restored);
        assertEquals(0, mManager.getBackgroundSessions().size());

        // 3. Subsequently transition active tasks back to the background.
        when(mTabModel.indexOf(mTab)).thenReturn(0);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(mTab);
        when(mTabModel.iterator()).thenAnswer(inv -> Collections.singletonList(mTab).iterator());

        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(123);
        when(task.isUnderActorControl()).thenReturn(true);
        when(task.getTabs()).thenReturn(Collections.singleton(100));
        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(123);
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(task));
        when(mActorKeyedService.getTask(123)).thenReturn(task);

        var transitionWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Actor.BackgroundActuation.Trigger")
                        .build();
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        mManager.transitionActiveTasksToBackground(mTabModelSelector);
        transitionWatcher.assertExpected();

        // 4. Restore the tab to foreground, complete the FCM task, and verify a subsequent
        // non-FCM task on the same tab records TASK_INIT_BY_CHROME.
        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        List<BackgroundSession> restoredAgain =
                ActorBackgroundActuationManager.restoreActiveWindowBackgroundTabsForTesting(
                        mTabModelSelector,
                        42,
                        mWindowAndroid,
                        mManager.getBackgroundSessions(),
                        mTabDelegateFactory);
        mManager.removeBackgroundSessions(restoredAgain);
        mManager.onTaskCompleted(123);
        ActorMetrics.getInstance().onTaskStateChangedForTesting(123, ActorTaskState.FINISHED);

        ActorTask newTask = mock(ActorTask.class);
        when(newTask.getId()).thenReturn(456);
        when(newTask.isUnderActorControl()).thenReturn(true);
        when(newTask.getTabs()).thenReturn(Collections.singleton(100));
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(456);
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(newTask));
        when(mActorKeyedService.getTask(456)).thenReturn(newTask);
        when(mTabModel.indexOf(mTab)).thenReturn(0);
        when(mTabModel.iterator()).thenAnswer(inv -> Collections.singletonList(mTab).iterator());

        var newChromeTaskWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.BackgroundActuation.Trigger",
                        ActorMetrics.ActorBackgroundActuationTrigger.TASK_INIT_BY_CHROME);
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        mManager.transitionActiveTasksToBackground(mTabModelSelector);
        newChromeTaskWatcher.assertExpected();

        TabStateExtractor.resetTabStatesForTesting();
    }

    @Test
    public void testOnTaskCompleted_FcmInitiatedTaskCompletedInForeground_CleansUpTabForReuse() {
        setupTransitionMocks();
        TabStateExtractor.setTabStateForTesting(100, new TabState());
        when(mTab.getId()).thenReturn(100);
        when(mTab.hasParentCollection()).thenReturn(false);
        when(mTab.isDestroyed()).thenReturn(false);
        when(mTab.isOffTheRecord()).thenReturn(false);

        // 1. Start background actuation via FCM and complete page load.
        mManager.startBackgroundActuation(mProfile, MESSAGE_ID_SUCCESS);
        triggerPageLoadFinished();
        assertEquals(1, mManager.getBackgroundSessions().size());

        // 2. Restore the FCM-initiated tab to the foreground window.
        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        List<BackgroundSession> restored =
                ActorBackgroundActuationManager.restoreActiveWindowBackgroundTabsForTesting(
                        mTabModelSelector,
                        42,
                        mWindowAndroid,
                        mManager.getBackgroundSessions(),
                        mTabDelegateFactory);
        mManager.removeBackgroundSessions(restored);
        assertEquals(0, mManager.getBackgroundSessions().size());

        // 3. The FCM task finishes while running in the foreground (never transitioned back to bg).
        ActorTask fcmTask = mock(ActorTask.class);
        when(fcmTask.getId()).thenReturn(123);
        when(fcmTask.getTabs()).thenReturn(Collections.emptySet());
        when(fcmTask.getLastActuatedTabId()).thenReturn(100);
        when(mActorKeyedService.getTask(123)).thenReturn(fcmTask);
        mManager.onTaskCompleted(123);

        // 4. Launch a new non-FCM foreground task on the reused tab and transition to background.
        when(mTabModel.indexOf(mTab)).thenReturn(0);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(mTab);
        when(mTabModel.iterator()).thenAnswer(inv -> Collections.singletonList(mTab).iterator());

        ActorTask newTask = mock(ActorTask.class);
        when(newTask.getId()).thenReturn(456);
        when(newTask.isUnderActorControl()).thenReturn(true);
        when(newTask.getTabs()).thenReturn(Collections.singleton(100));
        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(456);
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(newTask));
        when(mActorKeyedService.getTask(456)).thenReturn(newTask);

        var chromeWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.BackgroundActuation.Trigger",
                        ActorMetrics.ActorBackgroundActuationTrigger.TASK_INIT_BY_CHROME);
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        mManager.transitionActiveTasksToBackground(mTabModelSelector);
        chromeWatcher.assertExpected();

        TabStateExtractor.resetTabStatesForTesting();
    }

    @Test
    public void testStartBackgroundActuation_TabDestroyed_CleansUpFcmInitiatedTabId() {
        setupTransitionMocks();
        TabStateExtractor.setTabStateForTesting(100, new TabState());
        when(mTab.getId()).thenReturn(100);

        mManager.startBackgroundActuation(mProfile, MESSAGE_ID_SUCCESS);
        triggerPageLoadFinished();

        // Notify all observers attached to the prepared FCM tab of onDestroyed.
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab, atLeastOnce()).addObserver(captor.capture());
        for (TabObserver observer : captor.getAllValues()) {
            observer.onDestroyed(mTab);
        }

        // If a new tab with the same ID is later used for a Chrome-initiated task, it should
        // record TASK_INIT_BY_CHROME.
        when(mTabModel.indexOf(mTab)).thenReturn(0);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(mTab);
        when(mTabModel.iterator()).thenAnswer(inv -> Collections.singletonList(mTab).iterator());

        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(789);
        when(task.isUnderActorControl()).thenReturn(true);
        when(task.getTabs()).thenReturn(Collections.singleton(100));
        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(789);
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(task));
        when(mActorKeyedService.getTask(789)).thenReturn(task);

        var chromeWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.BackgroundActuation.Trigger",
                        ActorMetrics.ActorBackgroundActuationTrigger.TASK_INIT_BY_CHROME);
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        mManager.transitionActiveTasksToBackground(mTabModelSelector);
        chromeWatcher.assertExpected();

        TabStateExtractor.resetTabStatesForTesting();
    }

    @Test
    public void testTransitionActiveTasksToBackground_NoActiveTasks_NoMetricRecorded() {
        when(mActorKeyedService.getActiveTasksCount()).thenReturn(0);
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.emptyList());

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Actor.BackgroundActuation.Trigger")
                        .build();

        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        mManager.transitionActiveTasksToBackground(mTabModelSelector);

        watcher.assertExpected();
    }

    @Test
    public void testTransitionActiveTasksToBackground_MultipleTabs() {
        setupTransitionMocks();
        TabStateExtractor.setTabStateForTesting(200, new TabState());

        Tab tab2 = mock(Tab.class);
        when(tab2.getId()).thenReturn(200);
        when(mTabModel.indexOf(tab2)).thenReturn(1);

        when(mTabModel.iterator()).thenAnswer(inv -> Arrays.asList(mTab, tab2).iterator());

        Tab placeholderTab2 = mock(Tab.class);
        when(placeholderTab2.getId()).thenReturn(201);
        when(mTabCreator.createFrozenTab(any(), anyInt(), anyInt()))
                .thenReturn(mPlaceholderTab, placeholderTab2);

        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(123);
        when(mActorKeyedService.getActiveTaskIdOnTab(200, false)).thenReturn(123);

        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(123);
        when(task.isUnderActorControl()).thenReturn(true);
        when(task.getTabs()).thenReturn(new LinkedHashSet<>(Arrays.asList(100, 200)));
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(task));

        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        mManager.transitionActiveTasksToBackground(mTabModelSelector);

        // Verify offscreen rendering is started for both tabs
        verify(mOffscreenRenderingManager).startOffscreenRendering(eq(mTab), anyInt(), anyInt());
        verify(mOffscreenRenderingManager).startOffscreenRendering(eq(tab2), anyInt(), anyInt());

        // Verify both transitioned tabs were ingested into BackgroundTabPool
        BackgroundTabPool pool2 = BackgroundTabPoolManager.acquire(mProfile);
        try {
            assertNotNull(pool2.getLiveTab(100));
            assertNotNull(pool2.getLiveTab(200));
        } finally {
            BackgroundTabPoolManager.release(pool2);
        }

        mManager.destroy();
        verify(mOffscreenRenderingManager).stopOffscreenRendering(tab2);

        TabStateExtractor.resetTabStatesForTesting();
    }

    @Test
    public void testProvisionBackgroundTabForTask_Success() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ActorMetrics.ACTOR_BACKGROUND_ACTUATION_TAB_CREATED_IN_BACKGROUND,
                        ActorMetrics.ActorBackgroundTabCreationStatus.TASK_INIT_BY_CHROME_SUCCESS);
        @SuppressWarnings("unchecked")
        Callback<Tab> callback = mock(Callback.class);
        mManager.provisionBackgroundTabForTask(mProfile, 123, callback);

        // Verify offscreen rendering started
        verify(mOffscreenRenderingManager).startOffscreenRendering(eq(mTab), anyInt(), anyInt());

        // Capture and trigger page load success
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        observer.onPageLoadFinished(mTab, new GURL(TEST_URL));
        watcher.assertExpected();

        // Verify callback invoked with tab
        verify(callback).onResult(mTab);

        // Verify the provisioned background tab was registered in BackgroundTabPool
        BackgroundTabPool pool = BackgroundTabPoolManager.acquire(mProfile);
        try {
            assertNotNull(pool.getLiveTab(TAB_ID));
        } finally {
            BackgroundTabPoolManager.release(pool);
        }
    }

    @Test
    public void testProvisionBackgroundTabForTask_ExistingSession() {
        setupTransitionMocks();

        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(123);
        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(123);
        when(task.isUnderActorControl()).thenReturn(true);
        when(task.getTabs()).thenReturn(Collections.singleton(100));
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(task));

        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        mManager.transitionActiveTasksToBackground(mTabModelSelector);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ActorMetrics.ACTOR_BACKGROUND_ACTUATION_TAB_CREATED_IN_BACKGROUND,
                        ActorMetrics.ActorBackgroundTabCreationStatus.TASK_INIT_BY_CHROME_SUCCESS);
        @SuppressWarnings("unchecked")
        Callback<Tab> callback = mock(Callback.class);
        mManager.provisionBackgroundTabForTask(mProfile, 123, callback);

        verify(mOffscreenRenderingManager, atLeastOnce())
                .startOffscreenRendering(eq(mTab), anyInt(), anyInt());

        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab, atLeastOnce()).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        observer.onPageLoadFinished(mTab, new GURL(TEST_URL));
        watcher.assertExpected();
        verify(callback).onResult(mTab);
    }

    @Test
    public void testProvisionBackgroundTabForTask_PageLoadFailed() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ActorMetrics.ACTOR_BACKGROUND_ACTUATION_TAB_CREATED_IN_BACKGROUND,
                        ActorMetrics.ActorBackgroundTabCreationStatus.TASK_INIT_BY_CHROME_FAILURE);
        @SuppressWarnings("unchecked")
        Callback<Tab> callback = mock(Callback.class);
        mManager.provisionBackgroundTabForTask(mProfile, 123, callback);

        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab, atLeastOnce()).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        observer.onPageLoadFailed(mTab, -1);
        watcher.assertExpected();

        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
        verify(mTab).destroy();
        verify(callback).onResult(null);
    }

    @Test
    public void testProvisionBackgroundTabForTask_Crash() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ActorMetrics.ACTOR_BACKGROUND_ACTUATION_TAB_CREATED_IN_BACKGROUND,
                        ActorMetrics.ActorBackgroundTabCreationStatus.TASK_INIT_BY_CHROME_FAILURE);
        @SuppressWarnings("unchecked")
        Callback<Tab> callback = mock(Callback.class);
        mManager.provisionBackgroundTabForTask(mProfile, 123, callback);

        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab, atLeastOnce()).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        observer.onCrash(mTab);
        watcher.assertExpected();

        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
        verify(mTab).destroy();
        verify(callback).onResult(null);
    }

    @Test
    public void testCleanupContext_StopsRendering_SavesState() {
        TabState testTabState = new TabState();
        TabStateExtractor.setTabStateForTesting(999, testTabState);
        when(mTab.getId()).thenReturn(999);
        when(mTab.isIncognito()).thenReturn(false);

        // Setup a background session with a triggering message ID
        mManager.startBackgroundActuation(mProfile, "test_msg_id");

        // Verify offscreen rendering started
        verify(mOffscreenRenderingManager).startOffscreenRendering(eq(mTab), anyInt(), anyInt());

        // Capture observer and complete page load to ingest session into BackgroundTabPool
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab).addObserver(captor.capture());
        captor.getValue().onPageLoadFinished(mTab, new GURL(TEST_URL));

        // Call cleanupContext on the ingested session
        mManager.cleanupContext("test_msg_id");

        // Verify offscreen rendering stopped
        verify(mOffscreenRenderingManager, atLeastOnce()).stopOffscreenRendering(mTab);

        // Verify the ingested session is retained in mBackgroundSessions when no warm activity was alive
        assertEquals(1, mManager.getBackgroundSessions().size());

        TabStateExtractor.resetTabStatesForTesting();
    }

    @Test
    public void testDestroy_WarmActivity_RestoresTabsBeforeClear() {
        setupWarmActivityMocks();
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        mManager.startBackgroundActuation(mProfile, "msg_warm_test");
        triggerPageLoadFinished();
        assertEquals(1, mManager.getBackgroundSessions().size());

        mManager.destroy();

        verify(mTab).updateAttachment(eq(mWindowAndroid), any());
        verify(mTabPersistentStore).saveState();
        assertEquals(0, mManager.getBackgroundSessions().size());
    }

    @Test
    public void testDestroy_TabStateNotInitialized_SkipsRestoration() {
        setupWarmActivityMocks();
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        mManager.startBackgroundActuation(mProfile, "msg_uninit_test");
        triggerPageLoadFinished();
        assertEquals(1, mManager.getBackgroundSessions().size());

        mManager.destroy();

        verify(mTab, never()).updateAttachment(any(), any());
        verify(mOffscreenRenderingManager, atLeastOnce()).stopOffscreenRendering(mTab);
        verify(mTabPersistentStore, never()).saveState();
        assertEquals(0, mManager.getBackgroundSessions().size());
    }

    @Test
    public void testDestroy_ActivityFinishingOrDestroyed_SkipsRestoration() {
        mActivity.finish();
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        mManager.startBackgroundActuation(mProfile, "msg_finishing_test");
        triggerPageLoadFinished();
        assertEquals(1, mManager.getBackgroundSessions().size());

        mManager.destroy();

        verify(mTab, never()).updateAttachment(any(), any());
        verify(mOffscreenRenderingManager, atLeastOnce()).stopOffscreenRendering(mTab);
        verify(mTabPersistentStore, never()).saveState();
        assertEquals(0, mManager.getBackgroundSessions().size());
    }

    @Test
    public void testDestroy_MixedWarmAndColdActivities() {
        setupWarmActivityMocks();

        AsyncInitializationActivity coldActivity =
                Robolectric.buildActivity(TestChromeTabbedActivity.class).get();
        TabModelSelector coldSelector = mock(TabModelSelector.class);
        coldActivity.finish();
        when(mTabWindowManager.getIdForWindow(coldActivity)).thenReturn(84);
        when(mTabWindowManager.getTabModelSelectorById(84)).thenReturn(coldSelector);

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(coldActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        ApplicationStatus.onStateChangeForTesting(coldActivity, ActivityState.STOPPED);

        mManager.startBackgroundActuation(mProfile, "msg_mixed_test");
        triggerPageLoadFinished();

        mManager.destroy();

        verify(mTab).updateAttachment(eq(mWindowAndroid), any());
        verify(mTabPersistentStore).saveState();
        assertEquals(0, mManager.getBackgroundSessions().size());
    }

    @Test
    public void testCleanupContext_WarmActivity_RestoresTabsBeforeStop() {
        setupWarmActivityMocks();
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        mManager.startBackgroundActuation(mProfile, "msg_cleanup_warm");
        triggerPageLoadFinished();
        assertEquals(1, mManager.getBackgroundSessions().size());

        mManager.cleanupContext("msg_cleanup_warm");

        verify(mTab).updateAttachment(eq(mWindowAndroid), any());
        verify(mTabPersistentStore).saveState();
        assertEquals(0, mManager.getBackgroundSessions().size());
    }

    @Test
    public void testDestroy_NonAsyncInitializationActivity_SkipsRestoration() {
        Activity nonTabbedActivity = Robolectric.buildActivity(Activity.class).get();
        ApplicationStatus.onStateChangeForTesting(nonTabbedActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(nonTabbedActivity, ActivityState.STOPPED);

        mManager.startBackgroundActuation(mProfile, "msg_non_tabbed_test");
        triggerPageLoadFinished();
        assertEquals(1, mManager.getBackgroundSessions().size());

        mManager.destroy();

        verify(mTab, never()).updateAttachment(any(), any());
        verify(mOffscreenRenderingManager, atLeastOnce()).stopOffscreenRendering(mTab);
        verify(mTabPersistentStore, never()).saveState();
        assertEquals(0, mManager.getBackgroundSessions().size());
    }

    @Test
    public void testOnTaskCompleted_WarmActivity_RestoresTabsBeforeStop() {
        setupTransitionMocks();
        setupWarmActivityMocks();
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(777);
        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(777);
        when(task.isUnderActorControl()).thenReturn(true);
        when(task.getTabs()).thenReturn(Collections.singleton(100));
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(task));

        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        mManager.transitionActiveTasksToBackground(mTabModelSelector);
        assertEquals(1, mManager.getBackgroundSessions().size());

        mManager.onTaskCompleted(777);

        verify(mTab).updateAttachment(eq(mWindowAndroid), any());
        verify(mTabPersistentStore).saveState();
        assertEquals(0, mManager.getBackgroundSessions().size());
    }

    @Test
    public void testOnTaskCompleted_NoSession_NoOp() {
        mManager.onTaskCompleted(999);
        verify(mTab, never()).updateAttachment(any(), any());
    }

    @Test
    public void testDestroy_DefaultDelegateFactoryNull_SkipsRestoration() {
        setupWarmActivityMocks();
        when(mTabCreator.createDefaultTabDelegateFactory()).thenReturn(null);

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        mManager.startBackgroundActuation(mProfile, "msg_null_factory");
        triggerPageLoadFinished();
        assertEquals(1, mManager.getBackgroundSessions().size());

        mManager.destroy();

        verify(mTab, never()).updateAttachment(any(), any());
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
        assertEquals(0, mManager.getBackgroundSessions().size());
    }

    @Test
    public void testRestoreActiveWindowBackgroundTabs_MatchingWindow() {
        when(mTab.getId()).thenReturn(100);
        when(mTab.hasParentCollection()).thenReturn(false);
        when(mTab.isDestroyed()).thenReturn(false);
        when(mTab.isOffTheRecord()).thenReturn(false);

        when(mPlaceholderTab.getId()).thenReturn(101);
        when(mPlaceholderTab.isDestroyed()).thenReturn(false);

        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.getTabById(101)).thenReturn(mPlaceholderTab);
        when(mTabModel.indexOf(mPlaceholderTab)).thenReturn(0);

        BackgroundSession session = new BackgroundSession(mTab, 500);
        BackgroundSession.BackgroundTabData tabData = session.getTabDataList().get(0);
        tabData.setTabWindowId(1);
        tabData.setPlaceholderTabId(101);

        BackgroundTabPool pool = BackgroundTabPoolManager.acquire(mProfile);
        try {
            LiveBackgroundTab liveTab =
                    new LiveBackgroundTab(
                            pool,
                            mTab,
                            101,
                            500,
                            /* originalTabIndex= */ TabModel.INVALID_TAB_INDEX);
            pool.addLiveTab(liveTab);
        } finally {
            BackgroundTabPoolManager.release(pool);
        }

        List<BackgroundSession> sessions = new ArrayList<>(Collections.singletonList(session));
        List<BackgroundSession> restored =
                ActorBackgroundActuationManager.restoreActiveWindowBackgroundTabsForTesting(
                        mTabModelSelector, 1, mWindowAndroid, sessions, mTabDelegateFactory);

        assertEquals(1, restored.size());
        assertEquals(session, restored.get(0));
        assertTrue(session.getTabDataList().isEmpty());
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
        verify(mTab).updateAttachment(mWindowAndroid, mTabDelegateFactory);
        verify(mTabRemover).removeTab(mPlaceholderTab, false);
    }

    @Test
    public void testRestoreActiveWindowBackgroundTabs_NonMatchingWindow() {
        MultiWindowTestUtils.enableMultiInstance();
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 2,
                /* url= */ "https://www.example.com",
                /* tabCount= */ 1,
                /* taskId= */ 57);

        when(mTab.getId()).thenReturn(100);
        when(mTab.hasParentCollection()).thenReturn(false);
        when(mTab.isDestroyed()).thenReturn(false);
        when(mTab.isOffTheRecord()).thenReturn(false);

        BackgroundSession session = new BackgroundSession(mTab, 500);
        BackgroundSession.BackgroundTabData tabData = session.getTabDataList().get(0);
        tabData.setTabWindowId(2);
        tabData.setPlaceholderTabId(101);

        BackgroundTabPool pool = BackgroundTabPoolManager.acquire(mProfile);
        try {
            LiveBackgroundTab liveTab =
                    new LiveBackgroundTab(
                            pool,
                            mTab,
                            101,
                            500,
                            /* originalTabIndex= */ TabModel.INVALID_TAB_INDEX);
            pool.addLiveTab(liveTab);
        } finally {
            BackgroundTabPoolManager.release(pool);
        }

        List<BackgroundSession> sessions = new ArrayList<>(Collections.singletonList(session));
        List<BackgroundSession> restored =
                ActorBackgroundActuationManager.restoreActiveWindowBackgroundTabsForTesting(
                        mTabModelSelector, 1, mWindowAndroid, sessions, mTabDelegateFactory);

        assertTrue(restored.isEmpty());
        assertEquals(1, session.getTabDataList().size());
        assertEquals(mTab, session.getTabDataList().get(0).getTab());
        verify(mTab, never()).updateAttachment(any(), any());
    }

    @Test
    public void testRestoreActiveWindowBackgroundTabs_ClosedWindow_RestoresIntoActiveWindow() {
        MultiWindowTestUtils.enableMultiInstance();

        when(mTab.getId()).thenReturn(100);
        when(mTab.hasParentCollection()).thenReturn(false);
        when(mTab.isDestroyed()).thenReturn(false);
        when(mTab.isOffTheRecord()).thenReturn(false);

        when(mPlaceholderTab.getId()).thenReturn(101);
        when(mPlaceholderTab.isDestroyed()).thenReturn(false);

        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.getTabById(101)).thenReturn(mPlaceholderTab);
        when(mTabModel.indexOf(mPlaceholderTab)).thenReturn(0);

        // Window 2 was closed while the tab was detached, so no instance is persisted for it.
        BackgroundSession session = new BackgroundSession(mTab, 500);
        BackgroundSession.BackgroundTabData tabData = session.getTabDataList().get(0);
        tabData.setTabWindowId(2);
        tabData.setPlaceholderTabId(101);

        BackgroundTabPool pool = BackgroundTabPoolManager.acquire(mProfile);
        try {
            LiveBackgroundTab liveTab =
                    new LiveBackgroundTab(
                            pool,
                            mTab,
                            101,
                            500,
                            /* originalTabIndex= */ TabModel.INVALID_TAB_INDEX);
            pool.addLiveTab(liveTab);
        } finally {
            BackgroundTabPoolManager.release(pool);
        }

        List<BackgroundSession> sessions = new ArrayList<>(Collections.singletonList(session));
        List<BackgroundSession> restored =
                ActorBackgroundActuationManager.restoreActiveWindowBackgroundTabsForTesting(
                        mTabModelSelector, 1, mWindowAndroid, sessions, mTabDelegateFactory);

        assertEquals(
                "A tab whose window no longer exists should not be stranded.", 1, restored.size());
        assertTrue(session.getTabDataList().isEmpty());
        verify(mTab).updateAttachment(mWindowAndroid, mTabDelegateFactory);
    }

    @Test
    public void testRestoreActiveWindowBackgroundTabs_InactiveWindow_RestoresIntoActiveWindow() {
        MultiWindowTestUtils.enableMultiInstance();
        // Window 2 is still persisted, but its task is gone from Android Recents.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 2,
                /* url= */ "https://www.example.com",
                /* tabCount= */ 1,
                /* taskId= */ -1);

        when(mTab.getId()).thenReturn(100);
        when(mTab.hasParentCollection()).thenReturn(false);
        when(mTab.isDestroyed()).thenReturn(false);
        when(mTab.isOffTheRecord()).thenReturn(false);

        when(mPlaceholderTab.getId()).thenReturn(101);
        when(mPlaceholderTab.isDestroyed()).thenReturn(false);

        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.getTabById(101)).thenReturn(mPlaceholderTab);
        when(mTabModel.indexOf(mPlaceholderTab)).thenReturn(0);

        BackgroundSession session = new BackgroundSession(mTab, 500);
        BackgroundSession.BackgroundTabData tabData = session.getTabDataList().get(0);
        tabData.setTabWindowId(2);
        tabData.setPlaceholderTabId(101);

        BackgroundTabPool pool = BackgroundTabPoolManager.acquire(mProfile);
        try {
            LiveBackgroundTab liveTab =
                    new LiveBackgroundTab(
                            pool,
                            mTab,
                            101,
                            500,
                            /* originalTabIndex= */ TabModel.INVALID_TAB_INDEX);
            pool.addLiveTab(liveTab);
        } finally {
            BackgroundTabPoolManager.release(pool);
        }

        List<BackgroundSession> sessions = new ArrayList<>(Collections.singletonList(session));
        List<BackgroundSession> restored =
                ActorBackgroundActuationManager.restoreActiveWindowBackgroundTabsForTesting(
                        mTabModelSelector, 1, mWindowAndroid, sessions, mTabDelegateFactory);

        assertEquals(
                "A tab whose window has no live task should not be stranded.", 1, restored.size());
        assertTrue(session.getTabDataList().isEmpty());
        verify(mTab).updateAttachment(mWindowAndroid, mTabDelegateFactory);
    }

    @Test
    public void testRestoreActiveWindowBackgroundTabs_EvictedTabCleanup() {
        when(mTab.getId()).thenReturn(100);
        when(mPlaceholderTab.getId()).thenReturn(101);
        when(mTabModel.getTabById(101)).thenReturn(mPlaceholderTab);

        BackgroundSession session = new BackgroundSession(mTab, 500);
        BackgroundSession.BackgroundTabData tabData = session.getTabDataList().get(0);
        tabData.setTabWindowId(1);
        tabData.setPlaceholderTabId(101);

        // Do not add liveTab to pool to simulate eviction/destruction from pool
        List<BackgroundSession> sessions = new ArrayList<>(Collections.singletonList(session));
        List<BackgroundSession> restored =
                ActorBackgroundActuationManager.restoreActiveWindowBackgroundTabsForTesting(
                        mTabModelSelector, 1, mWindowAndroid, sessions, mTabDelegateFactory);

        assertEquals(1, restored.size());
        assertEquals(session, restored.get(0));
        assertTrue(session.getTabDataList().isEmpty());
        verify(mTab, never()).updateAttachment(any(), any());
        verify(mTabRemover).removeTab(mPlaceholderTab, false);
    }

    @Test
    public void testBackgroundSession_AddTab_InheritsWindowId() {
        Tab tab2 = mock(Tab.class);
        BackgroundSession.BackgroundTabData tabData1 =
                new BackgroundSession.BackgroundTabData(mTab, 101, 0, 42);
        BackgroundSession session = new BackgroundSession(tabData1, 500);

        session.addTab(tab2);

        assertEquals(2, session.getTabDataList().size());
        assertEquals(42, session.getTabDataList().get(0).getTabWindowId());
        assertEquals(42, session.getTabDataList().get(1).getTabWindowId());
        assertNull(session.getTabDataList().get(1).getPlaceholderTabId());
    }

    @Test
    public void testRestoreActiveWindowBackgroundTabs_PlaceholderFreeTab_AppendsToEnd() {
        when(mTab.getId()).thenReturn(100);
        when(mTab.hasParentCollection()).thenReturn(false);
        when(mTab.isDestroyed()).thenReturn(false);
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.getCount()).thenReturn(3);

        BackgroundSession session = new BackgroundSession(mTab, 500);
        BackgroundSession.BackgroundTabData tabData = session.getTabDataList().get(0);
        tabData.setTabWindowId(1);
        tabData.setPlaceholderTabId(null);

        BackgroundTabPool pool = BackgroundTabPoolManager.acquire(mProfile);
        try {
            LiveBackgroundTab liveTab =
                    new LiveBackgroundTab(
                            pool,
                            mTab,
                            Tab.INVALID_TAB_ID,
                            500,
                            /* originalTabIndex= */ TabModel.INVALID_TAB_INDEX);
            pool.addLiveTab(liveTab);
        } finally {
            BackgroundTabPoolManager.release(pool);
        }

        List<BackgroundSession> sessions = new ArrayList<>(Collections.singletonList(session));
        List<BackgroundSession> restored =
                ActorBackgroundActuationManager.restoreActiveWindowBackgroundTabsForTesting(
                        mTabModelSelector, 1, mWindowAndroid, sessions, mTabDelegateFactory);

        assertEquals(1, restored.size());
        assertTrue(session.getTabDataList().isEmpty());
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
        verify(mTab).updateAttachment(mWindowAndroid, mTabDelegateFactory);
        verify(mTabModel).addTab(eq(mTab), eq(3), anyInt(), anyInt());
    }

    @Test
    public void testOnTaskCompleted_NoActivity_StopsOffscreenAndRetainsSession() {
        // No running activities in ApplicationStatus
        when(mTab.getId()).thenReturn(100);
        BackgroundSession session = new BackgroundSession(mTab, 500);
        mManager.getBackgroundSessions(); // read-only check

        // Ingest into mManager's background sessions
        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(500);
        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(500);
        when(task.isUnderActorControl()).thenReturn(true);
        when(task.getTabs()).thenReturn(Collections.singleton(100));
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(task));

        setupTransitionMocks();

        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        mManager.transitionActiveTasksToBackground(mTabModelSelector);
        assertEquals(1, mManager.getBackgroundSessions().size());

        mManager.onTaskCompleted(500);

        // Offscreen rendering should be stopped for the tab to conserve resources
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
        // Session should be retained in memory so when an activity is launched, it can restore
        assertEquals(1, mManager.getBackgroundSessions().size());
    }

    @Test
    public void testHasBackgroundSessionForTask_TaskScopedSession() {
        @SuppressWarnings("unchecked")
        Callback<Tab> callback = mock(Callback.class);
        mManager.provisionBackgroundTabForTask(mProfile, 500, callback);
        triggerPageLoadFinished();

        assertTrue(mManager.hasBackgroundSessionForTask(500));
        assertFalse(mManager.hasBackgroundSessionForTask(999));
    }

    @Test
    public void testHasBackgroundSessionForTask_MessageScopedSession() {
        when(mTab.getId()).thenReturn(100);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        mManager.startBackgroundActuation(mProfile, "msg_session_test");
        triggerPageLoadFinished();

        Tab secondTab = mock(Tab.class);
        when(secondTab.getId()).thenReturn(101);
        when(secondTab.getProfile()).thenReturn(mProfile);
        mManager.getBackgroundSessions().get(0).addTab(secondTab);

        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(600);
        when(task.getTabs()).thenReturn(Collections.emptySet());
        when(task.getLastActuatedTabId()).thenReturn(101);
        when(mActorKeyedService.getTask(600)).thenReturn(task);

        assertTrue(mManager.hasBackgroundSessionForTask(600));
        verify(mActorKeyedService).getTask(600);
        assertFalse(mManager.hasBackgroundSessionForTask(999));
        verify(mActorKeyedService).getTask(999);
    }

    @Test
    public void testRestoreActiveWindowBackgroundTabs_WarmStart_RecordsWarmRestoreLatency() {
        setupTransitionMocks();
        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(500);
        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(500);
        when(task.isUnderActorControl()).thenReturn(true);
        when(task.getTabs()).thenReturn(Collections.singleton(100));
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(task));

        mManager.transitionActiveTasksToBackground(mTabModelSelector);
        assertEquals(1, mManager.getBackgroundSessions().size());

        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.getTabById(101)).thenReturn(mPlaceholderTab);
        when(mTabModel.indexOf(mPlaceholderTab)).thenReturn(0);

        doAnswer(
                        inv -> {
                            ShadowSystemClock.advanceBy(45, TimeUnit.MILLISECONDS);
                            return null;
                        })
                .when(mTab)
                .updateAttachment(mWindowAndroid, mTabDelegateFactory);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_TOTAL, 45)
                        .expectIntRecord(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_TOTAL_WARM,
                                45)
                        .expectNoRecords(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_TOTAL_COLD)
                        .expectNoRecords(
                                ActorMetrics
                                        .ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_WAIT_FOR_TAB_STATE_INITIALIZED)
                        .build();

        assertTrue(
                mManager.restoreActiveWindowBackgroundTabs(
                        mTabModelSelector,
                        42,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        /* waitDurationMs= */ 0,
                        /* isColdStart= */ false));
        assertEquals(0, mManager.getBackgroundSessions().size());
        watcher.assertExpected();
    }

    @Test
    public void testRestoreActiveWindowBackgroundTabs_ColdStart_RecordsWaitAndColdRestoreLatency() {
        setupTransitionMocks();
        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(500);
        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(500);
        when(task.isUnderActorControl()).thenReturn(true);
        when(task.getTabs()).thenReturn(Collections.singleton(100));
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(task));

        mManager.transitionActiveTasksToBackground(mTabModelSelector);
        assertEquals(1, mManager.getBackgroundSessions().size());

        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.getTabById(101)).thenReturn(mPlaceholderTab);
        when(mTabModel.indexOf(mPlaceholderTab)).thenReturn(0);

        doAnswer(
                        inv -> {
                            ShadowSystemClock.advanceBy(40, TimeUnit.MILLISECONDS);
                            return null;
                        })
                .when(mTab)
                .updateAttachment(mWindowAndroid, mTabDelegateFactory);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                ActorMetrics
                                        .ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_WAIT_FOR_TAB_STATE_INITIALIZED,
                                250)
                        .expectIntRecord(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_TOTAL, 290)
                        .expectIntRecord(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_TOTAL_COLD,
                                290)
                        .expectNoRecords(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_TOTAL_WARM)
                        .build();

        assertTrue(
                mManager.restoreActiveWindowBackgroundTabs(
                        mTabModelSelector,
                        42,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        /* waitDurationMs= */ 250,
                        /* isColdStart= */ true));
        assertEquals(0, mManager.getBackgroundSessions().size());
        watcher.assertExpected();
    }

    @Test
    public void testRestoreActiveWindowBackgroundTabs_ZeroTabsRestored_DoesNotRecordLatency() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_TOTAL)
                        .expectNoRecords(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_TOTAL_COLD)
                        .expectNoRecords(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_TOTAL_WARM)
                        .expectNoRecords(
                                ActorMetrics
                                        .ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_WAIT_FOR_TAB_STATE_INITIALIZED)
                        .build();

        // Case 1: Empty sessions list.
        assertFalse(
                mManager.restoreActiveWindowBackgroundTabs(
                        mTabModelSelector,
                        42,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        /* waitDurationMs= */ 200,
                        /* isColdStart= */ true));

        // Case 2: Session whose LiveBackgroundTab was evicted from the pool.
        setupTransitionMocks();
        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(500);
        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(500);
        when(task.isUnderActorControl()).thenReturn(true);
        when(task.getTabs()).thenReturn(Collections.singleton(100));
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(task));

        mManager.transitionActiveTasksToBackground(mTabModelSelector);
        assertEquals(1, mManager.getBackgroundSessions().size());

        BackgroundTabPool pool = BackgroundTabPoolManager.acquire(mProfile);
        try {
            pool.removeLiveTabByOriginalId(100);
        } finally {
            BackgroundTabPoolManager.release(pool);
        }

        when(mTabModel.getTabById(101)).thenReturn(mPlaceholderTab);

        assertFalse(
                mManager.restoreActiveWindowBackgroundTabs(
                        mTabModelSelector,
                        42,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        /* waitDurationMs= */ 200,
                        /* isColdStart= */ true));
        assertEquals(0, mManager.getBackgroundSessions().size());
        watcher.assertExpected();
    }

    @Test
    public void testOnTaskCompleted_WarmActivity_RecordsInMemoryWarmRestoreLatency() {
        setupTransitionMocks();
        setupWarmActivityMocks();
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(777);
        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(777);
        when(task.isUnderActorControl()).thenReturn(true);
        when(task.getTabs()).thenReturn(Collections.singleton(100));
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(task));

        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        mManager.transitionActiveTasksToBackground(mTabModelSelector);
        assertEquals(1, mManager.getBackgroundSessions().size());

        doAnswer(
                        inv -> {
                            ShadowSystemClock.advanceBy(30, TimeUnit.MILLISECONDS);
                            return null;
                        })
                .when(mTab)
                .updateAttachment(eq(mWindowAndroid), any());
        doAnswer(
                        inv -> {
                            ShadowSystemClock.advanceBy(55, TimeUnit.MILLISECONDS);
                            return null;
                        })
                .when(mTabPersistentStore)
                .saveState();

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_TOTAL, 30)
                        .expectIntRecord(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_TOTAL_WARM,
                                30)
                        .expectNoRecords(
                                ActorMetrics.ACTOR_BACKGROUND_ACTUATION_RESTORE_LATENCY_TOTAL_COLD)
                        .build();

        mManager.onTaskCompleted(777);

        watcher.assertExpected();
        verify(mTabPersistentStore).saveState();
        assertEquals(0, mManager.getBackgroundSessions().size());
    }

    @Test
    public void
            testTransitionActiveTasksToBackground_AndRestore_RecordsBackgroundExecutionDuration() {
        ActorForegroundServiceController controller = mock(ActorForegroundServiceController.class);
        when(controller.hasBackgroundSessionForTask(anyInt()))
                .thenAnswer(inv -> mManager.hasBackgroundSessionForTask(inv.getArgument(0)));
        ActorForegroundServiceController.setInstanceForTesting(controller);
        setupTransitionMocks();

        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(500);
        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(500);
        when(task.isUnderActorControl()).thenReturn(true);
        when(task.getTabs()).thenReturn(Collections.singleton(100));
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(task));
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);

        // Task starts acting in the foreground for 150ms.
        ActorMetrics.getInstance().onTaskStateChanged(500, ActorTaskState.ACTING);
        ShadowSystemClock.advanceBy(150, TimeUnit.MILLISECONDS);

        // Transitioning active tasks to background flushes foreground time and switches to
        // Background mode for 350ms.
        mManager.transitionActiveTasksToBackground(mTabModelSelector);
        ShadowSystemClock.advanceBy(350, TimeUnit.MILLISECONDS);

        // Prepare mocks for restoring the background tab back into the active window.
        when(mPlaceholderTab.getId()).thenReturn(101);
        when(mPlaceholderTab.isDestroyed()).thenReturn(false);
        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.getTabById(101)).thenReturn(mPlaceholderTab);
        when(mTabModel.indexOf(mPlaceholderTab)).thenReturn(0);

        assertTrue(
                mManager.restoreActiveWindowBackgroundTabs(
                        mTabModelSelector,
                        TabWindowManager.INVALID_WINDOW_ID,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        /* waitDurationMs= */ 0,
                        /* isColdStart= */ false));

        // Subsequent time (100ms) before completion is accumulated back in Foreground mode
        // (150ms + 100ms = 250ms total Foreground, 350ms total Background).
        ShadowSystemClock.advanceBy(100, TimeUnit.MILLISECONDS);
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Actor.Tools.ExecutionDuration.Foreground.Acting", 250)
                        .expectIntRecord("Actor.Tools.ExecutionDuration.Background.Acting", 350)
                        .build();
        ActorMetrics.getInstance().onTaskStateChanged(500, ActorTaskState.FINISHED);
        watcher.assertExpected();
    }

    @Test
    public void testProvisionBackgroundTabForTask_RecordsBackgroundExecutionDuration() {
        ActorForegroundServiceController controller = mock(ActorForegroundServiceController.class);
        when(controller.hasBackgroundSessionForTask(anyInt()))
                .thenAnswer(inv -> mManager.hasBackgroundSessionForTask(inv.getArgument(0)));
        ActorForegroundServiceController.setInstanceForTesting(controller);
        @SuppressWarnings("unchecked")
        Callback<Tab> callback = mock(Callback.class);
        mManager.provisionBackgroundTabForTask(mProfile, 500, callback);
        triggerPageLoadFinished();

        ActorMetrics.getInstance().onTaskStateChanged(500, ActorTaskState.ACTING);
        ShadowSystemClock.advanceBy(300, TimeUnit.MILLISECONDS);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Actor.Tools.ExecutionDuration.Background.Acting", 300);
        ActorMetrics.getInstance().onTaskStateChanged(500, ActorTaskState.FINISHED);
        watcher.assertExpected();
    }

    @Test
    public void testDestroy_MessageScopedSession_ClearsTaskBackgroundMode() {
        ActorForegroundServiceController controller = mock(ActorForegroundServiceController.class);
        when(controller.hasBackgroundSessionForTask(anyInt()))
                .thenAnswer(inv -> mManager.hasBackgroundSessionForTask(inv.getArgument(0)));
        ActorForegroundServiceController.setInstanceForTesting(controller);

        when(mTab.getId()).thenReturn(100);
        when(mTab.getProfile()).thenReturn(mProfile);

        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(600);
        when(task.getTabs()).thenReturn(Collections.singleton(100));
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(task));
        when(mActorKeyedService.getTask(600)).thenReturn(task);

        // Task starts acting in the foreground for 100ms before the message-scoped background
        // session is created.
        ActorMetrics.getInstance().onTaskStateChanged(600, ActorTaskState.ACTING);
        ShadowSystemClock.advanceBy(100, TimeUnit.MILLISECONDS);

        // Starting background actuation creates the message-scoped session and calls
        // refreshActiveTaskModes(), flushing the 100ms Foreground duration and transitioning
        // task 600 to Background mode for 200ms.
        mManager.startBackgroundActuation(mProfile, "msg_session_destroy_test");
        triggerPageLoadFinished();
        ShadowSystemClock.advanceBy(200, TimeUnit.MILLISECONDS);

        // Destroying the message-scoped session calls refreshActiveTaskModes(), transitioning
        // task 600 back to Foreground so subsequent active time (150ms) is attributed to
        // Foreground (100ms + 150ms = 250ms total Foreground, 200ms total Background).
        mManager.destroy();
        ShadowSystemClock.advanceBy(150, TimeUnit.MILLISECONDS);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Actor.Tools.ExecutionDuration.Background.Acting", 200)
                        .expectIntRecord("Actor.Tools.ExecutionDuration.Foreground.Acting", 250)
                        .build();
        ActorMetrics.getInstance().onTaskStateChanged(600, ActorTaskState.FINISHED);
        watcher.assertExpected();
    }

    @Test
    public void
            testRestoreActiveWindowBackgroundTabs_MessageScopedSession_PartialRestore_ClearsRestoredTaskBackgroundMode() {
        MultiWindowTestUtils.enableMultiInstance();
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 2,
                /* url= */ "https://www.example.com",
                /* tabCount= */ 1,
                /* taskId= */ 57);

        ActorForegroundServiceController controller = mock(ActorForegroundServiceController.class);
        when(controller.hasBackgroundSessionForTask(anyInt()))
                .thenAnswer(inv -> mManager.hasBackgroundSessionForTask(inv.getArgument(0)));
        ActorForegroundServiceController.setInstanceForTesting(controller);

        when(mTab.getId()).thenReturn(100);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mTab.hasParentCollection()).thenReturn(false);
        when(mTab.isDestroyed()).thenReturn(false);
        when(mTab.isOffTheRecord()).thenReturn(false);
        when(mTabModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);
        when(mTabModel.getCount()).thenReturn(1);

        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(600);
        when(task.getTabs()).thenReturn(Collections.singleton(100));
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(task));
        when(mActorKeyedService.getTask(600)).thenReturn(task);

        mManager.startBackgroundActuation(mProfile, "msg_session_partial_restore_test");
        triggerPageLoadFinished();

        BackgroundSession session = mManager.getBackgroundSessions().get(0);
        session.getTabDataList().get(0).setTabWindowId(1);
        session.getTabDataList().get(0).setPlaceholderTabId(null);

        Tab secondTab = mock(Tab.class);
        when(secondTab.getId()).thenReturn(200);
        when(secondTab.getProfile()).thenReturn(mProfile);
        session.addTab(secondTab);
        session.getTabDataList().get(1).setTabWindowId(2);

        ActorMetrics.getInstance().onTaskStateChanged(600, ActorTaskState.ACTING);
        ShadowSystemClock.advanceBy(200, TimeUnit.MILLISECONDS);

        // Restore only window 1's tab (tab 100); tab 200 in window 2 remains in the session.
        assertTrue(
                mManager.restoreActiveWindowBackgroundTabs(
                        mTabModelSelector,
                        1,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        /* waitDurationMs= */ 0,
                        /* isColdStart= */ false));

        assertEquals(1, mManager.getBackgroundSessions().size());
        assertEquals(1, session.getTabDataList().size());
        assertEquals(secondTab, session.getTabDataList().get(0).getTab());

        // Even though the message-scoped session is not yet empty, task 600's tab (100) was
        // restored to the foreground, so subsequent active time should be attributed to Foreground.
        ShadowSystemClock.advanceBy(150, TimeUnit.MILLISECONDS);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Actor.Tools.ExecutionDuration.Background.Acting", 200)
                        .expectIntRecord("Actor.Tools.ExecutionDuration.Foreground.Acting", 150)
                        .build();
        ActorMetrics.getInstance().onTaskStateChanged(600, ActorTaskState.FINISHED);
        watcher.assertExpected();
    }
}
