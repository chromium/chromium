// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

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

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicInstanceHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_bottom_sheet.CoBrowseComponentProvider.TabSelectionDelegate;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetPeekProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Tests for {@link ActorControlCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.GLIC)
public class ActorControlCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int TASK_ID = 123;
    private static final int TAB_ID = 456;
    private static final String CONVERSATION_ID_1 = "conversation_1";
    private static final String CONVERSATION_ID_2 = "conversation_2";
    private static final String TASK_TITLE = "Test Task Title";
    private static final String CONVERSATION_TITLE_1 = "Test Conversation Title 1";
    private static final String CONVERSATION_TITLE_2 = "Test Conversation Title 2";

    @Mock private TabBottomSheetManager mTabBottomSheetManager;
    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorKeyedService;
    @Mock private GlicInstanceHelper mGlicInstanceHelper;
    @Mock private GlicInstanceHelper.Natives mGlicInstanceHelperNatives;
    @Mock private Tab mTab;
    @Mock private ActorTask mActorTask;
    @Mock private TabSelectionDelegate mTabSelectionDelegate;

    private Activity mActivity;
    private ActorControlStateTracker mStateTracker;
    private ActorControlCoordinator mCoordinator;
    private PropertyModel mModel;
    private ActorControlMediator mMediator;
    private SettableMonotonicObservableSupplier<Profile> mProfileSupplier;
    private SettableNullableObservableSupplier<Tab> mTabSupplier;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);
        GlicInstanceHelper.setNativesForTesting(mGlicInstanceHelperNatives);
        when(mGlicInstanceHelperNatives.getForTab(mTab)).thenReturn(mGlicInstanceHelper);

        mProfileSupplier = ObservableSuppliers.createMonotonic();
        mTabSupplier = ObservableSuppliers.createNullable();

        mStateTracker = new ActorControlStateTracker(mProfileSupplier, mTabSupplier);
        mCoordinator =
                new ActorControlCoordinator(
                        mTabBottomSheetManager, mStateTracker, mTabSelectionDelegate);

        mModel = mCoordinator.getModelForTesting();
        mMediator = mCoordinator.getMediatorForTesting();

        ShadowLooper.idleMainLooper();
    }

    private void expectValidProfile() {
        when(mProfile.isNativeInitialized()).thenReturn(true);
        when(mProfile.isOffTheRecord()).thenReturn(false);
    }

    private void expectValidGlicInstance1() {
        when(mGlicInstanceHelper.getConversationTitle()).thenReturn(CONVERSATION_TITLE_1);
        when(mGlicInstanceHelper.getConversationId()).thenReturn(CONVERSATION_ID_1);
        when(mGlicInstanceHelper.getTaskId()).thenReturn(0);
    }

    private void expectValidGlicInstance2() {
        when(mGlicInstanceHelper.getConversationTitle()).thenReturn(CONVERSATION_TITLE_2);
        when(mGlicInstanceHelper.getConversationId()).thenReturn(CONVERSATION_ID_2);
        when(mGlicInstanceHelper.getTaskId()).thenReturn(0);
    }

    private void expectValidActorTask() {
        when(mActorTask.getId()).thenReturn(TASK_ID);
        when(mActorTask.getTitle()).thenReturn(TASK_TITLE);
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mActorKeyedService.getTask(TASK_ID)).thenReturn(mActorTask);
        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.singletonList(mActorTask));
        when(mGlicInstanceHelper.getTaskId()).thenReturn(TASK_ID);
    }

    private void setUpProfileSupplier() {
        expectValidProfile();
        expectValidGlicInstance1();
        mTabSupplier.set(mTab);
        mProfileSupplier.set(mProfile);
        ShadowLooper.idleMainLooper();
    }

    private void setUpProfileSupplierWithRunningTask() {
        expectValidProfile();
        expectValidGlicInstance1();
        mTabSupplier.set(mTab);
        expectValidActorTask();
        mProfileSupplier.set(mProfile);
        ShadowLooper.idleMainLooper();
    }

    private void assertModelPropertiesMatchState(PeekViewUiState state) {
        assertEquals(
                state.getTitleTextAppearanceResId(),
                mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT_APPEARANCE_ID));
        assertEquals(
                state.descriptionResId,
                mModel.get(TabBottomSheetPeekProperties.DESCRIPTION_TEXT_ID));
        assertEquals(
                state.getDescriptionVisibility(),
                mModel.get(TabBottomSheetPeekProperties.DESCRIPTION_VISIBILITY));
        assertEquals(
                state.buttonTextResId,
                mModel.get(TabBottomSheetPeekProperties.ACTION_BUTTON_TEXT_ID));
        assertEquals(
                state.getButtonVisibility(),
                mModel.get(TabBottomSheetPeekProperties.ACTION_BUTTON_VISIBILITY));
        assertEquals(
                state.buttonIconResId,
                mModel.get(TabBottomSheetPeekProperties.ACTION_BUTTON_ICON_ID));
        assertEquals(
                state.buttonBackgroundResId,
                mModel.get(TabBottomSheetPeekProperties.ACTION_BUTTON_BACKGROUND_TINT_ID));
        assertEquals(
                state.iconTintResId,
                mModel.get(TabBottomSheetPeekProperties.ACTION_BUTTON_ICON_TINT_ID));
        assertEquals(
                state.buttonHorizontalPaddingResId,
                mModel.get(TabBottomSheetPeekProperties.ACTION_BUTTON_HORIZONTAL_PADDING_ID));
    }

    private void performActorControlClick() {
        mModel.get(TabBottomSheetPeekProperties.ON_ACTION_BUTTON_CLICKED).run();
    }

    private void performPeekViewClick() {
        mModel.get(TabBottomSheetPeekProperties.ON_PEEK_VIEW_CLICKED).run();
    }

    private void performCloseClick() {
        mModel.get(TabBottomSheetPeekProperties.ON_CLOSE_CLICKED).run();
    }

    @Test
    public void testInitialization() {
        assertNotNull(mModel);
        assertNotNull(mModel.get(TabBottomSheetPeekProperties.ON_ACTION_BUTTON_CLICKED));
        assertNotNull(mModel.get(TabBottomSheetPeekProperties.ON_CLOSE_CLICKED));
        assertEquals(mModel, mCoordinator.getModel());
    }

    @Test
    public void testSetContent_ActingState() {
        expectValidProfile();
        expectValidActorTask();
        mProfileSupplier.set(mProfile);

        String expectedDesc =
                ActorControlMediator.calculateContentDescription(
                        mActivity, TASK_TITLE, PeekViewUiState.ACTING);
        mMediator.setContent(TASK_TITLE, PeekViewUiState.ACTING);

        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertModelPropertiesMatchState(PeekViewUiState.ACTING);
        assertEquals(
                expectedDesc,
                mModel.get(TabBottomSheetPeekProperties.CONTENT_DESCRIPTION_A11Y).get(mActivity));
    }

    @Test
    public void testSetContent_PausedState() {
        expectValidProfile();
        expectValidActorTask();
        mProfileSupplier.set(mProfile);

        String expectedDesc =
                ActorControlMediator.calculateContentDescription(
                        mActivity, TASK_TITLE, PeekViewUiState.PAUSED);
        mMediator.setContent(TASK_TITLE, PeekViewUiState.PAUSED);

        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertModelPropertiesMatchState(PeekViewUiState.PAUSED);
        assertEquals(
                expectedDesc,
                mModel.get(TabBottomSheetPeekProperties.CONTENT_DESCRIPTION_A11Y).get(mActivity));
    }

    @Test
    public void testSetContent_WaitingState() {
        expectValidProfile();
        expectValidActorTask();
        mProfileSupplier.set(mProfile);

        String expectedDesc =
                ActorControlMediator.calculateContentDescription(
                        mActivity, TASK_TITLE, PeekViewUiState.WAITING);
        mMediator.setContent(TASK_TITLE, PeekViewUiState.WAITING);

        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertModelPropertiesMatchState(PeekViewUiState.WAITING);
        assertEquals(
                expectedDesc,
                mModel.get(TabBottomSheetPeekProperties.CONTENT_DESCRIPTION_A11Y).get(mActivity));
    }

    @Test
    public void testSetContent_DefaultState() {
        expectValidProfile();
        expectValidActorTask();
        mProfileSupplier.set(mProfile);

        String expectedDesc =
                ActorControlMediator.calculateContentDescription(
                        mActivity, TASK_TITLE, PeekViewUiState.DEFAULT);
        mMediator.setContent(TASK_TITLE, PeekViewUiState.DEFAULT);

        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertModelPropertiesMatchState(PeekViewUiState.DEFAULT);
        assertEquals(
                expectedDesc,
                mModel.get(TabBottomSheetPeekProperties.CONTENT_DESCRIPTION_A11Y).get(mActivity));
    }

    @Test
    public void testCalculateContentDescription() {
        // Case 1: Both title and description exist
        assertEquals(
                mActivity.getString(
                        R.string.peek_state_accessible_label, "Ask Gemini, Needs your attention"),
                ActorControlMediator.calculateContentDescription(
                        mActivity, "Ask Gemini", PeekViewUiState.WAITING));

        // Case 2: Title only (Description visibility GONE / empty)
        assertEquals(
                mActivity.getString(R.string.peek_state_accessible_label, "Ask Gemini"),
                ActorControlMediator.calculateContentDescription(
                        mActivity, "Ask Gemini", PeekViewUiState.DEFAULT));

        // Case 3: Description only (Title empty)
        assertEquals(
                mActivity.getString(R.string.peek_state_accessible_label, "Needs your attention"),
                ActorControlMediator.calculateContentDescription(
                        mActivity, "", PeekViewUiState.WAITING));

        // Case 4: Neither exist
        assertEquals(
                null,
                ActorControlMediator.calculateContentDescription(
                        mActivity, "", PeekViewUiState.DEFAULT));
    }

    @Test
    public void testTabChanged_observesGlicInstanceHelper() {
        expectValidProfile();
        expectValidGlicInstance1();
        mProfileSupplier.set(mProfile);

        mTabSupplier.set(mTab);

        verify(mGlicInstanceHelper).addObserver(mStateTracker);
        assertEquals(CONVERSATION_TITLE_1, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.DEFAULT, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnInstanceChanged_updatesTitle() {
        expectValidProfile();
        expectValidGlicInstance1();
        mProfileSupplier.set(mProfile);
        mTabSupplier.set(mTab);

        ArgumentCaptor<GlicInstanceHelper.Observer> captor =
                ArgumentCaptor.forClass(GlicInstanceHelper.Observer.class);
        verify(mGlicInstanceHelper).addObserver(captor.capture());
        when(mGlicInstanceHelper.getConversationTitle()).thenReturn(CONVERSATION_TITLE_2);

        captor.getValue().onInstanceChanged();

        assertEquals(CONVERSATION_TITLE_2, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
    }

    @Test
    public void testTabChanged_toNull_clearsContent() {
        expectValidProfile();
        expectValidGlicInstance1();
        mProfileSupplier.set(mProfile);

        mTabSupplier.set(mTab);
        verify(mGlicInstanceHelper).addObserver(mStateTracker);

        mTabSupplier.set(null);
        verify(mGlicInstanceHelper).removeObserver(mStateTracker);
        assertEquals("", mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.DEFAULT, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnProfileAdded_invalidProfile() {
        when(mProfile.isNativeInitialized()).thenReturn(false);
        mProfileSupplier.set(mProfile);

        verify(mActorKeyedService, never()).addObserver(any());
        assertEquals("", mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
    }

    @Test
    public void testOnProfileAdded_withRunningTask() {
        expectValidProfile();
        expectValidGlicInstance1();
        mTabSupplier.set(mTab);
        expectValidActorTask();
        when(mActorTask.getState()).thenReturn(ActorTaskState.ACTING);

        mProfileSupplier.set(mProfile);
        ShadowLooper.idleMainLooper();

        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.ACTING, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnProfileAdded_withoutRunningTask() {
        expectValidProfile();

        mProfileSupplier.set(mProfile);

        assertEquals("", mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.DEFAULT, mCoordinator.getPeekViewUiStateForTesting());
    }

    private void setUpForOnTaskStateChanged() {
        setUpProfileSupplier();
        expectValidActorTask();
    }

    @Test
    public void testOnTaskStateChanged_acting() {
        setUpForOnTaskStateChanged();
        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.ACTING, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_pausedByUser() {
        setUpForOnTaskStateChanged();
        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.PAUSED_BY_USER);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.PAUSED, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_pausedByActor() {
        setUpForOnTaskStateChanged();
        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.PAUSED_BY_ACTOR);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.WAITING, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_waitingOnUser() {
        setUpForOnTaskStateChanged();
        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.WAITING_ON_USER);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.WAITING, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_cancelled() {
        setUpForOnTaskStateChanged();
        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.CANCELLED);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.DEFAULT, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_reflecting() {
        setUpForOnTaskStateChanged();
        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.REFLECTING);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.ACTING, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_created() {
        setUpForOnTaskStateChanged();
        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.CREATED);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.DEFAULT, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_finished() {
        setUpForOnTaskStateChanged();
        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.FINISHED);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.WAITING, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_nullTask_notFinished_defaultsBackToConversationPeekView() {
        setUpProfileSupplier();
        expectValidGlicInstance1();
        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        assertEquals(CONVERSATION_TITLE_1, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.DEFAULT, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_nullTask_finished_keepsTitle() {
        setUpProfileSupplier();
        expectValidActorTask();
        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));

        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.FINISHED);

        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.WAITING, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_nullTask_cancelled_clearsContent() {
        setUpProfileSupplier();
        expectValidActorTask();
        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.CANCELLED);

        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.DEFAULT, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnConversationTitleChanged_updatesTitle() {
        setUpProfileSupplier();
        when(mGlicInstanceHelper.getConversationTitle()).thenReturn(CONVERSATION_TITLE_1);
        mStateTracker.onInstanceChanged();
        assertEquals(CONVERSATION_TITLE_1, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
    }

    @Test
    public void testOnActorControlClick_taskActing_pauses() {
        setUpProfileSupplierWithRunningTask();
        when(mActorTask.getState()).thenReturn(ActorTaskState.ACTING);

        performActorControlClick();

        verify(mActorTask).pause();
        verify(mActorTask, never()).resume();
    }

    @Test
    public void testOnActorControlClick_taskPaused_resumes() {
        setUpProfileSupplierWithRunningTask();
        when(mActorTask.getState()).thenReturn(ActorTaskState.PAUSED_BY_USER);

        performActorControlClick();

        verify(mActorTask, never()).pause();
        verify(mActorTask).resume();
    }

    @Test
    public void testOnActorControlClick_taskReflecting_pauses() {
        setUpProfileSupplierWithRunningTask();
        when(mActorTask.getState()).thenReturn(ActorTaskState.REFLECTING);

        performActorControlClick();

        verify(mActorTask).pause();
        verify(mActorTask, never()).resume();
    }

    @Test
    public void testOnActorControlClick_taskUnhandledState() {
        setUpProfileSupplierWithRunningTask();
        when(mActorTask.getState()).thenReturn(ActorTaskState.CREATED);

        performActorControlClick();

        verify(mActorTask, never()).pause();
        verify(mActorTask, never()).resume();
    }

    @Test
    public void testOnActorControlClick_taskWaitingOnUser_opensBottomSheet() {
        setUpProfileSupplierWithRunningTask();
        when(mActorTask.getState()).thenReturn(ActorTaskState.WAITING_ON_USER);

        performActorControlClick();

        verify(mActorTask, never()).pause();
        verify(mActorTask, never()).resume();
        verify(mTabBottomSheetManager).setSheetExpanded(true);
    }

    @Test
    public void testOnActorControlClick_taskPausedByActor_opensBottomSheet() {
        setUpProfileSupplierWithRunningTask();
        when(mActorTask.getState()).thenReturn(ActorTaskState.PAUSED_BY_ACTOR);

        performActorControlClick();

        verify(mActorTask, never()).pause();
        verify(mActorTask, never()).resume();
        verify(mTabBottomSheetManager).setSheetExpanded(true);
    }

    @Test
    public void testOnActorControlClick_noActiveTask_waitingState_hidesPeekView() {
        setUpProfileSupplier();
        mCoordinator.setPeekViewContentForTesting(TASK_TITLE, PeekViewUiState.WAITING);
        mModel.set(TabBottomSheetPeekProperties.TITLE_TEXT, TASK_TITLE);

        performActorControlClick();

        verify(mTabBottomSheetManager).setSheetExpanded(true);
        assertEquals(CONVERSATION_TITLE_1, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.DEFAULT, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnActorControlClick_noActiveTask_waitingState_triggersCallback() {
        setUpProfileSupplier();
        expectValidActorTask();

        Set<Integer> tabIds = new HashSet<>();
        tabIds.add(TAB_ID);
        when(mActorTask.getLastActedTabs()).thenReturn(tabIds);

        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.emptyList());
        when(mActorKeyedService.getTask(TASK_ID)).thenReturn(null);
        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.FINISHED);

        assertEquals(PeekViewUiState.WAITING, mCoordinator.getPeekViewUiStateForTesting());
        performActorControlClick();

        verify(mTabSelectionDelegate).switchToTab(TAB_ID);
    }

    @Test
    public void testOnActorControlClick_noTabs_doesNotTriggerCallback() {
        setUpProfileSupplierWithRunningTask();

        when(mActorTask.getLastActedTabs()).thenReturn(new HashSet<>());
        when(mActorTask.getTabs()).thenReturn(new HashSet<>());
        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        when(mActorKeyedService.getActiveTasks()).thenReturn(Collections.emptyList());
        when(mActorKeyedService.getTask(TASK_ID)).thenReturn(null);
        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.FINISHED);
        performActorControlClick();

        verify(mTabSelectionDelegate, never()).switchToTab(anyInt());
    }

    @Test
    public void testOnActorControlClick_noActiveTask_notInWaitingState_clearsContent() {
        setUpProfileSupplier();
        mCoordinator.setPeekViewContentForTesting(TASK_TITLE, PeekViewUiState.ACTING);
        mModel.set(TabBottomSheetPeekProperties.TITLE_TEXT, TASK_TITLE);

        performActorControlClick();

        assertEquals("", mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.DEFAULT, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnCloseClick_sheetInitialized_closesBottomSheet() {
        when(mTabBottomSheetManager.isSheetInitialized()).thenReturn(true);

        performCloseClick();

        verify(mTabBottomSheetManager).tryToCloseBottomSheet(/* animate= */ true);
    }

    @Test
    public void testOnCloseClick_recordsMetric() {
        when(mTabBottomSheetManager.isSheetInitialized()).thenReturn(true);
        UserActionTester userActionTester = new UserActionTester();
        try {
            performCloseClick();
            assertEquals(1, userActionTester.getActionCount("Glic.Instance.Close.PeekView"));
        } finally {
            userActionTester.tearDown();
        }
    }

    @Test
    public void testOnPeekViewClick_expandsBottomSheet() {
        setUpProfileSupplier();

        performPeekViewClick();

        verify(mTabBottomSheetManager).setSheetExpanded(true);
    }

    @Test
    public void testOnTaskStateChanged_matchingConversationId() {
        setUpProfileSupplier();
        expectValidGlicInstance1();
        mStateTracker.onInstanceChanged();

        expectValidActorTask();
        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.ACTING, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_nonMatchingConversationId() {
        setUpProfileSupplier();
        expectValidGlicInstance1();
        mStateTracker.onInstanceChanged();

        expectValidActorTask();
        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        assertEquals(PeekViewUiState.ACTING, mCoordinator.getPeekViewUiStateForTesting());

        // Change active instance ID
        expectValidGlicInstance2();
        mStateTracker.onInstanceChanged();

        // State changes for task 1
        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.WAITING_ON_USER);

        // Content should not be updated to WAITING
        assertEquals(PeekViewUiState.DEFAULT, mCoordinator.getPeekViewUiStateForTesting());
        assertEquals(CONVERSATION_TITLE_2, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
    }

    @Test
    public void testOnActiveInstanceChanged_matchingConversationId_updatesContent() {
        setUpProfileSupplierWithRunningTask();
        when(mActorTask.getState()).thenReturn(ActorTaskState.ACTING);
        mStateTracker.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        assertEquals(PeekViewUiState.ACTING, mCoordinator.getPeekViewUiStateForTesting());

        // Switch to non-matching instance
        expectValidGlicInstance2();
        mStateTracker.onInstanceChanged();

        // Should switch to non-matching instance content
        assertEquals(PeekViewUiState.DEFAULT, mCoordinator.getPeekViewUiStateForTesting());
        assertEquals(CONVERSATION_TITLE_2, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));

        // Switch back to matching instance
        expectValidGlicInstance1();
        when(mGlicInstanceHelper.getTaskId()).thenReturn(TASK_ID);
        mStateTracker.onInstanceChanged();

        // Should update to ACTING again
        assertEquals(PeekViewUiState.ACTING, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testTabChanged_betweenValidTabs_updatesObserverAndContent() {
        expectValidProfile();
        mProfileSupplier.set(mProfile);

        GlicInstanceHelper helper1 = mGlicInstanceHelper;
        expectValidGlicInstance1();

        mTabSupplier.set(mTab);
        verify(helper1).addObserver(mStateTracker);
        assertEquals(CONVERSATION_TITLE_1, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));

        Tab tab2 = org.mockito.Mockito.mock(Tab.class);

        GlicInstanceHelper helper2 = org.mockito.Mockito.mock(GlicInstanceHelper.class);
        when(helper2.getConversationId()).thenReturn(CONVERSATION_ID_2);
        when(helper2.getConversationTitle()).thenReturn(CONVERSATION_TITLE_2);
        when(mGlicInstanceHelperNatives.getForTab(tab2)).thenReturn(helper2);

        mTabSupplier.set(tab2);

        verify(helper1).removeObserver(mStateTracker);
        verify(helper2).addObserver(mStateTracker);

        assertEquals(CONVERSATION_TITLE_2, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
    }

    @Test
    public void testTabChanged_initialTabIncognito_doesNotObserveAndClearsContent() {
        expectValidProfile();
        mProfileSupplier.set(mProfile);

        Tab incognitoTab = org.mockito.Mockito.mock(Tab.class);
        when(incognitoTab.isOffTheRecord()).thenReturn(true);

        mTabSupplier.set(incognitoTab);

        verify(mGlicInstanceHelperNatives, never()).getForTab(incognitoTab);

        assertEquals("", mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.DEFAULT, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testPerConversationTaskTracking() {
        setUpProfileSupplier(); // Sets up Tab 1 with Conversation 1 (CONVERSATION_ID_1)
        when(mGlicInstanceHelper.getTaskId()).thenReturn(101);

        // Task 1 on Conversation 1
        ActorTask task1 = org.mockito.Mockito.mock(ActorTask.class);
        when(task1.getId()).thenReturn(101);
        when(task1.getTitle()).thenReturn("Task 1");
        when(task1.getState()).thenReturn(ActorTaskState.ACTING);

        // Task 2 on Conversation 2
        ActorTask task2 = org.mockito.Mockito.mock(ActorTask.class);
        when(task2.getId()).thenReturn(102);
        when(task2.getTitle()).thenReturn("Task 2");
        when(task2.getState()).thenReturn(ActorTaskState.PAUSED_BY_USER);

        // Mock active tasks list
        List<ActorTask> activeTasks = new ArrayList<>();
        activeTasks.add(task1);
        activeTasks.add(task2);
        when(mActorKeyedService.getActiveTasks()).thenReturn(activeTasks);
        when(mActorKeyedService.getTask(101)).thenReturn(task1);
        when(mActorKeyedService.getTask(102)).thenReturn(task2);

        // 1. We are on Tab 1 (Conversation 1).
        // Simulate Task 1 starting. This should register the mapping 101 -> Conversation 1.
        mStateTracker.onTaskStateChanged(101, ActorTaskState.CREATED);
        mStateTracker.onTaskStateChanged(101, ActorTaskState.ACTING);

        assertEquals("Task 1", mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.ACTING, mCoordinator.getPeekViewUiStateForTesting());

        // 2. Switch to Tab 2 (Conversation 2)
        Tab tab2 = org.mockito.Mockito.mock(Tab.class);
        when(tab2.getId()).thenReturn(2);
        GlicInstanceHelper helper2 = org.mockito.Mockito.mock(GlicInstanceHelper.class);
        when(helper2.getConversationId()).thenReturn(CONVERSATION_ID_2);
        when(helper2.getConversationTitle()).thenReturn(CONVERSATION_TITLE_2);
        when(helper2.getTaskId()).thenReturn(102);
        when(mGlicInstanceHelperNatives.getForTab(tab2)).thenReturn(helper2);

        mTabSupplier.set(tab2); // Triggers onInstanceChanged -> mActiveGlicConversationId =
        // CONVERSATION_ID_2

        // Simulate Task 2 starting on Conversation 2.
        mStateTracker.onTaskStateChanged(102, ActorTaskState.CREATED);
        mStateTracker.onTaskStateChanged(102, ActorTaskState.PAUSED_BY_USER);

        assertEquals("Task 2", mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.PAUSED, mCoordinator.getPeekViewUiStateForTesting());

        // 3. Switch back to Tab 1 (Conversation 1) -> should show Task 1 again (matching from map)
        mTabSupplier.set(mTab);
        assertEquals("Task 1", mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.ACTING, mCoordinator.getPeekViewUiStateForTesting());

        // 4. Open Tab 3 and switch to Conversation 1 -> should also show Task 1
        Tab tab3 = org.mockito.Mockito.mock(Tab.class);
        when(tab3.getId()).thenReturn(3);
        // We reuse helper1 (mGlicInstanceHelper) which has CONVERSATION_ID_1
        when(mGlicInstanceHelperNatives.getForTab(tab3)).thenReturn(mGlicInstanceHelper);

        mTabSupplier.set(tab3);
        assertEquals("Task 1", mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.ACTING, mCoordinator.getPeekViewUiStateForTesting());
    }
}
