// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Point;

import org.junit.After;
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

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabContextMenuData;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.url.GURL;

/** Unit tests for {@link AnimationInterruptor}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AnimationInterruptorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private Tab mAnimationTab;
    @Mock private Tab mAnotherTab;
    @Mock private Tab mYetAnotherTab;

    @Captor private ArgumentCaptor<LayoutStateObserver> mLayoutStateObserverCaptor;
    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private final UserDataHost mUserDataHost = new UserDataHost();
    private final ObservableSupplierImpl<Tab> mCurrentTabSupplier = new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mScrimVisibilitySupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Float> mNtpSearchBoxTransitionPercentageSupplier =
            new ObservableSupplierImpl<>();
    private Runnable mInterruptRunnable;
    private AnimationInterruptor mAnimationInterruptor;
    private int mInterruptCount;

    @Before
    public void setUp() {
        when(mAnimationTab.getUserDataHost()).thenReturn(mUserDataHost);
        TabContextMenuData.getOrCreateForTab(mAnimationTab);
        mInterruptRunnable = () -> mInterruptCount++;

        mAnimationInterruptor =
                new AnimationInterruptor(
                        mLayoutStateProvider,
                        mCurrentTabSupplier,
                        mAnimationTab,
                        mScrimVisibilitySupplier,
                        mNtpSearchBoxTransitionPercentageSupplier,
                        /* isRegularNtp= */ false,
                        mInterruptRunnable);

        verify(mLayoutStateProvider).addObserver(mLayoutStateObserverCaptor.capture());
        verify(mAnimationTab).addObserver(mTabObserverCaptor.capture());
        assertTrue(mCurrentTabSupplier.hasObservers());
        assertTrue(mScrimVisibilitySupplier.hasObservers());
        assertFalse(mNtpSearchBoxTransitionPercentageSupplier.hasObservers());
    }

    @After
    public void tearDown() {
        verify(mLayoutStateProvider).removeObserver(any());
        verify(mAnimationTab).removeObserver(any());
        assertFalse(mCurrentTabSupplier.hasObservers());
        assertFalse(mScrimVisibilitySupplier.hasObservers());
        assertFalse(mNtpSearchBoxTransitionPercentageSupplier.hasObservers());
    }

    private void recreateAnimationInterruptor(boolean isRegularNtp) {
        // Reset.
        mAnimationInterruptor.destroy();
        Mockito.clearInvocations(mLayoutStateProvider, mAnimationTab);

        mAnimationInterruptor =
                new AnimationInterruptor(
                        mLayoutStateProvider,
                        mCurrentTabSupplier,
                        mAnimationTab,
                        mScrimVisibilitySupplier,
                        mNtpSearchBoxTransitionPercentageSupplier,
                        isRegularNtp,
                        mInterruptRunnable);
    }

    @Test
    public void testNoInterrupt() {
        mAnimationInterruptor.destroy();
        assertEquals(0, mInterruptCount);
    }

    @Test
    public void testTabChanged() {
        mCurrentTabSupplier.set(mAnimationTab);
        assertEquals(0, mInterruptCount);

        mCurrentTabSupplier.set(mAnotherTab);
        assertEquals(1, mInterruptCount);

        mCurrentTabSupplier.set(mYetAnotherTab);
        assertEquals(1, mInterruptCount);
    }

    @Test
    public void testLayoutChanged_TabSwitcher() {
        mLayoutStateObserverCaptor.getValue().onStartedShowing(LayoutType.TAB_SWITCHER);
        assertEquals(1, mInterruptCount);
    }

    @Test
    public void testLayoutChanged_ToolbarSwipe() {
        mLayoutStateObserverCaptor.getValue().onStartedShowing(LayoutType.TOOLBAR_SWIPE);
        assertEquals(1, mInterruptCount);
    }

    @Test
    public void testPageLoadStarted() {
        mTabObserverCaptor.getValue().onPageLoadStarted(mAnimationTab, GURL.emptyGURL());
        assertEquals(1, mInterruptCount);
    }

    @Test
    public void testScrimVisibility() {
        mScrimVisibilitySupplier.set(true);
        assertEquals(1, mInterruptCount);
    }

    @Test
    public void testContextMenuVisibility() {
        TabContextMenuData data = TabContextMenuData.getForTab(mAnimationTab);
        data.setLastTriggeringTouchPositionDp(new Point(0, 0));
        assertEquals(1, mInterruptCount);
    }

    @Test
    public void testNtpTransitionPercentageChanged_Interrupts() {
        recreateAnimationInterruptor(/* isRegularNtp= */ true);
        assertTrue(mNtpSearchBoxTransitionPercentageSupplier.hasObservers());

        mNtpSearchBoxTransitionPercentageSupplier.set(0.1f);
        assertEquals(1, mInterruptCount);
    }

    @Test
    public void testNtpTransitionPercentageChanged_NoInterrup() {
        recreateAnimationInterruptor(/* isRegularNtp= */ true);
        assertTrue(mNtpSearchBoxTransitionPercentageSupplier.hasObservers());

        mNtpSearchBoxTransitionPercentageSupplier.set(0f);
        assertEquals(0, mInterruptCount);

        mAnimationInterruptor.destroy();
    }
}
