// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.FrameLayout;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.ActorOverlayState;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.HandoffButtonState;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.UiTabState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandlerRegistry;

/** Tests for {@link ActorOverlayCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.GLIC)
public class ActorOverlayCoordinatorTest {
    @Mock private ViewStub mViewStub;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    @Mock private Tab mTab;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private BackPressHandlerRegistry mBackPressHandlerRegistry;
    @Mock private ActorUiTabController.Natives mTabControllerNatives;
    @Mock private LayoutManager mLayoutManager;
    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private ActorOverlayView mView;
    private static final int TAB_ID = 123;

    private ActorUiTabController mTabController;
    private TabObscuringHandler mTabObscuringHandler;
    private ActorOverlayCoordinator mCoordinator;
    private SettableNullableObservableSupplier<Tab> mCurrentTabSupplier;
    private UserDataHost mUserDataHost;
    private SettableMonotonicObservableSupplier<LayoutManager> mLayoutManagerSupplier;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).get();
        mView = Mockito.spy(new ActorOverlayView(activity, null));
        mView.setLayoutParams(
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        Mockito.when(mViewStub.inflate()).thenReturn(mView);

        mTabObscuringHandler = new TabObscuringHandler();
        mUserDataHost = new UserDataHost();
        Mockito.when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        Mockito.when(mTab.getId()).thenReturn(TAB_ID);

        // Make ActorUiTabController.from() return a real instance.
        mTabController = ActorUiTabController.from(mTab);
        ActorUiTabControllerJni.setInstanceForTesting(mTabControllerNatives);

        mCurrentTabSupplier = ObservableSuppliers.createNullable();
        Mockito.when(mTabModelSelector.getCurrentTabSupplier()).thenReturn(mCurrentTabSupplier);
        mCurrentTabSupplier.set(mTab);

        mLayoutManagerSupplier = ObservableSuppliers.createMonotonic();
        Mockito.when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);

        mCoordinator =
                new ActorOverlayCoordinator(
                        mViewStub,
                        mTabModelSelector,
                        mBrowserControlsVisibilityManager,
                        mTabObscuringHandler,
                        mSnackbarManager,
                        mBackPressHandlerRegistry,
                        mLayoutManagerSupplier);
        mLayoutManagerSupplier.set(mLayoutManager);
    }

    @Test
    public void testConstruction() {
        Assert.assertNotNull(mCoordinator.getMediator());
        Assert.assertEquals(mView, mCoordinator.getView());
        verify(mViewStub).inflate();
        Assert.assertTrue(mCurrentTabSupplier.hasObservers());
        verify(mBrowserControlsVisibilityManager).addObserver(any());
        verify(mLayoutManager).addObserver(any());
        verify(mBackPressHandlerRegistry).addHandler(any(), eq(BackPressHandler.Type.ACTOR_OVERLAY));
    }

    @Test
    public void testHideOnLayoutTypeChanged() {
        ActorOverlayMediator mediator = mCoordinator.getMediator();
        verify(mLayoutManager).addObserver(mediator);

        Mockito.clearInvocations(mView);

        mediator.setOverlayVisible(true);
        verify(mView).setVisibility(View.VISIBLE);

        // Change layout type to TAB_SWITCHER.
        Mockito.when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.TAB_SWITCHER);
        mediator.onStartedShowing(LayoutType.BROWSING);

        verify(mView).setVisibility(View.GONE);

        // Change layout type back to BROWSING.
        Mockito.clearInvocations(mView);
        Mockito.when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mediator.onStartedShowing(LayoutType.TAB_SWITCHER);

        verify(mView).setVisibility(View.VISIBLE);
    }

    @Test
    public void testSnackbarOnClicked() {
        OnClickListener clickListener =
                mCoordinator.getModelForTesting().get(ActorOverlayProperties.ON_CLICK_LISTENER);
        Assert.assertNotNull(clickListener);

        // Snackbar should be shown if not already showing.
        when(mSnackbarManager.isShowing()).thenReturn(false);
        clickListener.onClick(mView);
        verify(mSnackbarManager).showSnackbar(any());

        // Snackbar should NOT be shown if already showing.
        clearInvocations(mSnackbarManager);
        when(mSnackbarManager.isShowing()).thenReturn(true);
        clickListener.onClick(mView);
        verify(mSnackbarManager, Mockito.never()).showSnackbar(any());
    }

    @Test
    public void testVisibility() {
        clearInvocations(mView);

        ActorOverlayMediator mediator = mCoordinator.getMediator();
        mediator.setOverlayVisible(true);
        // CAN_SHOW is true by default from Coordinator init, so VISIBLE=true makes view visible.
        verify(mView).setVisibility(View.VISIBLE);

        clearInvocations(mView);
        mediator.setOverlayVisible(false);
        verify(mView).setVisibility(View.GONE);
    }

    @Test
    public void testHideOnTabHidden() {
        verify(mTab).addObserver(mTabObserverCaptor.capture());

        clearInvocations(mView);

        ActorOverlayMediator mediator = mCoordinator.getMediator();
        mediator.setOverlayVisible(true);
        verify(mView).setVisibility(View.VISIBLE);

        clearInvocations(mView);
        mTabObserverCaptor.getValue().onHidden(mTab, 0);
        verify(mView).setVisibility(View.GONE);
    }

    @Test
    public void testShowOnTabShown() {
        verify(mTab).addObserver(mTabObserverCaptor.capture());

        ActorOverlayMediator mediator = mCoordinator.getMediator();
        mediator.setOverlayVisible(true);

        // Hide it first
        mTabObserverCaptor.getValue().onHidden(mTab, 0);

        clearInvocations(mView);
        mTabObserverCaptor.getValue().onShown(mTab, 0);
        verify(mView).setVisibility(View.VISIBLE);
    }

    @Test
    public void testNoShowOnTabShownIfStateDeactivatedWhileHidden() {
        verify(mTab).addObserver(mTabObserverCaptor.capture());

        ActorOverlayMediator mediator = mCoordinator.getMediator();
        mediator.setOverlayVisible(true);

        // Hide it first.
        clearInvocations(mView);
        mTabObserverCaptor.getValue().onHidden(mTab, 0);
        verify(mView).setVisibility(View.GONE);

        // While hidden, task state changes to inactive!
        ActorUiTabController tabController = ActorUiTabController.from(mTab);
        UiTabState state =
                new UiTabState(
                        /* tabId= */ TAB_ID,
                        /* actorOverlay= */ new ActorOverlayState(
                                /* isActive= */ false,
                                /* borderGlowVisible= */ false,
                                /* mouseDown= */ false),
                        /* handoffButton= */ new HandoffButtonState(
                                /* isActive= */ false, /* controller= */ 0),
                        /* tabIndicator= */ 0,
                        /* borderGlowVisible= */ false);

        clearInvocations(mView);
        tabController.onUiTabStateChange(state);

        // Now bring back the tab to SHOWN!
        clearInvocations(mView);
        mTabObserverCaptor.getValue().onShown(mTab, 0);

        // It should NOT become visible because trigger state turned false.
        verify(mView, Mockito.never()).setVisibility(View.VISIBLE);
    }

    @Test
    public void testUpdateCanShowOverlayOnTabShown() {
        clearInvocations(mView);

        ActorOverlayMediator mediator = mCoordinator.getMediator();
        mediator.setOverlayVisible(true);
        verify(mView).setVisibility(View.VISIBLE);

        // Simulate a new tab showing. This should trigger updateCanShowOverlay, which currently
        // sets CAN_SHOW to false for native pages.
        Mockito.when(mTab.isNativePage()).thenReturn(true);
        clearInvocations(mView);

        // Trigger LayoutType change before setting tab to null
        Mockito.when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.SIMPLE_ANIMATION);
        mediator.onStartedShowing(LayoutType.SIMPLE_ANIMATION);

        mCurrentTabSupplier.set(null);

        // Switch back to BROWSING and set the tab
        Mockito.when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mediator.onStartedShowing(LayoutType.BROWSING);

        mCurrentTabSupplier.set(mTab);

        verify(mView).setVisibility(View.GONE);
    }

    @Test
    public void testTabSwitchToNullHidesOverlay() {
        mCurrentTabSupplier.set(mTab);

        ActorOverlayMediator mediator = mCoordinator.getMediator();
        mediator.setOverlayVisible(true);
        verify(mView).setVisibility(View.VISIBLE);

        clearInvocations(mView);
        mCurrentTabSupplier.set(null);
        verify(mView).setVisibility(View.GONE);
        Assert.assertFalse(mCoordinator.getModelForTesting().get(ActorOverlayProperties.CAN_SHOW));
    }

    @Test
    public void testTabSwitchToClosingTab() {
        ActorOverlayMediator mediator = mCoordinator.getMediator();
        mediator.setOverlayVisible(true);

        Mockito.when(mTab.isClosing()).thenReturn(true);
        clearInvocations(mView);

        // Trigger LayoutType change before setting tab to null
        Mockito.when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.SIMPLE_ANIMATION);
        mediator.onStartedShowing(LayoutType.SIMPLE_ANIMATION);

        mCurrentTabSupplier.set(null);

        // Switch back to BROWSING and set the tab
        Mockito.when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mediator.onStartedShowing(LayoutType.BROWSING);

        mCurrentTabSupplier.set(mTab);

        verify(mView).setVisibility(View.GONE);
        Assert.assertFalse(mCoordinator.getModelForTesting().get(ActorOverlayProperties.CAN_SHOW));
    }

    @Test
    public void testTabSwitchToDestroyedTab() {
        ActorOverlayMediator mediator = mCoordinator.getMediator();
        mediator.setOverlayVisible(true);

        Mockito.when(mTab.isDestroyed()).thenReturn(true);
        clearInvocations(mView);

        // Trigger LayoutType change before setting tab to null
        Mockito.when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.SIMPLE_ANIMATION);
        mediator.onStartedShowing(LayoutType.SIMPLE_ANIMATION);

        mCurrentTabSupplier.set(null);

        // Switch back to BROWSING and set the tab
        Mockito.when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mediator.onStartedShowing(LayoutType.BROWSING);

        mCurrentTabSupplier.set(mTab);

        verify(mView).setVisibility(View.GONE);
        Assert.assertFalse(mCoordinator.getModelForTesting().get(ActorOverlayProperties.CAN_SHOW));
    }

    @Test
    public void testTabSwitchToHiddenTab() {
        ActorOverlayMediator mediator = mCoordinator.getMediator();
        mediator.setOverlayVisible(true);

        Mockito.when(mTab.isHidden()).thenReturn(true);
        clearInvocations(mView);

        // Trigger LayoutType change before setting tab to null
        Mockito.when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.SIMPLE_ANIMATION);
        mediator.onStartedShowing(LayoutType.SIMPLE_ANIMATION);

        mCurrentTabSupplier.set(null);

        // Switch back to BROWSING and set the tab
        Mockito.when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mediator.onStartedShowing(LayoutType.BROWSING);

        mCurrentTabSupplier.set(mTab);

        verify(mView).setVisibility(View.GONE);
        Assert.assertFalse(mCoordinator.getModelForTesting().get(ActorOverlayProperties.CAN_SHOW));
    }

    @Test
    public void testInitialStateAppliedOnTabSwitch() {
        ActorUiTabController tabController = ActorUiTabController.from(mTab);
        UiTabState state =
                new UiTabState(
                        /* tabId= */ TAB_ID,
                        /* actorOverlay= */ new ActorOverlayState(
                                /* isActive= */ true,
                                /* borderGlowVisible= */ false,
                                /* mouseDown= */ false),
                        /* handoffButton= */ new HandoffButtonState(
                                /* isActive= */ false, /* controller= */ 0),
                        /* tabIndicator= */ 0,
                        /* borderGlowVisible= */ false);

        // Set initial state on the tab controller before it's observed.
        tabController.onUiTabStateChange(state);

        // Switching to the tab should immediately apply its active state.
        mCurrentTabSupplier.set(mTab);
        verify(mView).setVisibility(View.VISIBLE);
    }

    @Test
    public void testResetVisibilityIfNoInitialState() {
        ActorOverlayMediator mediator = mCoordinator.getMediator();
        mediator.setOverlayVisible(true);
        verify(mView).setVisibility(View.VISIBLE);

        // Switch to a new tab that has no state.
        Tab tab2 = Mockito.mock(Tab.class);
        UserDataHost userDataHost2 = new UserDataHost();
        Mockito.when(tab2.getUserDataHost()).thenReturn(userDataHost2);

        clearInvocations(mView);
        mCurrentTabSupplier.set(tab2);
        // It should hide because no state is available for tab2.
        verify(mView).setVisibility(View.GONE);
    }

    @Test
    public void testVisibilityDrivenByTabController() {
        clearInvocations(mView);

        // Initial state: CAN_SHOW is true (default in Mediator for non-native tab),
        // VISIBLE is false (default in Mediator).
        mCurrentTabSupplier.set(mTab);

        // Set state via TabController.
        ActorUiTabController tabController = ActorUiTabController.from(mTab);
        UiTabState state =
                new UiTabState(
                        /* tabId= */ TAB_ID,
                        /* actorOverlay= */ new ActorOverlayState(
                                /* isActive= */ true,
                                /* borderGlowVisible= */ false,
                                /* mouseDown= */ false),
                        /* handoffButton= */ new HandoffButtonState(
                                /* isActive= */ false, /* controller= */ 0),
                        /* tabIndicator= */ 0,
                        /* borderGlowVisible= */ false);

        // Use the package-private testing method.
        tabController.onUiTabStateChange(state);

        verify(mView).setVisibility(View.VISIBLE);

        // Now set isActive to false.
        UiTabState state2 =
                new UiTabState(
                        /* tabId= */ TAB_ID,
                        /* actorOverlay= */ new ActorOverlayState(
                                /* isActive= */ false,
                                /* borderGlowVisible= */ false,
                                /* mouseDown= */ false),
                        /* handoffButton= */ new HandoffButtonState(
                                /* isActive= */ false, /* controller= */ 0),
                        /* tabIndicator= */ 0,
                        /* borderGlowVisible= */ false);

        clearInvocations(mView);
        tabController.onUiTabStateChange(state2);

        verify(mView).setVisibility(View.GONE);
    }

    @Test
    public void testTabSwitchUnregistersObserver() {
        mCurrentTabSupplier.set(mTab);
        ActorUiTabController tabController1 = ActorUiTabController.from(mTab);

        Tab tab2 = Mockito.mock(Tab.class);
        UserDataHost userDataHost2 = new UserDataHost();
        Mockito.when(tab2.getUserDataHost()).thenReturn(userDataHost2);
        mCurrentTabSupplier.set(tab2);
        ActorUiTabController tabController2 = ActorUiTabController.from(tab2);

        // tabController1 should no longer have the mediator as observer.
        // We can check this by triggering an update on tabController1 and seeing if it affects
        // mView.
        clearInvocations(mView);
        UiTabState state =
                new UiTabState(
                        /* tabId= */ TAB_ID,
                        /* actorOverlay= */ new ActorOverlayState(
                                /* isActive= */ true,
                                /* borderGlowVisible= */ false,
                                /* mouseDown= */ false),
                        /* handoffButton= */ new HandoffButtonState(
                                /* isActive= */ false, /* controller= */ 0),
                        /* tabIndicator= */ 0,
                        /* borderGlowVisible= */ false);

        tabController1.onUiTabStateChange(state);

        verify(mView, Mockito.never()).setVisibility(any(Integer.class));

        // Trigger on tabController2 should work.
        tabController2.onUiTabStateChange(state);
        verify(mView).setVisibility(View.VISIBLE);
    }

    @Test
    public void testMargins() {
        ArgumentCaptor<BrowserControlsStateProvider.Observer> observerCaptor =
                ArgumentCaptor.forClass(BrowserControlsStateProvider.Observer.class);
        verify(mBrowserControlsVisibilityManager).addObserver(observerCaptor.capture());

        observerCaptor.getValue().onTopControlsHeightChanged(100, 0);
        verify(mView).setMargins(100, 0);

        observerCaptor.getValue().onBottomControlsHeightChanged(50, 0);
        verify(mView).setMargins(100, 50);
    }

    @Test
    public void testObscuringHandler() {
        ActorOverlayMediator mediator = mCoordinator.getMediator();

        Assert.assertFalse(mTabObscuringHandler.isTabContentObscured());

        // CAN_SHOW is true by default. Setting VISIBLE to true makes it visible.
        mediator.setOverlayVisible(true);
        Assert.assertTrue(mTabObscuringHandler.isTabContentObscured());

        // Setting VISIBLE to false hides it.
        mediator.setOverlayVisible(false);
        Assert.assertFalse(mTabObscuringHandler.isTabContentObscured());
    }

    @Test
    public void testDestroy() {
        mCoordinator.destroy();
        verify(mBackPressHandlerRegistry).removeHandler(any());
        verify(mTab).removeObserver(any(TabObserver.class));
        verify(mBrowserControlsVisibilityManager).removeObserver(any());
        Assert.assertFalse(mCurrentTabSupplier.hasObservers());
    }
}
