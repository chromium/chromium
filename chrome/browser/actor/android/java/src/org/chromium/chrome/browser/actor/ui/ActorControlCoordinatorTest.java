// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import com.google.android.material.button.MaterialButton;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChromeImageButton;

/** Tests for {@link ActorControlCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.GLIC)
public class ActorControlCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int TASK_ID = 123;
    private static final String TASK_TITLE = "Test Task Title";

    @Mock private TabBottomSheetManager mTabBottomSheetManager;
    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorKeyedService;
    @Mock private ActorTask mActorTask;

    private Activity mActivity;
    private ActorControlCoordinator mCoordinator;
    private PropertyModel mModel;
    private ActorControlMediator mMediator;
    private SettableMonotonicObservableSupplier<Profile> mProfileSupplier;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);
        mProfileSupplier = ObservableSuppliers.createMonotonic();

        mCoordinator =
                new ActorControlCoordinator(
                        mActivity,
                        mTabBottomSheetManager,
                        mProfileSupplier);

        mModel = mCoordinator.getModelForTesting();
        mMediator = mCoordinator.getMediatorForTesting();

        ShadowLooper.idleMainLooper();
    }

    private void expectValidProfile() {
        when(mProfile.isNativeInitialized()).thenReturn(true);
        when(mProfile.isOffTheRecord()).thenReturn(false);
    }

    private void expectValidActorTask() {
        when(mActorTask.getId()).thenReturn(TASK_ID);
        when(mActorTask.getTitle()).thenReturn(TASK_TITLE);
        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(mActorTask);
    }

    private void setUpProfileSupplier() {
        expectValidProfile();
        mProfileSupplier.set(mProfile);
        ShadowLooper.idleMainLooper();
    }

    private void performActorControlClick() {
        ActorControlView view = (ActorControlView) mCoordinator.getPeekViewForTesting();
        assertNotNull("Peek view should be attached", view);
        MaterialButton actorControlButton = view.findViewById(R.id.actor_control_button);
        actorControlButton.performClick();
    }

    private void performPeekViewClick() {
        ActorControlView view = (ActorControlView) mCoordinator.getPeekViewForTesting();
        assertNotNull("Peek view should be attached", view);
        view.performClick();
    }

    private void performCloseClick() {
        ActorControlView view = (ActorControlView) mCoordinator.getPeekViewForTesting();
        assertNotNull("Peek view should be attached", view);
        ChromeImageButton closeButton = view.findViewById(R.id.actor_control_close_button);
        closeButton.performClick();
    }

    @Test
    public void testInitialization() {
        assertNotNull(mModel);
        assertNotNull(mModel.get(ActorControlProperties.ON_ACTOR_CONTROL_CLICKED));
        assertNotNull(mModel.get(ActorControlProperties.ON_CLOSE_CLICKED));
        verify(mTabBottomSheetManager).setPeekView(any());
    }

    @Test
    public void testSetContent_ActingState() {
        expectValidProfile();
        expectValidActorTask();
        mProfileSupplier.set(mProfile);

        mMediator.setContent(TASK_TITLE, PeekViewUiState.ACTING);
        ActorControlView view = (ActorControlView) mCoordinator.getPeekViewForTesting();

        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(PeekViewUiState.ACTING, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
        assertEquals(
                view.getStepDescriptionForTesting(),
                PeekViewUiState.ACTING.getDescription(view.getContext()));
    }

    @Test
    public void testSetContent_PausedState() {
        expectValidProfile();
        expectValidActorTask();
        mProfileSupplier.set(mProfile);

        mMediator.setContent(TASK_TITLE, PeekViewUiState.PAUSED);
        ActorControlView view = (ActorControlView) mCoordinator.getPeekViewForTesting();

        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(PeekViewUiState.PAUSED, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
        assertEquals(
                view.getStepDescriptionForTesting(),
                PeekViewUiState.PAUSED.getDescription(view.getContext()));
    }

    @Test
    public void testSetContent_WaitingState() {
        expectValidProfile();
        expectValidActorTask();
        mProfileSupplier.set(mProfile);

        mMediator.setContent(TASK_TITLE, PeekViewUiState.WAITING);
        ActorControlView view = (ActorControlView) mCoordinator.getPeekViewForTesting();

        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(
                PeekViewUiState.WAITING, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
        assertEquals(
                view.getStepDescriptionForTesting(),
                PeekViewUiState.WAITING.getDescription(view.getContext()));
    }

    @Test
    public void testSetContent_DefaultState() {
        expectValidProfile();
        expectValidActorTask();
        mProfileSupplier.set(mProfile);

        mMediator.setContent(TASK_TITLE, PeekViewUiState.DEFAULT);
        ActorControlView view = (ActorControlView) mCoordinator.getPeekViewForTesting();

        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(
                PeekViewUiState.DEFAULT, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
        assertEquals(
                view.getStepDescriptionForTesting(),
                PeekViewUiState.DEFAULT.getDescription(view.getContext()));
    }

    @Test
    public void testOnProfileAdded_validProfile_withTask() {
        expectValidProfile();
        when(mActorTask.getState()).thenReturn(ActorTaskState.ACTING);
        expectValidActorTask();

        mProfileSupplier.set(mProfile);

        verify(mActorKeyedService).addObserver(mCoordinator);
        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(PeekViewUiState.ACTING, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testOnProfileAdded_validProfile_noTask() {
        expectValidProfile();
        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(null);

        mProfileSupplier.set(mProfile);

        verify(mActorKeyedService).addObserver(mCoordinator);
        assertEquals("", mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(
                PeekViewUiState.DEFAULT, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testOnProfileAdded_nonValidProfile() {
        when(mProfile.isNativeInitialized()).thenReturn(false);
        mProfileSupplier.set(mProfile);

        verify(mActorKeyedService, never()).addObserver(any());
        assertEquals("", mModel.get(ActorControlProperties.TASK_TITLE));
    }

    private void setUpForOnTaskStateChanged() {
        setUpProfileSupplier();
        expectValidActorTask();
    }

    @Test
    public void testOnTaskStateChanged_acting() {
        setUpForOnTaskStateChanged();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);
        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(PeekViewUiState.ACTING, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testOnTaskStateChanged_pausedByUser() {
        setUpForOnTaskStateChanged();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.PAUSED_BY_USER);
        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(PeekViewUiState.PAUSED, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testOnTaskStateChanged_pausedByActor() {
        setUpForOnTaskStateChanged();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.PAUSED_BY_ACTOR);
        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(
                PeekViewUiState.WAITING, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testOnTaskStateChanged_waitingOnUser() {
        setUpForOnTaskStateChanged();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.WAITING_ON_USER);
        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(
                PeekViewUiState.WAITING, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testOnTaskStateChanged_cancelled() {
        setUpForOnTaskStateChanged();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.CANCELLED);
        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(
                PeekViewUiState.DEFAULT, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testOnTaskStateChanged_reflecting() {
        setUpForOnTaskStateChanged();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.REFLECTING);
        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(PeekViewUiState.ACTING, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testOnTaskStateChanged_created() {
        setUpForOnTaskStateChanged();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.CREATED);
        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(
                PeekViewUiState.DEFAULT, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testOnTaskStateChanged_finished() {
        setUpForOnTaskStateChanged();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.FINISHED);
        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(
                PeekViewUiState.WAITING, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testOnTaskStateChanged_nullTask_notFinished_clearsContent() {
        setUpProfileSupplier();
        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(null);

        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        assertEquals("", mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(
                PeekViewUiState.DEFAULT, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testOnTaskStateChanged_nullTask_finished_keepsTitle() {
        setUpProfileSupplier();
        expectValidActorTask();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);
        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));

        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(null);

        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.FINISHED);

        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(
                PeekViewUiState.WAITING, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testOnTaskStateChanged_nullTask_cancelled_clearsContent() {
        setUpProfileSupplier();
        expectValidActorTask();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(null);

        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.CANCELLED);

        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(
                PeekViewUiState.DEFAULT, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testOnActorControlClick_taskActing_pauses() {
        setUpProfileSupplier();
        expectValidActorTask();
        when(mActorTask.getState()).thenReturn(ActorTaskState.ACTING);

        performActorControlClick();

        verify(mActorTask).pause();
        verify(mActorTask, never()).resume();
    }

    @Test
    public void testOnActorControlClick_taskPaused_resumes() {
        setUpProfileSupplier();
        expectValidActorTask();
        when(mActorTask.getState()).thenReturn(ActorTaskState.PAUSED_BY_USER);

        performActorControlClick();

        verify(mActorTask, never()).pause();
        verify(mActorTask).resume();
    }

    @Test
    public void testOnActorControlClick_taskReflecting_pauses() {
        setUpProfileSupplier();
        expectValidActorTask();
        when(mActorTask.getState()).thenReturn(ActorTaskState.REFLECTING);

        performActorControlClick();

        verify(mActorTask).pause();
        verify(mActorTask, never()).resume();
    }

    @Test
    public void testOnActorControlClick_taskUnhandledState() {
        setUpProfileSupplier();
        expectValidActorTask();
        when(mActorTask.getState()).thenReturn(ActorTaskState.CREATED);

        performActorControlClick();

        verify(mActorTask, never()).pause();
        verify(mActorTask, never()).resume();
    }

    @Test
    public void testOnActorControlClick_taskWaitingOnUser_opensBottomSheet() {
        setUpProfileSupplier();
        expectValidActorTask();
        when(mActorTask.getState()).thenReturn(ActorTaskState.WAITING_ON_USER);

        performActorControlClick();

        verify(mActorTask, never()).pause();
        verify(mActorTask, never()).resume();
        verify(mTabBottomSheetManager).hidePeekViewAndShowExpandedContent();
    }

    @Test
    public void testOnActorControlClick_taskPausedByActor_opensBottomSheet() {
        setUpProfileSupplier();
        expectValidActorTask();
        when(mActorTask.getState()).thenReturn(ActorTaskState.PAUSED_BY_ACTOR);

        performActorControlClick();

        verify(mActorTask, never()).pause();
        verify(mActorTask, never()).resume();
        verify(mTabBottomSheetManager).hidePeekViewAndShowExpandedContent();
    }

    @Test
    public void testOnActorControlClick_noActiveTask_waitingState_hidesPeekView() {
        setUpProfileSupplier();
        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(null);
        mModel.set(ActorControlProperties.PEEK_VIEW_UI_STATE, PeekViewUiState.WAITING);

        performActorControlClick();

        verify(mTabBottomSheetManager).hidePeekViewAndShowExpandedContent();
        assertEquals(
                PeekViewUiState.DEFAULT, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testOnActorControlClick_noActiveTask_notInWaitingState_clearsContent() {
        setUpProfileSupplier();
        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(null);
        mModel.set(ActorControlProperties.PEEK_VIEW_UI_STATE, PeekViewUiState.ACTING);
        mModel.set(ActorControlProperties.TASK_TITLE, TASK_TITLE);

        performActorControlClick();

        assertEquals("", mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(
                PeekViewUiState.DEFAULT, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testOnCloseClick_sheetInitialized_closesBottomSheet() {
        when(mTabBottomSheetManager.isSheetInitialized()).thenReturn(true);

        performCloseClick();

        verify(mTabBottomSheetManager).tryToCloseBottomSheet(/* animate= */ true);
    }

    @Test
    public void testOnPeekViewClick_expandsBottomSheet() {
        setUpProfileSupplier();

        performPeekViewClick();

        verify(mTabBottomSheetManager).hidePeekViewAndShowExpandedContent();
    }
}
