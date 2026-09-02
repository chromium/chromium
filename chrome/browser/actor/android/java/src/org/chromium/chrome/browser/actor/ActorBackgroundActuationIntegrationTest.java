// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableLeakChecks;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.ui.base.WindowAndroid;

import java.io.File;
import java.util.Collections;

/**
 * Integration tests for Actor background actuation when Chrome transitions between foreground and
 * background states across activity lifecycle events.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({
    ChromeFeatureList.GLIC_BACKGROUND_ACTUATION,
    ChromeFeatureList.ACTOR_LIVE_NOTIFICATION,
    ChromeFeatureList.ACTOR_NOTIFICATION_INTENT_ROUTING,
    ChromeFeatureList.ACTOR_STEP_PROGRESS_NOTIFICATION,
})
@DoNotBatch(reason = "Tests background actuation and tab model state across activity lifecycles.")
@DisableLeakChecks("Activity destruction causes production leak in TabBottomSheetWebUi")
@SuppressWarnings("DirectInvocationOnMock")
public class ActorBackgroundActuationIntegrationTest {
    private static final int TASK_ID = 123;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActorKeyedService mActorService;
    @Mock private ActorTask mActorTask;
    @Mock private ActorForegroundServiceImpl mForegroundService;
    @Mock private ActorForegroundServiceImpl.LocalBinder mBinder;

    private ActorForegroundServiceControllerImpl mController;
    private ActorBackgroundActuationManager mBackgroundManager;
    private Tab mTab;

    @Before
    public void setUp() throws Exception {
        ChromeFeatureList.sGlicBackgroundActuationRequireNotifications.setForTesting(false);

        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivityTab();
        ChromeTabUtils.waitForTabPageLoaded(mTab, (String) null);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(mActorTask.getId()).thenReturn(TASK_ID);
                    when(mActorTask.getState()).thenReturn(ActorTaskState.ACTING);
                    when(mActorTask.isActingOnTab(mTab.getId())).thenReturn(true);
                    when(mActorTask.isUnderActorControl()).thenReturn(true);
                    when(mActorTask.getTabs()).thenReturn(Collections.singleton(mTab.getId()));
                    when(mActorTask.getLastActedTabs())
                            .thenReturn(Collections.singleton(mTab.getId()));
                    when(mActorTask.getLastActuatedTabId()).thenReturn(mTab.getId());

                    when(mActorService.getActiveTasks())
                            .thenReturn(Collections.singletonList(mActorTask));
                    when(mActorService.getActiveTasksCount()).thenReturn(1);
                    when(mActorService.getTask(TASK_ID)).thenReturn(mActorTask);
                    when(mActorService.getCurrentActiveTask()).thenReturn(mActorTask);
                    when(mActorService.getActiveTaskIdOnTab(mTab.getId())).thenReturn(TASK_ID);
                    when(mActorService.getActiveTaskIdOnTab(mTab.getId(), false))
                            .thenReturn(TASK_ID);
                    when(mActorService.getActiveTaskIdOnTab(mTab.getId(), true))
                            .thenReturn(TASK_ID);

                    ActorKeyedServiceFactory.setForTesting(mActorService);

                    when(mBinder.getService()).thenReturn(mForegroundService);
                    mController = new ActorForegroundServiceControllerImpl();
                    mBackgroundManager = new ActorBackgroundActuationManager();
                    mController.setBackgroundManagerForTesting(mBackgroundManager);
                    mController.getServiceConnectionForTesting().onServiceConnected(null, mBinder);
                    ActorForegroundServiceController.setInstanceForTesting(mController);
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mBackgroundManager != null) {
                        mBackgroundManager.destroy();
                    }
                    if (mController != null) {
                        mController.destroyBackgroundActuationManager();
                    }
                    OffscreenRenderingManager.getInstance().destroy();
                });
        ActorForegroundServiceController.setInstanceForTesting(null);
        ActorKeyedServiceFactory.setForTesting(null);
    }

    /**
     * Verifies that when Chrome is backgrounded and the activity stops while an Actor task is in
     * progress, the tab transitions to offscreen background rendering, and foregrounding the
     * activity restores the tab while the task continues running.
     */
    @Test
    @MediumTest
    public void
            testChromeToBackground_CtaStops_TaskInProgress_ForegroundRestoresTabAndTaskContinues()
                    throws Exception {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        TabModelSelector selector = activity.getTabModelSelector();
        TabModel model = selector.getModel(/* incognito= */ false);
        TabDelegateFactory delegateFactory = TabTestUtils.getDelegateFactory(mTab);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    verifyTabInForegroundModel(model, mTab);
                    assertEquals(TASK_ID, (int) mActorService.getActiveTaskIdOnTab(mTab.getId()));
                });

        // Transition active tasks to background when activity stops.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController.transitionActiveTasksToBackground(selector);
                });

        int placeholderTabId =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> verifyTabTransitionedToBackground(model, mTab));

        // Task remains acting on the tab while in the background.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(TASK_ID, (int) mActorService.getActiveTaskIdOnTab(mTab.getId()));
                });

        // Restore tab back to the active window when activity returns to foreground.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController.restoreActiveWindowBackgroundTabs(
                            selector, activity.getWindowAndroid(), delegateFactory);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    verifyTabInForegroundModel(model, mTab);
                    assertNull(model.getTabById(placeholderTabId));
                    assertTrue(mBackgroundManager.getBackgroundSessions().isEmpty());
                    assertEquals(TASK_ID, (int) mActorService.getActiveTaskIdOnTab(mTab.getId()));
                });
    }

    /**
     * Verifies that when Chrome is backgrounded and an Actor task completes while the activity is
     * stopped, the tab is restored back to the activity model and its state is persisted to disk.
     */
    @Test
    @MediumTest
    public void testChromeToBackground_CtaStops_TaskCompleted_TabsRestoredAndSavedToDisk()
            throws Exception {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        TabModelSelector selector = activity.getTabModelSelector();
        TabModel model = selector.getModel(/* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(() -> verifyTabInForegroundModel(model, mTab));

        // Transition active tasks to background when activity stops.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController.transitionActiveTasksToBackground(selector);
                });

        int placeholderTabId =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> verifyTabTransitionedToBackground(model, mTab));

        // Complete the task while the activity is stopped in background.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(mActorTask.getState()).thenReturn(ActorTaskState.FINISHED);
                    when(mActorTask.isCompleted()).thenReturn(true);
                    when(mActorTask.isUnderActorControl()).thenReturn(false);

                    mController.onTaskCompleted(TASK_ID);
                });

        // Warm session restoration places the tab back in the model and destroys placeholder.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    verifyTabInForegroundModel(model, mTab);
                    assertNull(model.getTabById(placeholderTabId));
                    assertTrue(mBackgroundManager.getBackgroundSessions().isEmpty());
                });

        // Persist tab state to disk upon backgrounding / completion.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    activity.saveState();
                });

        File stateFolder = TabStateDirectory.getOrCreateTabbedModeStateDirectory();
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    File tabFile =
                            TabStateFileManager.getTabStateFile(
                                    stateFolder,
                                    mTab.getId(),
                                    /* encrypted= */ false,
                                    /* isFlatbuffer= */ true);
                    File legacyFile =
                            TabStateFileManager.getTabStateFile(
                                    stateFolder,
                                    mTab.getId(),
                                    /* encrypted= */ false,
                                    /* isFlatbuffer= */ false);
                    Criteria.checkThat(
                            "Tab state file should exist on disk after saving state",
                            tabFile.exists() || legacyFile.exists(),
                            Matchers.is(true));
                });
    }

    /**
     * Verifies that when Chrome is backgrounded and the activity is destroyed while an Actor task
     * is in progress, the task and background session remain alive, and foregrounding in a new
     * activity restores the tab and allows the task to keep working.
     */
    @Test
    @MediumTest
    public void
            testChromeToBackground_CtaDestroyed_TaskInProgress_ForegroundRestoresTabAndTaskContinues()
                    throws Exception {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        TabModelSelector selector = activity.getTabModelSelector();
        TabModel model = selector.getModel(/* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(() -> verifyTabInForegroundModel(model, mTab));

        // Transition active tasks to background session.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController.transitionActiveTasksToBackground(selector);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(1, mBackgroundManager.getBackgroundSessions().size());
                    assertTrue(mTab.getIsOffscreenRenderingSupplier().get());
                });

        // Destroy previous activity while background actuation keeps the task alive.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    activity.finish();
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(1, mBackgroundManager.getBackgroundSessions().size());
                });

        // Restore tab into the newly created / restored activity window.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int windowId =
                            TabWindowManagerSingleton.getInstance()
                                    .getWindowIdForSelector(selector);
                    TabModel mockNewModel = mock(TabModel.class);
                    TabModelSelector mockNewSelector = mock(TabModelSelector.class);
                    TabCreatorManager mockCreatorManager = mock(TabCreatorManager.class);
                    TabCreator mockTabCreator = mock(TabCreator.class);
                    TabDelegateFactory delegateFactory = TabTestUtils.getDelegateFactory(mTab);
                    WindowAndroid newWindow = activity.getWindowAndroid();

                    TabWindowManager mockWindowManager = mock(TabWindowManager.class);
                    when(mockWindowManager.getWindowIdForSelector(mockNewSelector))
                            .thenReturn(windowId);
                    TabWindowManagerSingleton.setTabWindowManagerForTesting(mockWindowManager);

                    when(mockNewSelector.getModel(false)).thenReturn(mockNewModel);
                    when(mockNewSelector.getTabCreatorManager()).thenReturn(mockCreatorManager);
                    when(mockCreatorManager.getTabCreator(false)).thenReturn(mockTabCreator);
                    when(mockTabCreator.createDefaultTabDelegateFactory())
                            .thenReturn(delegateFactory);
                    when(mockNewModel.indexOf(mTab)).thenReturn(TabModel.INVALID_TAB_INDEX);

                    mController.restoreActiveWindowBackgroundTabs(
                            mockNewSelector, newWindow, delegateFactory);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(mTab.getIsOffscreenRenderingSupplier().get());
                    assertTrue(mBackgroundManager.getBackgroundSessions().isEmpty());
                    assertEquals(TASK_ID, (int) mActorService.getActiveTaskIdOnTab(mTab.getId()));
                });
    }

    /**
     * Verifies that step progress updates received while an Actor task is running in the background
     * maintain the background actuation session and offscreen rendering state.
     */
    @Test
    @MediumTest
    public void testChromeToBackground_StepProgressUpdated_MaintainsBackgroundActuationState()
            throws Exception {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        TabModelSelector selector = activity.getTabModelSelector();
        TabModel model = selector.getModel(/* incognito= */ false);
        TabDelegateFactory delegateFactory = TabTestUtils.getDelegateFactory(mTab);

        ThreadUtils.runOnUiThreadBlocking(() -> verifyTabInForegroundModel(model, mTab));

        // Transition active tasks to background.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController.transitionActiveTasksToBackground(selector);
                });

        int placeholderTabId =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> verifyTabTransitionedToBackground(model, mTab));

        // Simulate step action update while in background.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(mActorTask.getCurrentActionName()).thenReturn("Navigating to page...");
                    assertEquals(1, mBackgroundManager.getBackgroundSessions().size());
                    assertTrue(mTab.getIsOffscreenRenderingSupplier().get());
                });

        // Bring Chrome back to foreground and verify clean restoration.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController.restoreActiveWindowBackgroundTabs(
                            selector, activity.getWindowAndroid(), delegateFactory);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    verifyTabInForegroundModel(model, mTab);
                    assertNull(model.getTabById(placeholderTabId));
                    assertTrue(mBackgroundManager.getBackgroundSessions().isEmpty());
                    assertEquals(TASK_ID, (int) mActorService.getActiveTaskIdOnTab(mTab.getId()));
                });
    }

    /**
     * Verifies that when background actuation is disabled, tabs are not detached to background
     * sessions upon background transitions.
     */
    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.GLIC_BACKGROUND_ACTUATION})
    public void testBackgroundActuation_FeatureDisabled_DoesNotDetachTabs() throws Exception {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        TabModelSelector selector = activity.getTabModelSelector();
        TabModel model = selector.getModel(/* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    verifyTabInForegroundModel(model, mTab);
                    assertFalse(ActorUtils.isBackgroundActuationEnabled());
                });

        // When background actuation is disabled, tab remains attached to the model.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    verifyTabInForegroundModel(model, mTab);
                    assertTrue(mBackgroundManager.getBackgroundSessions().isEmpty());
                });
    }

    private void verifyTabInForegroundModel(TabModel model, Tab expectedTab) {
        assertEquals(1, model.getCount());
        assertEquals(expectedTab, model.getTabAt(0));
        assertEquals(expectedTab, model.getTabById(expectedTab.getId()));
        assertFalse(expectedTab.getIsOffscreenRenderingSupplier().get());
    }

    private int verifyTabTransitionedToBackground(TabModel model, Tab originalTab) {
        assertEquals(1, model.getCount());
        Tab placeholder = model.getTabAt(0);
        assertNotNull(placeholder);
        assertNotEquals(originalTab.getId(), placeholder.getId());
        assertEquals(TabModel.INVALID_TAB_INDEX, model.indexOf(originalTab));
        assertTrue(originalTab.getIsOffscreenRenderingSupplier().get());
        assertEquals(1, mBackgroundManager.getBackgroundSessions().size());
        return placeholder.getId();
    }
}
