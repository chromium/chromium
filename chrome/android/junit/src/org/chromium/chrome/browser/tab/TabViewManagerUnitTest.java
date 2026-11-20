// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Color;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.ContextUtils;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProviderSupplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for the {@link TabViewManager} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabViewManagerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabImpl mTab;
    @Mock private TabBrowserControlsOffsetHelper mTabBrowserControlsOffsetHelper;
    @Mock private TabViewProvider mTabViewProvider0;
    @Mock private TabViewProvider mTabViewProvider1;
    @Mock private TabViewProvider mTabViewProvider2;
    @Mock private BrowserControlsStateProvider mBrowserControlsA;
    @Mock private BrowserControlsStateProvider mBrowserControlsB;
    @Mock private TabDelegateFactory mTabDelegateFactory;
    @Mock private TabWebContentsDelegateAndroid mTabWebContentsDelegate;
    @Mock private Profile mProfile;

    private final View mTabView0 = new View(ContextUtils.getApplicationContext());
    private final View mTabView1 = new View(ContextUtils.getApplicationContext());
    private final View mTabView2 = new View(ContextUtils.getApplicationContext());

    private static final int VIEW_BACKGROUND0 = Color.WHITE;
    private static final int VIEW_BACKGROUND1 = Color.BLACK;
    private static final int VIEW_BACKGROUND2 = Color.RED;

    private TabViewManagerImpl mTabViewManager;

    // Held here because the mocked windows only keep weak references to it.
    private Activity mActivity;

    // Held here because UnownedUserDataHost keeps only weak references to its data.
    private MonotonicObservableSupplier<BrowserControlsStateProvider> mBrowserControlsSupplierA;
    private MonotonicObservableSupplier<BrowserControlsStateProvider> mBrowserControlsSupplierB;
    private WindowAndroid mWindowB;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        TabBrowserControlsOffsetHelper.setInstanceForTesting(mTabBrowserControlsOffsetHelper);
        when(mTabBrowserControlsOffsetHelper.contentOffset()).thenReturn(0);
        when(mTabBrowserControlsOffsetHelper.bottomControlsOffset()).thenReturn(0);

        mTabViewManager = new TabViewManagerImpl(mTab);
        when(mTabViewProvider0.getTabViewProviderType())
                .thenReturn(TabViewManagerImpl.PRIORITIZED_TAB_VIEW_PROVIDER_TYPES[0]);
        when(mTabViewProvider1.getTabViewProviderType())
                .thenReturn(TabViewManagerImpl.PRIORITIZED_TAB_VIEW_PROVIDER_TYPES[1]);
        when(mTabViewProvider2.getTabViewProviderType())
                .thenReturn(TabViewManagerImpl.PRIORITIZED_TAB_VIEW_PROVIDER_TYPES[2]);
        when(mTabViewProvider0.getView()).thenReturn(mTabView0);
        when(mTabViewProvider0.getBackgroundColor(any())).thenReturn(VIEW_BACKGROUND0);
        when(mTabViewProvider1.getView()).thenReturn(mTabView1);
        when(mTabViewProvider1.getBackgroundColor(any())).thenReturn(VIEW_BACKGROUND1);
        when(mTabViewProvider2.getView()).thenReturn(mTabView2);
        when(mTabViewProvider2.getBackgroundColor(any())).thenReturn(VIEW_BACKGROUND2);
    }

    /**
     * Verifies that the {@link TabViewProvider} with the highest priority is always showing after
     * each call to {@link TabViewManager#addTabViewProvider}.
     */
    @Test
    public void testAddTabViewProvider() {
        mTabViewManager.addTabViewProvider(mTabViewProvider1);
        Assert.assertTrue(
                "TabViewProvider with the highest priority should be shown",
                mTabViewManager.isShowing(mTabViewProvider1));
        verify(mTab).setCustomView(mTabView1, VIEW_BACKGROUND1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider1, 1);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider1, 0);

        mTabViewManager.addTabViewProvider(mTabViewProvider2);
        Assert.assertTrue(
                "TabViewProvider with the highest priority should be shown",
                mTabViewManager.isShowing(mTabViewProvider1));
        verify(mTab).setCustomView(mTabView1, VIEW_BACKGROUND1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider1, 1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider2, 0);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider1, 0);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider2, 0);

        mTabViewManager.addTabViewProvider(mTabViewProvider0);
        Assert.assertTrue(
                "TabViewProvider with the highest priority should be shown",
                mTabViewManager.isShowing(mTabViewProvider0));
        verify(mTab).setCustomView(mTabView0, VIEW_BACKGROUND0);
        verifyTabViewProviderOnShownCalled(mTabViewProvider0, 1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider1, 1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider2, 0);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider0, 0);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider1, 1);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider2, 0);
    }

    /**
     * Verifies that the {@link TabViewProvider} with the highest priority is always
     * showing after each call to {@link TabViewManager#removeTabViewProvider}.
     */
    @Test
    public void testRemoveTabViewProvider() {
        mTabViewManager.addTabViewProvider(mTabViewProvider0);
        mTabViewManager.addTabViewProvider(mTabViewProvider2);
        mTabViewManager.addTabViewProvider(mTabViewProvider1);
        Assert.assertTrue(
                "TabViewProvider with the highest priority should be shown",
                mTabViewManager.isShowing(mTabViewProvider0));

        mTabViewManager.removeTabViewProvider(mTabViewProvider0);
        Assert.assertTrue(
                "TabViewProvider with the highest priority should be shown",
                mTabViewManager.isShowing(mTabViewProvider1));
        verify(mTab).setCustomView(mTabView1, VIEW_BACKGROUND1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider0, 1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider1, 1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider2, 0);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider0, 1);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider1, 0);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider2, 0);

        mTabViewManager.removeTabViewProvider(mTabViewProvider2);
        Assert.assertTrue(
                "TabViewProvider with the highest priority should be shown",
                mTabViewManager.isShowing(mTabViewProvider1));
        verify(mTab).setCustomView(mTabView1, VIEW_BACKGROUND1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider1, 1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider2, 0);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider1, 0);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider2, 0);

        mTabViewManager.removeTabViewProvider(mTabViewProvider1);
        Assert.assertFalse(
                "No TabViewProvider should be shown", mTabViewManager.isShowing(mTabViewProvider0));
        Assert.assertFalse(
                "No TabViewProvider should be shown", mTabViewManager.isShowing(mTabViewProvider1));
        Assert.assertFalse(
                "No TabViewProvider should be shown", mTabViewManager.isShowing(mTabViewProvider2));
        verify(mTab).setCustomView(null, null);
        verifyTabViewProviderOnShownCalled(mTabViewProvider1, 1);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider1, 1);
    }

    @Test
    public void testMarginsFollowBrowserControlsOfNewWindowAfterReparenting() {
        TabImpl tab = createTabShowingViewInWindowA();

        tab.updateAttachment(/* window= */ null, /* tabDelegateFactory= */ null);
        verify(mBrowserControlsA).removeObserver(any());

        // Adding a higher-priority provider while detached must not rebind to the old window.
        tab.getTabViewManager().addTabViewProvider(mTabViewProvider0);
        verify(mBrowserControlsA, times(1)).addObserver(any());

        tab.updateAttachment(mWindowB, mTabDelegateFactory);

        Assert.assertEquals(
                200, ((FrameLayout.LayoutParams) mTabView0.getLayoutParams()).topMargin);
        verify(mBrowserControlsB).addObserver(any());
    }

    @Test
    public void testMarginsFollowBrowserControlsOfNewWindowAfterWindowOnlySwap() {
        TabImpl tab = createTabShowingViewInWindowA();

        // The tab is not detached first, so the adapter is still bound to window A.
        tab.updateAttachment(mWindowB, /* tabDelegateFactory= */ null);

        verify(mBrowserControlsA).removeObserver(any());
        Assert.assertEquals(
                200, ((FrameLayout.LayoutParams) mTabView1.getLayoutParams()).topMargin);
        verify(mBrowserControlsB).addObserver(any());
    }

    /**
     * Returns a tab attached to a window whose top controls are 100px tall, showing {@link
     * #mTabView1}. {@link #mWindowB} has 200px tall top controls.
     */
    private TabImpl createTabShowingViewInWindowA() {
        when(mBrowserControlsA.getTopControlsHeight()).thenReturn(100);
        when(mBrowserControlsB.getTopControlsHeight()).thenReturn(200);
        mBrowserControlsSupplierA = ObservableSuppliers.createMonotonic(mBrowserControlsA);
        mBrowserControlsSupplierB = ObservableSuppliers.createMonotonic(mBrowserControlsB);
        WindowAndroid windowA = mockWindow(mBrowserControlsSupplierA);
        mWindowB = mockWindow(mBrowserControlsSupplierB);

        when(mTabDelegateFactory.createWebContentsDelegate(any()))
                .thenReturn(mTabWebContentsDelegate);
        TabImpl tab = new TabImpl(1, mProfile, TabLaunchType.FROM_CHROME_UI);
        tab.updateAttachment(windowA, mTabDelegateFactory);
        verify(mBrowserControlsA, never()).addObserver(any());

        tab.getTabViewManager().addTabViewProvider(mTabViewProvider1);
        Assert.assertEquals(
                100, ((FrameLayout.LayoutParams) mTabView1.getLayoutParams()).topMargin);
        verify(mBrowserControlsA, times(1)).addObserver(any());
        return tab;
    }

    private WindowAndroid mockWindow(
            MonotonicObservableSupplier<BrowserControlsStateProvider> browserControlsSupplier) {
        UnownedUserDataHost host = new UnownedUserDataHost();
        BrowserControlsStateProviderSupplier.attach(host, browserControlsSupplier);
        WindowAndroid window = mock(WindowAndroid.class);
        when(window.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(window.getContext()).thenReturn(new WeakReference<>(mActivity));
        when(window.getOcclusionSupplier()).thenReturn(ObservableSuppliers.alwaysFalse());
        when(window.getUnownedUserDataHost()).thenReturn(host);
        return window;
    }

    private void verifyTabViewProviderOnShownCalled(
            TabViewProvider mockTabViewProvider, int numberOfCalls) {
        String description =
                "onShown() should have been called "
                        + numberOfCalls
                        + " times on TabViewProvider type "
                        + mockTabViewProvider.getTabViewProviderType();
        verify(mockTabViewProvider, times(numberOfCalls).description(description)).onShown();
    }

    private void verifyTabViewProviderOnHiddenCalled(
            TabViewProvider mockTabViewProvider, int numberOfCalls) {
        String description =
                "onHidden() should have been called "
                        + numberOfCalls
                        + " times on TabViewProvider type "
                        + mockTabViewProvider.getTabViewProviderType();
        verify(mockTabViewProvider, times(numberOfCalls).description(description)).onHidden();
    }
}
