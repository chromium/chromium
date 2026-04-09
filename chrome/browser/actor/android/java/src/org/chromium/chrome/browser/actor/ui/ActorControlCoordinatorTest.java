// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.UiTabState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
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

    @Captor private ArgumentCaptor<ActorUiTabController.Observer> mActorObserverCaptor;

    @Mock private ActorUiTabController mActorUiTabController;
    @Mock private TabBottomSheetManager mTabBottomSheetManager;
    @Mock private View.OnClickListener mPlayPauseListener;
    @Mock private View.OnClickListener mCloseListener;
    @Mock private Tab mTab;
    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorKeyedService;
    @Mock private ActorTask mActorTask;

    private Activity mActivity;
    private ActorControlCoordinator mCoordinator;
    private PropertyModel mModel;
    private ActorControlMediator mMediator;
    private SettableNullableObservableSupplier<Tab> mTabSupplier;
    private SettableMonotonicObservableSupplier<Profile> mProfileSupplier;
    private UserDataHost mUserDataHost;

    // Helper method to create UiTabState instances
    private UiTabState createUiTabState(boolean isActive) {
        return new UiTabState(
                /* tabId= */ 0,
                /* actorOverlay= */ new ActorUiTabController.ActorOverlayState(
                        isActive, false, false),
                /* handoffButton= */ new ActorUiTabController.HandoffButtonState(isActive, 0),
                /* tabIndicator= */ 0,
                /* borderGlowVisible= */ false);
    }

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mActorObserverCaptor = ArgumentCaptor.forClass(ActorUiTabController.Observer.class);
        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);
        mUserDataHost = new UserDataHost();
        mUserDataHost.setUserData(ActorUiTabController.class, mActorUiTabController);

        mTabSupplier = ObservableSuppliers.createNullable();
        mProfileSupplier = ObservableSuppliers.createMonotonic();

        mCoordinator =
                new ActorControlCoordinator(
                        mActivity,
                        mPlayPauseListener,
                        mCloseListener,
                        mTabSupplier,
                        mTabBottomSheetManager,
                        mProfileSupplier);

        mModel = mCoordinator.getModelForTesting();
        mMediator = mCoordinator.getMediatorForTesting();

        ShadowLooper.idleMainLooper();
        reset(mTabBottomSheetManager, mActorUiTabController, mPlayPauseListener, mCloseListener);
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

    @Test
    public void testInitialization() {
        assertNotNull(mModel);
        assertEquals(mPlayPauseListener, mModel.get(ActorControlProperties.ON_PLAY_PAUSE_CLICKED));
        assertEquals(mCloseListener, mModel.get(ActorControlProperties.ON_CLOSE_CLICKED));
    }

    @Test
    public void testPlayPauseClickTriggered() {
        when(mTabBottomSheetManager.isSheetInitialized()).thenReturn(true);
        mCoordinator.attachPeekView();
        ActorControlView view = (ActorControlView) mCoordinator.getPeekViewForTesting();
        ChromeImageButton playPauseButton = view.findViewById(R.id.actor_control_status_button);

        playPauseButton.performClick();
        verify(mPlayPauseListener).onClick(playPauseButton);
    }

    @Test
    public void testCloseClickTriggered() {
        when(mTabBottomSheetManager.isSheetInitialized()).thenReturn(true);
        mCoordinator.attachPeekView();
        ActorControlView view = (ActorControlView) mCoordinator.getPeekViewForTesting();
        ChromeImageButton closeButton = view.findViewById(R.id.actor_control_close_button);

        closeButton.performClick();
        verify(mCloseListener).onClick(closeButton);
    }

    @Test
    public void testAttachPeekView() {
        when(mTabBottomSheetManager.isSheetInitialized()).thenReturn(true);
        mCoordinator.attachPeekView();
        verify(mTabBottomSheetManager).attachPeekView(any());
    }

    @Test
    public void testAttachPeekView_sheetNotInitialized() {
        when(mTabBottomSheetManager.isSheetInitialized()).thenReturn(false);
        mCoordinator.attachPeekView();
        verify(mTabBottomSheetManager, never()).attachPeekView(any());
    }

    @Test
    public void testTabObserver_removeObserver() {
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        mTabSupplier.set(mTab);
        mTabSupplier.set(null);
        verify(mActorUiTabController).removeObserver(mActorObserverCaptor.capture());
    }

    @Test
    public void testTabObserver_nonNullTab() {
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mTabBottomSheetManager.isSheetInitialized()).thenReturn(true);
        when(mActorUiTabController.getUiTabState()).thenReturn(createUiTabState(true));
        mTabSupplier.set(mTab);
        verify(mActorUiTabController).addObserver(mActorObserverCaptor.capture());
        verify(mTabBottomSheetManager).attachPeekView(any());
    }

    @Test
    public void testTabObserver_nullTab() {
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mTabBottomSheetManager.isSheetInitialized()).thenReturn(true);
        when(mTabBottomSheetManager.hidePeekView()).thenReturn(true);
        mTabSupplier.set(mTab);
        ShadowLooper.idleMainLooper();
        reset(mTabBottomSheetManager, mActorUiTabController);
        when(mTabBottomSheetManager.isSheetInitialized()).thenReturn(true);

        mTabSupplier.set(null);

        verify(mActorUiTabController).removeObserver(mCoordinator);
        verify(mActorUiTabController, never()).addObserver(any());
    }

    @Test
    public void testOnUiTabStateChanged_sheetNotInitialized() {
        when(mTabBottomSheetManager.isSheetInitialized()).thenReturn(false);
        mCoordinator.onUiTabStateChanged(createUiTabState(true));
        verify(mTabBottomSheetManager, never()).showPeekView();
        verify(mTabBottomSheetManager, never()).hidePeekView();
    }

    @Test
    public void testOnUiTabStateChanged_actorOverlayActive() {
        when(mTabBottomSheetManager.isSheetInitialized()).thenReturn(true);
        mCoordinator.onUiTabStateChanged(createUiTabState(true));
        verify(mTabBottomSheetManager).attachPeekView(any());
        verify(mTabBottomSheetManager).showPeekView();
    }

    @Test
    public void testOnUiTabStateChanged_actorOverlayInactive() {
        when(mTabBottomSheetManager.isSheetInitialized()).thenReturn(true);
        mCoordinator.onUiTabStateChanged(createUiTabState(false));
        verify(mTabBottomSheetManager).hidePeekView();
    }

    @Test
    public void testSetContent_ActingState() {
        mMediator.setContent(TASK_TITLE, PeekViewUiState.ACTING);

        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(PeekViewUiState.ACTING, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testSetContent_PausedState() {
        mMediator.setContent(TASK_TITLE, PeekViewUiState.PAUSED);

        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(PeekViewUiState.PAUSED, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testSetContent_WaitingState() {
        mMediator.setContent(TASK_TITLE, PeekViewUiState.WAITING);

        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(
                PeekViewUiState.WAITING, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testSetContent_DefaultState() {
        mMediator.setContent(TASK_TITLE, PeekViewUiState.DEFAULT);

        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(
                PeekViewUiState.DEFAULT, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testOnProfileAdded_validProfile_withTask() {
        expectValidProfile();
        when(mActorTask.getState()).thenReturn(ActorTaskState.ACTING);
        expectValidActorTask();
        when(mTabBottomSheetManager.isSheetInitialized()).thenReturn(true);
        mCoordinator.attachPeekView();

        mProfileSupplier.set(mProfile);

        verify(mActorKeyedService).addObserver(mCoordinator);
        assertEquals(TASK_TITLE, mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(PeekViewUiState.ACTING, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testOnProfileAdded_validProfile_noTask() {
        expectValidProfile();
        when(mActorKeyedService.getCurrentActiveTask()).thenReturn(null);
        when(mTabBottomSheetManager.isSheetInitialized()).thenReturn(true);
        mCoordinator.attachPeekView();

        mProfileSupplier.set(mProfile);

        verify(mActorKeyedService).addObserver(mCoordinator);
        assertEquals("", mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(
                PeekViewUiState.DEFAULT, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testOnProfileAdded_nonValidProfile() {
        when(mProfile.isNativeInitialized()).thenReturn(false);
        when(mTabBottomSheetManager.isSheetInitialized()).thenReturn(true);
        mCoordinator.attachPeekView();
        mProfileSupplier.set(mProfile);

        verify(mActorKeyedService, never()).addObserver(any());
        assertEquals("", mModel.get(ActorControlProperties.TASK_TITLE));
    }

    private void setUpForOnTaskStateChanged() {
        expectValidProfile();
        expectValidActorTask();
        when(mTabBottomSheetManager.isSheetInitialized()).thenReturn(true);
        mCoordinator.attachPeekView();
        mProfileSupplier.set(mProfile);
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
        assertEquals("", mModel.get(ActorControlProperties.TASK_TITLE));
        assertEquals(
                PeekViewUiState.DEFAULT, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }

    @Test
    public void testOnTaskStateChanged_taskIdMismatch() {
        setUpForOnTaskStateChanged();
        mCoordinator.onTaskStateChanged(TASK_ID, ActorTaskState.ACTING);
        mCoordinator.onTaskStateChanged(TASK_ID + 1, ActorTaskState.PAUSED_BY_USER);
        assertEquals(PeekViewUiState.ACTING, mModel.get(ActorControlProperties.PEEK_VIEW_UI_STATE));
    }
}
