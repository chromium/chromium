// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View.OnClickListener;
import android.view.ViewStub;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

/** Tests for {@link ActorOverlayCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ActorOverlayCoordinatorTest {
    @Mock private ViewStub mViewStub;
    @Mock private ActorOverlayView mView;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    @Mock private Tab mTab;
    @Mock private SnackbarManager mSnackbarManager;

    private TabObscuringHandler mTabObscuringHandler;
    private ActorOverlayCoordinator mCoordinator;
    private SettableNullableObservableSupplier<Tab> mCurrentTabSupplier;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).get();
        Mockito.when(mView.getContext()).thenReturn(activity);
        Mockito.when(mViewStub.inflate()).thenReturn(mView);

        mTabObscuringHandler = new TabObscuringHandler();

        mCurrentTabSupplier = ObservableSuppliers.createNullable();
        Mockito.when(mTabModelSelector.getCurrentTabSupplier()).thenReturn(mCurrentTabSupplier);

        mCoordinator =
                new ActorOverlayCoordinator(
                        mViewStub,
                        mTabModelSelector,
                        mBrowserControlsVisibilityManager,
                        mTabObscuringHandler,
                        mSnackbarManager);
    }

    @Test
    public void testConstruction() {
        Assert.assertNotNull(mCoordinator.getMediator());
        Assert.assertEquals(mView, mCoordinator.getView());
        verify(mViewStub).inflate();
        verify(mTabModelSelector).addObserver(any(TabModelSelectorObserver.class));
        verify(mBrowserControlsVisibilityManager).addObserver(any());
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
        Mockito.clearInvocations(mSnackbarManager);
        when(mSnackbarManager.isShowing()).thenReturn(true);
        clickListener.onClick(mView);
        verify(mSnackbarManager, Mockito.never()).showSnackbar(any());
    }

    @Test
    public void testVisibility() {
        Mockito.clearInvocations(mView);

        ActorOverlayMediator mediator = mCoordinator.getMediator();
        mediator.setOverlayVisible(true);
        // CAN_SHOW is true by default from Coordinator init, so VISIBLE=true makes view visible.
        verify(mView).setVisible(true);

        mediator.setOverlayVisible(false);
        verify(mView).setVisible(false);
    }

    @Test
    public void testHideOnTabHidden() {
        ArgumentCaptor<TabModelSelectorObserver> observerCaptor =
                ArgumentCaptor.forClass(TabModelSelectorObserver.class);
        verify(mTabModelSelector).addObserver(observerCaptor.capture());

        Mockito.clearInvocations(mView);

        ActorOverlayMediator mediator = mCoordinator.getMediator();
        mediator.setOverlayVisible(true);
        verify(mView).setVisible(true);

        observerCaptor.getValue().onTabHidden(mTab);
        verify(mView).setVisible(false);
    }

    @Test
    public void testUpdateCanShowOverlayOnTabShown() {
        Mockito.clearInvocations(mView);

        ActorOverlayMediator mediator = mCoordinator.getMediator();
        mediator.setOverlayVisible(true);
        verify(mView).setVisible(true);

        // Simulate a new tab showing. This should trigger updateCanShowOverlay, which currently
        // sets CAN_SHOW to false for native pages.
        Mockito.when(mTab.isNativePage()).thenReturn(true);
        mCurrentTabSupplier.set(mTab);

        verify(mView).setVisible(false);
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
        verify(mTabModelSelector).removeObserver(any(TabModelSelectorObserver.class));
        verify(mBrowserControlsVisibilityManager).removeObserver(any());
        Assert.assertFalse(mCurrentTabSupplier.hasObservers());
    }
}
