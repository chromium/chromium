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
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetPeekProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.HashSet;
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
    @Mock private ActorControlCoordinator.TabSelectionDelegate mTabSelectionDelegate;

    private Activity mActivity;
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

        mCoordinator =
                new ActorControlCoordinator(
                        mTabBottomSheetManager,
                        mProfileSupplier,
                        mTabSupplier,
                        mTabSelectionDelegate);

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
    }

    private void expectValidGlicInstance2() {
        when(mGlicInstanceHelper.getConversationTitle()).thenReturn(CONVERSATION_TITLE_2);
        when(mGlicInstanceHelper.getConversationId()).thenReturn(CONVERSATION_ID_2);
    }

    private void expectValidActorTask() {
        when(mActorTask.getId()).thenReturn(TASK_ID);
        when(mActorTask.getTitle()).thenReturn(TASK_TITLE);
        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(mActorTask);
    }

    private void setUpProfileSupplier() {
        expectValidProfile();
        expectValidGlicInstance1();
        mTabSupplier.set(mTab);
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
        assertEquals(
                state.buttonContentDescriptionResId,
                mModel.get(TabBottomSheetPeekProperties.ACTION_BUTTON_CONTENT_DESCRIPTION_ID));
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
        verify(mTabBottomSheetManager).setPeekViewModel(any());
    }

    @Test
    public void testSetContent_ActingState() {
        expectValidProfile();
        expectValidActorTask();
        mProfileSupplier.set(mProfile);

        mMediator.setContent(TASK_TITLE, PeekViewUiState.ACTING);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertModelPropertiesMatchState(PeekViewUiState.ACTING);
    }

    @Test
    public void testSetContent_PausedState() {
        expectValidProfile();
        expectValidActorTask();
        mProfileSupplier.set(mProfile);

        mMediator.setContent(TASK_TITLE, PeekViewUiState.PAUSED);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertModelPropertiesMatchState(PeekViewUiState.PAUSED);
    }

    @Test
    public void testSetContent_WaitingState() {
        expectValidProfile();
        expectValidActorTask();
        mProfileSupplier.set(mProfile);

        mMediator.setContent(TASK_TITLE, PeekViewUiState.WAITING);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertModelPropertiesMatchState(PeekViewUiState.WAITING);
    }

    @Test
    public void testSetContent_DefaultState() {
        expectValidProfile();
        expectValidActorTask();
        mProfileSupplier.set(mProfile);

        mMediator.setContent(TASK_TITLE, PeekViewUiState.DEFAULT);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertModelPropertiesMatchState(PeekViewUiState.DEFAULT);
    }

    @Test
    public void testTabChanged_observesGlicInstanceHelper() {
        expectValidProfile();
        expectValidGlicInstance1();
        mProfileSupplier.set(mProfile);

        mTabSupplier.set(mTab);

        verify(mGlicInstanceHelper).addObserver(mCoordinator);
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
        verify(mGlicInstanceHelper).addObserver(mCoordinator);

        mTabSupplier.set(null);
        verify(mGlicInstanceHelper).removeObserver(mCoordinator);
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
        expectValidActorTask();
        when(mActorTask.getState()).thenReturn(ActorTaskState.ACTING);

        mProfileSupplier.set(mProfile);

        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.ACTING, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnProfileAdded_withoutRunningTask() {
        expectValidProfile();
        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(null);

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
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.ACTING, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_pausedByUser() {
        setUpForOnTaskStateChanged();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.PAUSED_BY_USER);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.PAUSED, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_pausedByActor() {
        setUpForOnTaskStateChanged();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.PAUSED_BY_ACTOR);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.WAITING, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_waitingOnUser() {
        setUpForOnTaskStateChanged();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.WAITING_ON_USER);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.WAITING, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_cancelled() {
        setUpForOnTaskStateChanged();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.CANCELLED);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.DEFAULT, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_reflecting() {
        setUpForOnTaskStateChanged();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.REFLECTING);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.ACTING, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_created() {
        setUpForOnTaskStateChanged();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.CREATED);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.DEFAULT, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_finished() {
        setUpForOnTaskStateChanged();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.FINISHED);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.WAITING, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_nullTask_notFinished_defaultsBackToConversationPeekView() {
        setUpProfileSupplier();
        expectValidGlicInstance1();
        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(null);

        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        assertEquals(CONVERSATION_TITLE_1, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.DEFAULT, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_nullTask_finished_keepsTitle() {
        setUpProfileSupplier();
        expectValidActorTask();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);
        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));

        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(null);

        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.FINISHED);

        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.WAITING, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_nullTask_cancelled_clearsContent() {
        setUpProfileSupplier();
        expectValidActorTask();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(null);

        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.CANCELLED);

        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.DEFAULT, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnConversationTitleChanged_updatesTitle() {
        setUpProfileSupplier();
        when(mGlicInstanceHelper.getConversationTitle()).thenReturn(CONVERSATION_TITLE_1);
        mCoordinator.onInstanceChanged();
        assertEquals(CONVERSATION_TITLE_1, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
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
        verify(mTabBottomSheetManager).setSheetExpanded(true);
    }

    @Test
    public void testOnActorControlClick_taskPausedByActor_opensBottomSheet() {
        setUpProfileSupplier();
        expectValidActorTask();
        when(mActorTask.getState()).thenReturn(ActorTaskState.PAUSED_BY_ACTOR);

        performActorControlClick();

        verify(mActorTask, never()).pause();
        verify(mActorTask, never()).resume();
        verify(mTabBottomSheetManager).setSheetExpanded(true);
    }

    @Test
    public void testOnActorControlClick_noActiveTask_waitingState_hidesPeekView() {
        setUpProfileSupplier();
        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(null);
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

        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(null);
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.FINISHED);

        assertEquals(PeekViewUiState.WAITING, mCoordinator.getPeekViewUiStateForTesting());
        performActorControlClick();

        verify(mTabSelectionDelegate).switchToTab(TAB_ID);
    }

    @Test
    public void testOnActorControlClick_noTabs_doesNotTriggerCallback() {
        setUpProfileSupplier();
        expectValidActorTask();

        when(mActorTask.getLastActedTabs()).thenReturn(new HashSet<>());
        when(mActorTask.getTabs()).thenReturn(new HashSet<>());
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(null);
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.FINISHED);
        performActorControlClick();

        verify(mTabSelectionDelegate, never()).switchToTab(anyInt());
    }

    @Test
    public void testOnActorControlClick_noActiveTask_notInWaitingState_clearsContent() {
        setUpProfileSupplier();
        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(null);
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
        mCoordinator.onInstanceChanged();

        expectValidActorTask();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        assertEquals(TASK_TITLE, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        assertEquals(PeekViewUiState.ACTING, mCoordinator.getPeekViewUiStateForTesting());
    }

    @Test
    public void testOnTaskStateChanged_nonMatchingConversationId() {
        setUpProfileSupplier();
        expectValidGlicInstance1();
        mCoordinator.onInstanceChanged();

        expectValidActorTask();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        assertEquals(PeekViewUiState.ACTING, mCoordinator.getPeekViewUiStateForTesting());

        // Change active instance ID
        expectValidGlicInstance2();
        mCoordinator.onInstanceChanged();

        // State changes for task 1
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.WAITING_ON_USER);

        // Content should not be updated to WAITING
        assertEquals(PeekViewUiState.DEFAULT, mCoordinator.getPeekViewUiStateForTesting());
        assertEquals(CONVERSATION_TITLE_2, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));
    }

    @Test
    public void testOnActiveInstanceChanged_matchingConversationId_updatesContent() {
        setUpProfileSupplier();
        expectValidGlicInstance1();
        mCoordinator.onInstanceChanged();

        expectValidActorTask();
        when(mActorTask.getState()).thenReturn(ActorTaskState.ACTING);
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);

        assertEquals(PeekViewUiState.ACTING, mCoordinator.getPeekViewUiStateForTesting());

        // Switch to non-matching instance
        expectValidGlicInstance2();
        mCoordinator.onInstanceChanged();

        // Should switch to non-matching instance content
        assertEquals(PeekViewUiState.DEFAULT, mCoordinator.getPeekViewUiStateForTesting());
        assertEquals(CONVERSATION_TITLE_2, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));

        // Switch back to matching instance
        expectValidGlicInstance1();
        mCoordinator.onInstanceChanged();

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
        verify(helper1).addObserver(mCoordinator);
        assertEquals(CONVERSATION_TITLE_1, mModel.get(TabBottomSheetPeekProperties.TITLE_TEXT));

        Tab tab2 = org.mockito.Mockito.mock(Tab.class);

        GlicInstanceHelper helper2 = org.mockito.Mockito.mock(GlicInstanceHelper.class);
        when(helper2.getConversationId()).thenReturn(CONVERSATION_ID_2);
        when(helper2.getConversationTitle()).thenReturn(CONVERSATION_TITLE_2);
        when(mGlicInstanceHelperNatives.getForTab(tab2)).thenReturn(helper2);

        mTabSupplier.set(tab2);

        verify(helper1).removeObserver(mCoordinator);
        verify(helper2).addObserver(mCoordinator);

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
}
