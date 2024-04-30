// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Color;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for the {@link TabViewManager} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {TabViewManagerUnitTest.TabBrowserControlsOffsetHelperShadow.class})
public class TabViewManagerUnitTest {
    /** A shadow implementation of {@link TabBrowserControlsOffsetHelper}. */
    @Implements(TabBrowserControlsOffsetHelper.class)
    public static class TabBrowserControlsOffsetHelperShadow {
        @Implementation
        public static TabBrowserControlsOffsetHelper get(Tab tab) {
            TabBrowserControlsOffsetHelper mockTabBrowserControlsOffsetHelper =
                    mock(TabBrowserControlsOffsetHelper.class);
            when(mockTabBrowserControlsOffsetHelper.contentOffset()).thenReturn(0);
            when(mockTabBrowserControlsOffsetHelper.bottomControlsOffset()).thenReturn(0);
            return mockTabBrowserControlsOffsetHelper;
        }
    }

    @Mock private TabImpl mTab;
    @Mock private TabViewProvider mTabViewProvider0;
    @Mock private TabViewProvider mTabViewProvider1;
    @Mock private TabViewProvider mTabViewProvider2;
    @Mock private View mTabView0;
    private final int mViewBackground0 = Color.WHITE;
    @Mock private View mTabView1;
    private final int mViewBackground1 = Color.BLACK;
    @Mock private View mTabView2;
    private final int mViewBackground2 = Color.RED;

    private TabViewManagerImpl mTabViewManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mTabViewManager = new TabViewManagerImpl(mTab);
        when(mTabViewProvider0.getTabViewProviderType())
                .thenReturn(TabViewManagerImpl.PRIORITIZED_TAB_VIEW_PROVIDER_TYPES[0]);
        when(mTabViewProvider1.getTabViewProviderType())
                .thenReturn(TabViewManagerImpl.PRIORITIZED_TAB_VIEW_PROVIDER_TYPES[1]);
        when(mTabViewProvider2.getTabViewProviderType())
                .thenReturn(TabViewManagerImpl.PRIORITIZED_TAB_VIEW_PROVIDER_TYPES[2]);
        when(mTabViewProvider0.getView()).thenReturn(mTabView0);
        when(mTabViewProvider0.getBackgroundColor(any())).thenReturn(mViewBackground0);
        when(mTabViewProvider1.getView()).thenReturn(mTabView1);
        when(mTabViewProvider1.getBackgroundColor(any())).thenReturn(mViewBackground1);
        when(mTabViewProvider2.getView()).thenReturn(mTabView2);
        when(mTabViewProvider2.getBackgroundColor(any())).thenReturn(mViewBackground2);
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
        verify(mTab).setCustomView(mTabView1, mViewBackground1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider1, 1);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider1, 0);

        mTabViewManager.addTabViewProvider(mTabViewProvider2);
        Assert.assertTrue(
                "TabViewProvider with the highest priority should be shown",
                mTabViewManager.isShowing(mTabViewProvider1));
        verify(mTab).setCustomView(mTabView1, mViewBackground1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider1, 1);
        verifyTabViewProviderOnShownCalled(mTabViewProvider2, 0);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider1, 0);
        verifyTabViewProviderOnHiddenCalled(mTabViewProvider2, 0);

        mTabViewManager.addTabViewProvider(mTabViewProvider0);
        Assert.assertTrue(
                "TabViewProvider with the highest priority should be shown",
                mTabViewManager.isShowing(mTabViewProvider0));
        verify(mTab).setCustomView(mTabView0, mViewBackground0);
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
        verify(mTab).setCustomView(mTabView1, mViewBackground1);
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
        verify(mTab).setCustomView(mTabView1, mViewBackground1);
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
