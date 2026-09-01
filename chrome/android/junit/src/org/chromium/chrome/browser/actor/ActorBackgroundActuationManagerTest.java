// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doCallRealMethod;
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

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
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

import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashSet;

/** Unit tests for {@link ActorBackgroundActuationManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActorBackgroundActuationManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String MESSAGE_ID_SUCCESS = "message_id_success";
    private static final String MESSAGE_ID_FAIL = "message_id_fail";
    private static final String MESSAGE_ID_CRASH = "message_id_crash";
    private static final String MESSAGE_ID_CANCELLED = "message_id_cancelled";
    private static final String TEST_URL = "about:blank";

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
    @Mock private ChromeTabbedActivity mActivity;
    @Mock private TabModelOrchestrator mTabModelOrchestrator;
    @Mock private TabPersistentStore mTabPersistentStore;

    private ActorBackgroundActuationManager mManager;

    @Before
    public void setUp() {
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);
        OffscreenRenderingManager.setInstanceForTesting(mOffscreenRenderingManager);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);

        when(mOffscreenRenderingManager.getOffscreenWindow()).thenReturn(mWindowAndroid);
        TabBuilder.setTabForTesting(mTab);

        when(mActivity.getWindowAndroid()).thenReturn(mWindowAndroid);
        doCallRealMethod().when(mActivity).setTabModelOrchestratorForTesting(any());
        mActivity.setTabModelOrchestratorForTesting(mTabModelOrchestrator);
        when(mTabModelOrchestrator.getTabPersistentStore()).thenReturn(mTabPersistentStore);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModelSelector.getTabCreatorManager()).thenReturn(mTabCreatorManager);
        when(mTabCreatorManager.getTabCreator(false)).thenReturn(mTabCreator);
        when(mTabCreator.createDefaultTabDelegateFactory()).thenReturn(mTabDelegateFactory);
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        when(mTabModel.indexOf(mTab)).thenReturn(0);
        when(mTabModel.iterator()).thenAnswer(inv -> Collections.singletonList(mTab).iterator());
        when(mPlaceholderTab.getId()).thenReturn(101);
        when(mTabCreator.createFrozenTab(any(), anyInt(), anyInt())).thenReturn(mPlaceholderTab);
        when(mTabWindowManager.getWindowIdForSelector(mTabModelSelector)).thenReturn(42);
        when(mTabWindowManager.getIdForWindow(mActivity)).thenReturn(42);
        when(mTabWindowManager.getTabModelSelectorById(42)).thenReturn(mTabModelSelector);
        TabStateExtractor.setTabStateForTesting(100, new TabState());

        mManager = new ActorBackgroundActuationManager();
    }

    @After
    public void tearDown() {
        ProfileManager.resetForTesting();
        ActorKeyedServiceFactory.setForTesting(null);
        OffscreenRenderingManager.setInstanceForTesting(null);
        ApplicationStatus.destroyForJUnitTests();
        TabBuilder.setTabForTesting(null);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(null);
        TabStateExtractor.resetTabStatesForTesting();
    }

    @Test
    public void testStartBackgroundActuation_Success() {
        mManager.startBackgroundActuation(mProfile, MESSAGE_ID_SUCCESS);

        // Verify offscreen rendering started
        verify(mOffscreenRenderingManager).startOffscreenRendering(eq(mTab), anyInt(), anyInt());

        // Capture and trigger page load success
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        observer.onPageLoadFinished(mTab, new GURL(TEST_URL));

        // Verify the tab was prepared and set on ActorKeyedService
        verify(mActorKeyedService).setPreparedBackgroundTab(mTab, MESSAGE_ID_SUCCESS);
        verify(mActorKeyedService, never()).notifyBackgroundSetupFailed(any());
    }

    @Test
    public void testStartBackgroundActuation_PageLoadFailed() {
        mManager.startBackgroundActuation(mProfile, MESSAGE_ID_FAIL);

        // Capture observer and trigger page load failure
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        observer.onPageLoadFailed(mTab, 404);

        // Verify setup failed notification was sent to native
        verify(mActorKeyedService).notifyBackgroundSetupFailed(MESSAGE_ID_FAIL);
        // Verify cleanup stopped offscreen rendering
        verify(mTab).removeObserver(observer);
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
    }

    @Test
    public void testStartBackgroundActuation_Crash() {
        mManager.startBackgroundActuation(mProfile, MESSAGE_ID_CRASH);

        // Capture observer and trigger renderer crash
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        observer.onCrash(mTab);

        // Verify setup failed and cleanup was executed
        verify(mActorKeyedService).notifyBackgroundSetupFailed(MESSAGE_ID_CRASH);
        verify(mTab).removeObserver(observer);
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
    }

    @Test
    public void testStartBackgroundActuation_CancelledBeforeLoadFinished() {
        mManager.startBackgroundActuation(mProfile, MESSAGE_ID_CANCELLED);

        // Capture observer
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        // Cancel/Cleanup before load finished
        mManager.cleanupContext(MESSAGE_ID_CANCELLED);

        // Verify cleanup stopped offscreen rendering
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);

        // Simulate native destruction triggering onDestroyed
        observer.onDestroyed(mTab);

        // Verify observer removed itself
        verify(mTab).removeObserver(observer);

        // Now trigger page load finish
        observer.onPageLoadFinished(mTab, new GURL(TEST_URL));

        // Verify setPreparedBackgroundTab was NOT called because of our fast-guard
        verify(mActorKeyedService, never()).setPreparedBackgroundTab(any(), any());
    }

    @Test
    public void testTransitionActiveTasksToBackground() {
        TabState testTabState = new TabState();
        TabStateExtractor.setTabStateForTesting(100, testTabState);

        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(mTab);
        when(mTabModel.iterator()).thenReturn(Collections.singletonList(mTab).iterator());

        when(mTab.getId()).thenReturn(100);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(123);

        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(123);
        when(task.getTabs()).thenReturn(Collections.singleton(100));
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(task));

        mManager.transitionActiveTasksToBackground(mTabModelSelector);

        // Verify offscreen rendering was started for the transitioned tab
        verify(mOffscreenRenderingManager).startOffscreenRendering(eq(mTab), anyInt(), anyInt());

        // Verify the transitioned session is tracked
        mManager.destroy();
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);

        TabStateExtractor.resetTabStatesForTesting();
    }

    @Test
    public void testTransitionActiveTasksToBackground_MultipleTabs() {
        TabStateExtractor.setTabStateForTesting(100, new TabState());
        TabStateExtractor.setTabStateForTesting(101, new TabState());

        Tab tab2 = mock(Tab.class);
        when(tab2.getId()).thenReturn(101);
        when(tab2.getProfile()).thenReturn(mProfile);
        when(mTabModel.indexOf(tab2)).thenReturn(1);

        when(mTabModel.iterator()).thenReturn(Arrays.asList(mTab, tab2).iterator());

        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);

        when(mTab.getId()).thenReturn(100);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(123);
        when(mActorKeyedService.getActiveTaskIdOnTab(101, false)).thenReturn(123);

        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(123);
        when(task.getTabs()).thenReturn(new LinkedHashSet<>(Arrays.asList(100, 101)));
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(task));

        mManager.transitionActiveTasksToBackground(mTabModelSelector);

        // Verify offscreen rendering is started for both tabs
        verify(mOffscreenRenderingManager).startOffscreenRendering(eq(mTab), anyInt(), anyInt());
        verify(mOffscreenRenderingManager).startOffscreenRendering(eq(tab2), anyInt(), anyInt());

        mManager.destroy();
        verify(mOffscreenRenderingManager).stopOffscreenRendering(tab2);

        TabStateExtractor.resetTabStatesForTesting();
    }

    @Test
    public void testProvisionBackgroundTabForTask_Success() {
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

        // Verify callback invoked with tab
        verify(callback).onResult(mTab);
    }

    @Test
    public void testProvisionBackgroundTabForTask_ExistingSession() {
        TabState testTabState = new TabState();
        TabStateExtractor.setTabStateForTesting(100, testTabState);

        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(123);
        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(123);
        when(task.getTabs()).thenReturn(Collections.singleton(100));
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(task));

        when(mTab.getId()).thenReturn(100);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(mTab);
        when(mTabModel.iterator()).thenReturn(Collections.singletonList(mTab).iterator());
        mManager.transitionActiveTasksToBackground(mTabModelSelector);

        @SuppressWarnings("unchecked")
        Callback<Tab> callback = mock(Callback.class);
        mManager.provisionBackgroundTabForTask(mProfile, 123, callback);

        verify(mOffscreenRenderingManager, atLeastOnce())
                .startOffscreenRendering(eq(mTab), anyInt(), anyInt());

        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab, atLeastOnce()).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        observer.onPageLoadFinished(mTab, new GURL(TEST_URL));
        verify(callback).onResult(mTab);
    }

    @Test
    public void testProvisionBackgroundTabForTask_PageLoadFailed() {
        @SuppressWarnings("unchecked")
        Callback<Tab> callback = mock(Callback.class);
        mManager.provisionBackgroundTabForTask(mProfile, 123, callback);

        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab, atLeastOnce()).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        observer.onPageLoadFailed(mTab, -1);

        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
        verify(mTab).destroy();
        verify(callback).onResult(null);
    }

    @Test
    public void testProvisionBackgroundTabForTask_Crash() {
        @SuppressWarnings("unchecked")
        Callback<Tab> callback = mock(Callback.class);
        mManager.provisionBackgroundTabForTask(mProfile, 123, callback);

        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab, atLeastOnce()).addObserver(captor.capture());
        TabObserver observer = captor.getValue();

        observer.onCrash(mTab);

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

        // Call cleanupContext which should trigger save tab state if TabState != null
        mManager.cleanupContext("test_msg_id");

        // Verify offscreen rendering stopped
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);

        TabStateExtractor.resetTabStatesForTesting();
    }

    @Test
    public void testDestroy_WarmActivity_RestoresTabsBeforeClear() {
        when(mActivity.isFinishing()).thenReturn(false);
        when(mActivity.isDestroyed()).thenReturn(false);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        mManager.startBackgroundActuation(mProfile, "msg_warm_test");
        assertEquals(1, mManager.getBackgroundSessions().size());

        mManager.destroy();

        verify(mTab).updateAttachment(eq(mWindowAndroid), any());
        verify(mTabPersistentStore).saveState();
        assertEquals(0, mManager.getBackgroundSessions().size());
    }

    @Test
    public void testDestroy_TabStateNotInitialized_SkipsRestoration() {
        when(mActivity.isFinishing()).thenReturn(false);
        when(mActivity.isDestroyed()).thenReturn(false);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        mManager.startBackgroundActuation(mProfile, "msg_uninit_test");
        assertEquals(1, mManager.getBackgroundSessions().size());

        mManager.destroy();

        verify(mTab, never()).updateAttachment(any(), any());
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
        verify(mTabPersistentStore, never()).saveState();
        assertEquals(0, mManager.getBackgroundSessions().size());
    }

    @Test
    public void testDestroy_ActivityFinishingOrDestroyed_SkipsRestoration() {
        when(mActivity.isFinishing()).thenReturn(true);
        when(mActivity.isDestroyed()).thenReturn(false);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        mManager.startBackgroundActuation(mProfile, "msg_finishing_test");
        assertEquals(1, mManager.getBackgroundSessions().size());

        mManager.destroy();

        verify(mTab, never()).updateAttachment(any(), any());
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
        verify(mTabPersistentStore, never()).saveState();
        assertEquals(0, mManager.getBackgroundSessions().size());
    }

    @Test
    public void testDestroy_MixedWarmAndColdActivities() {
        AsyncInitializationActivity coldActivity = mock(AsyncInitializationActivity.class);
        TabModelSelector coldSelector = mock(TabModelSelector.class);
        when(coldActivity.isFinishing()).thenReturn(true);
        when(mTabWindowManager.getIdForWindow(coldActivity)).thenReturn(84);
        when(mTabWindowManager.getTabModelSelectorById(84)).thenReturn(coldSelector);

        when(mActivity.isFinishing()).thenReturn(false);
        when(mActivity.isDestroyed()).thenReturn(false);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(coldActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);
        ApplicationStatus.onStateChangeForTesting(coldActivity, ActivityState.STOPPED);

        mManager.startBackgroundActuation(mProfile, "msg_mixed_test");

        mManager.destroy();

        verify(mTab).updateAttachment(eq(mWindowAndroid), any());
        verify(mTabPersistentStore).saveState();
        assertEquals(0, mManager.getBackgroundSessions().size());
    }

    @Test
    public void testCleanupContext_WarmActivity_RestoresTabsBeforeStop() {
        when(mActivity.isFinishing()).thenReturn(false);
        when(mActivity.isDestroyed()).thenReturn(false);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        mManager.startBackgroundActuation(mProfile, "msg_cleanup_warm");
        assertEquals(1, mManager.getBackgroundSessions().size());

        mManager.cleanupContext("msg_cleanup_warm");

        verify(mTab).updateAttachment(eq(mWindowAndroid), any());
        verify(mTabPersistentStore).saveState();
        assertEquals(0, mManager.getBackgroundSessions().size());
    }

    @Test
    public void testDestroy_NonAsyncInitializationActivity_SkipsRestoration() {
        Activity nonTabbedActivity = mock(Activity.class);
        ApplicationStatus.onStateChangeForTesting(nonTabbedActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(nonTabbedActivity, ActivityState.STOPPED);

        mManager.startBackgroundActuation(mProfile, "msg_non_tabbed_test");
        assertEquals(1, mManager.getBackgroundSessions().size());

        mManager.destroy();

        verify(mTab, never()).updateAttachment(any(), any());
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
        verify(mTabPersistentStore, never()).saveState();
        assertEquals(0, mManager.getBackgroundSessions().size());
    }

    @Test
    public void testOnTaskCompleted_WarmActivity_RestoresTabsBeforeStop() {
        when(mActivity.isFinishing()).thenReturn(false);
        when(mActivity.isDestroyed()).thenReturn(false);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        when(mActorKeyedService.getActiveTasksCount()).thenReturn(1);
        when(mActorKeyedService.getActiveTaskIdOnTab(100, false)).thenReturn(777);
        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(777);
        when(task.getTabs()).thenReturn(Collections.singleton(100));
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(task));

        when(mTab.getId()).thenReturn(100);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(mTab);
        when(mTabModel.iterator()).thenReturn(Collections.singletonList(mTab).iterator());

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
        when(mActivity.isFinishing()).thenReturn(false);
        when(mActivity.isDestroyed()).thenReturn(false);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabCreator.createDefaultTabDelegateFactory()).thenReturn(null);

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        mManager.startBackgroundActuation(mProfile, "msg_null_factory");
        assertEquals(1, mManager.getBackgroundSessions().size());

        mManager.destroy();

        verify(mTab, never()).updateAttachment(any(), any());
        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
        assertEquals(0, mManager.getBackgroundSessions().size());
    }
}
