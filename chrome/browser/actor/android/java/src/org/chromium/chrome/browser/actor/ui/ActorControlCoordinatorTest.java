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
import android.widget.TextView;

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
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.UiTabState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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

    @Captor private ArgumentCaptor<ActorUiTabController.Observer> mActorObserverCaptor;

    @Mock private ActorUiTabController mActorUiTabController;
    @Mock private TabBottomSheetManager mTabBottomSheetManager;
    @Mock private View.OnClickListener mPlayPauseListener;
    @Mock private View.OnClickListener mCloseListener;
    @Mock private Tab mTab;

    private Activity mActivity;
    private ActorControlCoordinator mCoordinator;
    private PropertyModel mModel;
    private ActorControlMediator mMediator;
    private SettableNullableObservableSupplier<Tab> mTabSupplier;
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

    // Helper method to reapply default stubs after resets
    private void reapplyDefaultStubs() {
        when(mTabBottomSheetManager.isSheetInitialized()).thenReturn(true);
        when(mTabBottomSheetManager.hidePeekView()).thenReturn(true);
        when(mTabBottomSheetManager.showPeekView()).thenReturn(true);
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
    }

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mActorObserverCaptor = ArgumentCaptor.forClass(ActorUiTabController.Observer.class);
        mUserDataHost = new UserDataHost();
        mUserDataHost.setUserData(ActorUiTabController.class, mActorUiTabController);

        mTabSupplier = ObservableSuppliers.createNullable();
        mCoordinator =
                new ActorControlCoordinator(
                        mActivity,
                        mPlayPauseListener,
                        mCloseListener,
                        mTabSupplier,
                        mTabBottomSheetManager);

        mModel = mCoordinator.getModelForTesting();
        mMediator = mCoordinator.getMediatorForTesting();

        ShadowLooper.idleMainLooper();
        reset(mTabBottomSheetManager, mActorUiTabController, mPlayPauseListener, mCloseListener);
        reapplyDefaultStubs();
    }

    @Test
    public void testInitialization() {
        assertNotNull(mModel);
        assertEquals(mPlayPauseListener, mModel.get(ActorControlProperties.ON_PLAY_PAUSE_CLICKED));
        assertEquals(mCloseListener, mModel.get(ActorControlProperties.ON_CLOSE_CLICKED));
    }

    @Test
    public void testStatusIconUpdates_PausedToPlaying() {
        mCoordinator.attachPeekView();
        ActorControlView view = (ActorControlView) mCoordinator.getPeekViewForTesting();
        ChromeImageButton statusButton = view.findViewById(R.id.actor_control_status_button);

        // Set to Paused (should show Play Arrow)
        mMediator.updateStatusIcon(true);

        assertEquals(
                R.drawable.ic_play_arrow_white_24dp,
                mModel.get(ActorControlProperties.STATUS_ICON_RESOURCE));

        assertEquals(
                R.drawable.ic_play_arrow_white_24dp,
                Shadows.shadowOf(statusButton.getDrawable()).getCreatedFromResId());

        // Set to Playing (should show Pause icon)
        mMediator.updateStatusIcon(false);

        assertEquals(
                R.drawable.ic_pause_white_24dp,
                mModel.get(ActorControlProperties.STATUS_ICON_RESOURCE));

        assertEquals(
                R.drawable.ic_pause_white_24dp,
                Shadows.shadowOf(statusButton.getDrawable()).getCreatedFromResId());
    }

    @Test
    public void testContentUpdates() {
        String testTitle = "Task in Progress";
        String testDesc = "Step 4 of 10";

        mMediator.setContent(testTitle, testDesc);

        mCoordinator.attachPeekView();
        ActorControlView view = (ActorControlView) mCoordinator.getPeekViewForTesting();
        TextView titleView = view.findViewById(R.id.actor_control_title);
        TextView descView = view.findViewById(R.id.actor_control_description);

        assertEquals(testTitle, titleView.getText().toString());
        assertEquals(testDesc, descView.getText().toString());
    }

    @Test
    public void testPlayPauseClickTriggered() {
        mCoordinator.attachPeekView();
        ActorControlView view = (ActorControlView) mCoordinator.getPeekViewForTesting();
        ChromeImageButton playPauseButton = view.findViewById(R.id.actor_control_status_button);

        playPauseButton.performClick();
        verify(mPlayPauseListener).onClick(playPauseButton);
    }

    @Test
    public void testCloseClickTriggered() {
        mCoordinator.attachPeekView();
        ActorControlView view = (ActorControlView) mCoordinator.getPeekViewForTesting();
        ChromeImageButton closeButton = view.findViewById(R.id.actor_control_close_button);

        closeButton.performClick();
        verify(mCloseListener).onClick(closeButton);
    }

    @Test
    public void testAttachPeekView() {
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
        mTabSupplier.set(mTab);
        mTabSupplier.set(null);
        verify(mActorUiTabController).removeObserver(mActorObserverCaptor.capture());
    }

    @Test
    public void testTabObserver_nonNullTab() {
        when(mActorUiTabController.getUiTabState()).thenReturn(createUiTabState(true));
        mTabSupplier.set(mTab);
        verify(mActorUiTabController).addObserver(mActorObserverCaptor.capture());
        verify(mTabBottomSheetManager).attachPeekView(any());
    }

    @Test
    public void testTabObserver_nullTab() {
        mTabSupplier.set(mTab);
        ShadowLooper.idleMainLooper();
        reset(mTabBottomSheetManager, mActorUiTabController);
        reapplyDefaultStubs();
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
}
