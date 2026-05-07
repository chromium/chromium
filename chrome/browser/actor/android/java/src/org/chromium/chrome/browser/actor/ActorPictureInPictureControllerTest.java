// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.PictureInPictureParams;
import android.app.RemoteAction;
import android.content.Intent;
import android.util.Size;
import android.widget.FrameLayout;

import androidx.activity.ComponentActivity;
import androidx.core.pip.PictureInPictureDelegate;

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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.actor.ui.ActorPictureInPictureOverlayCoordinator;
import org.chromium.chrome.browser.actor.ui.R;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedServiceFactory;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

/** Unit tests for {@link ActorPictureInPictureController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ActorPictureInPictureControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorService;
    @Mock private ActorKeyedServiceFactory.Natives mActorKeyedServiceFactoryJni;
    @Mock private GlicKeyedService mGlicKeyedService;
    @Mock private ActorPictureInPictureOverlayCoordinator mMockCoordinator;
    @Mock private ProfileResolver.Natives mProfileResolverJni;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Callback<Boolean> mToggleGlicCallback;
    @Mock private OffscreenRenderingManager mOffscreenRenderingManager;
    @Mock private Callback<Boolean> mOnPipChangedCallback;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private WindowAndroid mWindowAndroid;

    private ComponentActivity mActivity;
    private Supplier<Profile> mProfileSupplier;
    private ActorPictureInPictureController mController;

    @Before
    public void setUp() {
        ActorKeyedServiceFactoryJni.setInstanceForTesting(mActorKeyedServiceFactoryJni);
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);
        OffscreenRenderingManager.setInstanceForTesting(mOffscreenRenderingManager);

        ComponentActivity realActivity =
                Robolectric.buildActivity(ComponentActivity.class).create().get();
        realActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mActivity = spy(realActivity);

        FrameLayout contentView = new FrameLayout(mActivity);
        contentView.setId(android.R.id.content);
        mActivity.setContentView(contentView);

        mProfileSupplier = () -> mProfile;

        mController =
                new ActorPictureInPictureController(
                        mActivity,
                        mProfileSupplier,
                        () -> mActivity.findViewById(android.R.id.content),
                        () -> mTabModelSelector,
                        () -> {},
                        mToggleGlicCallback,
                        new Size(1920, 1080),
                        mOnPipChangedCallback);
        mController.setOverlayCoordinatorForTesting(mMockCoordinator);

        ProfileResolverJni.setInstanceForTesting(mProfileResolverJni);
        when(mProfileResolverJni.tokenizeProfile(any())).thenReturn("test_token");

        ActorKeyedServiceFactory.setForTesting(mActorService);

        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mTabModelSelector.getTabById(anyInt())).thenReturn(mTab);
    }

    private ActorTask createMockActorTask(int taskId, String title, @ActorTaskState int state) {
        ActorTask mockTask = mock(ActorTask.class);
        when(mockTask.getId()).thenReturn(taskId);
        when(mockTask.getTitle()).thenReturn(title);
        when(mockTask.getState()).thenReturn(state);
        when(mockTask.getLastActedTabs()).thenReturn(Collections.singleton(1));
        when(mActorService.getCurrentActiveTask()).thenReturn(mockTask);
        when(mActorService.getActiveTasksCount()).thenReturn(1);
        when(mActorService.getTask(taskId)).thenReturn(mockTask);
        return mockTask;
    }

    @Test
    public void testShouldEnterPip_NoService() {
        ActorKeyedServiceFactory.setForTesting(null);
        assertFalse(mController.shouldEnterPip());
    }

    @Test
    public void testShouldEnterPip_NoActiveTasks() {
        when(mActorService.getActiveTasksCount()).thenReturn(0);
        assertFalse(mController.shouldEnterPip());
    }

    @Test
    public void testShouldEnterPip_ActiveTasks() {
        when(mActorService.getActiveTasksCount()).thenReturn(1);

        assertTrue(mController.shouldEnterPip());
    }

    @Test
    public void testOnPictureInPictureEvent_Entered_ShowsOverlay() {
        createMockActorTask(101, "Test Title", ActorTaskState.ACTING);
        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.ENTERED, null);

        verify(mMockCoordinator).setVisibility(true);
        verify(mMockCoordinator).updateTitle("Test Title");
        verify(mMockCoordinator).updateStatus(ActorTaskState.ACTING);
        verify(mMockCoordinator, never()).destroy();
    }

    @Test
    public void testOnPictureInPictureEvent_Exited_HidesOverview() {
        Runnable mockHideTabSwitcher = mock(Runnable.class);
        mController =
                new ActorPictureInPictureController(
                        mActivity,
                        mProfileSupplier,
                        () -> mActivity.findViewById(android.R.id.content),
                        () -> mTabModelSelector,
                        mockHideTabSwitcher,
                        mToggleGlicCallback,
                        new Size(1920, 1080),
                        mOnPipChangedCallback);

        ActorTask mockTask = createMockActorTask(101, "Task", ActorTaskState.ACTING);
        when(mockTask.getLastActedTabs()).thenReturn(Collections.singleton(1));
        Tab mockTab = mock(Tab.class);
        when(mTabModelSelector.getTabById(1)).thenReturn(mockTab);

        mController.setInActorPiPForTesting(true);
        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.EXITED, null);

        // Allow the delayed tab selection task to run.
        ShadowLooper.idleMainLooper();

        verify(mockHideTabSwitcher).run();
        verify(mToggleGlicCallback).onResult(true);
    }

    @Test
    public void testOnPictureInPictureEvent_Exited_HidesOverlay() {
        mController.setInActorPiPForTesting(true);
        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.EXITED, null);
        verify(mMockCoordinator).setVisibility(false);
    }

    @Test
    public void testExitPip_WithNonActorIntent_SkipsTabSelection() {
        Runnable mockHideTabSwitcher = mock(Runnable.class);
        mController =
                new ActorPictureInPictureController(
                        mActivity,
                        mProfileSupplier,
                        () -> mActivity.findViewById(android.R.id.content),
                        () -> mTabModelSelector,
                        mockHideTabSwitcher,
                        mToggleGlicCallback,
                        new Size(1920, 1080),
                        mOnPipChangedCallback);

        // Enter PiP
        mController.setInActorPiPForTesting(true);

        // Receive a non-actor intent while in PiP
        Intent nonActorIntent = new Intent();
        nonActorIntent.putExtra(ActorNotificationFactory.EXTRA_SHOW_ACTOR_CONTROL, false);
        mController.onNewIntent(nonActorIntent);

        // Exit PiP
        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.EXITED, null);

        // Run delayed task
        ShadowLooper.idleMainLooper();

        // Verify tab selection was skipped
        verify(mockHideTabSwitcher, never()).run();
        verify(mToggleGlicCallback, never()).onResult(any());
    }

    @Test
    public void testExitPip_ManualExpand_PerformsTabSelection() {
        Runnable mockHideTabSwitcher = mock(Runnable.class);
        mController =
                new ActorPictureInPictureController(
                        mActivity,
                        mProfileSupplier,
                        () -> mActivity.findViewById(android.R.id.content),
                        () -> mTabModelSelector,
                        mockHideTabSwitcher,
                        mToggleGlicCallback,
                        new Size(1920, 1080),
                        mOnPipChangedCallback);

        createMockActorTask(101, "Task", ActorTaskState.ACTING);

        // Enter PiP
        mController.setInActorPiPForTesting(true);

        // Exit PiP (No intent received -> Manual expand)
        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.EXITED, null);

        // Run delayed task
        ShadowLooper.idleMainLooper();

        // Verify tab selection was performed
        verify(mockHideTabSwitcher).run();
        verify(mToggleGlicCallback).onResult(true);
    }

    @Test
    public void testExitPip_WithActorIntent_PerformsTabSelection() {
        Runnable mockHideTabSwitcher = mock(Runnable.class);
        mController =
                new ActorPictureInPictureController(
                        mActivity,
                        mProfileSupplier,
                        () -> mActivity.findViewById(android.R.id.content),
                        () -> mTabModelSelector,
                        mockHideTabSwitcher,
                        mToggleGlicCallback,
                        new Size(1920, 1080),
                        mOnPipChangedCallback);

        createMockActorTask(101, "Task", ActorTaskState.ACTING);

        // Enter PiP
        mController.setInActorPiPForTesting(true);

        // Receive an actor intent while in PiP
        Intent actorIntent = new Intent();
        actorIntent.putExtra(ActorNotificationFactory.EXTRA_SHOW_ACTOR_CONTROL, true);
        mController.onNewIntent(actorIntent);

        // Exit PiP
        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.EXITED, null);

        // Run delayed task
        ShadowLooper.idleMainLooper();

        // Verify tab selection was performed
        verify(mockHideTabSwitcher).run();
        verify(mToggleGlicCallback).onResult(true);
    }

    @Test
    public void testEnterPip_CancelsPendingTabSelection() {
        Runnable mockHideTabSwitcher = mock(Runnable.class);
        mController =
                new ActorPictureInPictureController(
                        mActivity,
                        mProfileSupplier,
                        () -> mActivity.findViewById(android.R.id.content),
                        () -> mTabModelSelector,
                        mockHideTabSwitcher,
                        mToggleGlicCallback,
                        new Size(1920, 1080),
                        mOnPipChangedCallback);

        createMockActorTask(101, "Task", ActorTaskState.ACTING);

        // Enter PiP
        mController.setInActorPiPForTesting(true);

        // Exit PiP -> Posts tab selection
        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.EXITED, null);

        // Immediately Enter PiP again
        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.ENTERED, null);

        // Run delayed tasks
        ShadowLooper.idleMainLooper();

        // Verify tab selection was NOT performed
        verify(mockHideTabSwitcher, never()).run();
        verify(mToggleGlicCallback, never()).onResult(any());
    }

    @Test
    public void testOnTaskStateChanged_UpdatesOverlayWhenInPip() {
        createMockActorTask(101, "Shopping Task", ActorTaskState.CREATED);

        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.ENTERED, null);

        verify(mMockCoordinator).updateTitle("Shopping Task");

        // Change state
        int newState = ActorTaskState.ACTING;
        mController.onTaskStateChanged(123, newState);

        verify(mMockCoordinator).updateStatus(newState);
        verify(mMockCoordinator, times(1)).updateTitle(any());
    }

    @Test
    public void testAttemptPictureInPicture_Success() {
        when(mActorService.getActiveTasksCount()).thenReturn(1);
        doNothing().when(mActivity).enterPictureInPictureMode();

        mController.attemptPictureInPicture();

        verify(mActivity).enterPictureInPictureMode();
    }

    @Test
    public void testAttemptPictureInPicture_NoTasks() {

        mController.attemptPictureInPicture();

        verify(mActivity, never()).enterPictureInPictureMode();
    }

    @Test
    public void testDestroy_StopsTasksAndRemovesObserver() {
        mController.shouldEnterPip(); // Initialize service
        ActorTask mockTask = createMockActorTask(101, "Test Title", ActorTaskState.ACTING);
        when(mActorService.getActiveTasks()).thenReturn(Collections.singletonList(mockTask));

        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.ENTERED, null);
        mController.destroy();

        verify(mMockCoordinator).destroy();
        verify(mActorService).stopTask(101, StoppedReason.STOPPED_BY_USER);
        verify(mActorService).removeObserver(mController);
    }

    @Test
    public void testOnTaskStateChanged_ExitsPipWhenNoTasks_Delayed() {
        mController.setOverlayCoordinatorForTesting(mMockCoordinator);

        // Enter PiP
        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.ENTERED, null);

        // Task finishes
        ActorTask mockTask = createMockActorTask(101, "Test Title", ActorTaskState.FINISHED);
        when(mockTask.isCompleted()).thenReturn(true);
        when(mActorService.getTask(101)).thenReturn(mockTask);
        when(mActorService.getActiveTasksCount()).thenReturn(0);

        mController.onTaskStateChanged(101, ActorTaskState.FINISHED);

        // Should NOT exit immediately
        verify(mActivity, never()).moveTaskToBack(true);

        // Advance time by 1 hour
        ShadowLooper.idleMainLooper(1, TimeUnit.MINUTES);

        // Now it should exit
        verify(mActivity).moveTaskToBack(true);
        verify(mMockCoordinator).setVisibility(false);
    }

    @Test
    public void testCreateIntentForPauseResumeAction() {
        int taskId = 42;
        String action = "test_action";

        Intent intent = mController.createIntentForPauseResumeAction(taskId, action);

        assertEquals(action, intent.getAction());
        assertEquals(mActivity.getPackageName(), intent.getPackage());
        assertEquals(taskId, intent.getIntExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, -1));
    }

    @Test
    public void testUpdatePausePlayActions_ShowingPause() {
        createMockActorTask(101, "Task", ActorTaskState.ACTING);
        mController.updatePipState();

        ArgumentCaptor<PictureInPictureParams> captor =
                ArgumentCaptor.forClass(PictureInPictureParams.class);
        verify(mActivity).setPictureInPictureParams(captor.capture());

        List<RemoteAction> actions = captor.getValue().getActions();
        assertEquals(1, actions.size());
        assertEquals(
                mActivity.getString(R.string.actor_pip_working_status), actions.get(0).getTitle());
    }

    @Test
    public void testUpdatePausePlayActions_ShowingPlay() {
        createMockActorTask(101, "Task", ActorTaskState.PAUSED_BY_USER);

        mController.updatePipState();

        ArgumentCaptor<PictureInPictureParams> captor =
                ArgumentCaptor.forClass(PictureInPictureParams.class);
        verify(mActivity).setPictureInPictureParams(captor.capture());

        List<RemoteAction> actions = captor.getValue().getActions();
        assertEquals(1, actions.size());
        assertEquals(
                mActivity.getString(R.string.actor_pip_paused_status), actions.get(0).getTitle());
    }

    @Test
    public void testGetActiveTaskLastActedTabId() {
        // Case 1: Service is null
        ActorKeyedServiceFactory.setForTesting(null);
        assertEquals(Tab.INVALID_TAB_ID, mController.getActiveTaskLastActedTabId());
        ActorKeyedServiceFactory.setForTesting(mActorService);

        // Case 2: No active task
        when(mActorService.getCurrentActiveTask()).thenReturn(null);
        assertEquals(Tab.INVALID_TAB_ID, mController.getActiveTaskLastActedTabId());

        // Case 3: Task has no last acted tabs
        ActorTask mockTask = createMockActorTask(101, "Task", ActorTaskState.ACTING);
        when(mockTask.getLastActedTabs()).thenReturn(Collections.emptySet());
        assertEquals(Tab.INVALID_TAB_ID, mController.getActiveTaskLastActedTabId());

        // Case 4: Task has last acted tabs
        when(mockTask.getLastActedTabs()).thenReturn(Collections.singleton(1));
        assertEquals(1, mController.getActiveTaskLastActedTabId());
    }

    @Test
    public void testEnterPip_StartsOffscreenRendering() {
        createMockActorTask(101, "Test Title", ActorTaskState.ACTING);
        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.ENTERED, null);

        verify(mOffscreenRenderingManager).startOffscreenRendering(mTab, 1920, 1080);
        verify(mOnPipChangedCallback).onResult(true);
    }

    @Test
    public void testExitPip_StopsOffscreenRendering() {
        createMockActorTask(101, "Test Title", ActorTaskState.ACTING);
        // Enter first to set mActingTab and mOriginalWindow
        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.ENTERED, null);

        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.EXITED, null);

        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
        verify(mOnPipChangedCallback).onResult(false);
    }

    @Test
    public void testOnTaskStateChanged_Completed_StopsOffscreenRendering() {
        createMockActorTask(101, "Test Title", ActorTaskState.ACTING);
        mController.onPictureInPictureEvent(PictureInPictureDelegate.Event.ENTERED, null);

        // Task finishes
        ActorTask mockTask = createMockActorTask(101, "Test Title", ActorTaskState.FINISHED);
        when(mockTask.isCompleted()).thenReturn(true);
        when(mActorService.getTask(101)).thenReturn(mockTask);

        mController.onTaskStateChanged(101, ActorTaskState.FINISHED);

        verify(mOffscreenRenderingManager).stopOffscreenRendering(mTab);
        verify(mOnPipChangedCallback).onResult(false);
    }
}
