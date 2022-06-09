// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import androidx.test.filters.SmallTest;

import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.Predicate;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.metrics.HistogramTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.chrome.browser.back_press.MinimizeAppAndCloseTabBackPressHandler.MinimizeAppAndCloseTabType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Unit tests for {@link MinimizeAppAndCloseTabBackPressHandler}.
 */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseJUnit4ClassRunner.class)
public class MinimizeAppAndCloseTabBackPressHandlerUnitTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public HistogramTestRule mHistogramTester = new HistogramTestRule();

    @Mock
    private TabModelSelector mTabModelSelector;

    @Mock
    private Callback<Tab> mSendToBackground;

    @Mock
    private Predicate<Tab> mShouldCloseTab;

    private MinimizeAppAndCloseTabBackPressHandler mHandler;
    private ObservableSupplierImpl<TabModelSelector> mTabModelSelectorObservableSupplier;

    @BeforeClass
    public static void setUpClass() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        ObservableSupplierImpl.setIgnoreThreadChecksForTesting(true);
    }

    @AfterClass
    public static void afterClass() {
        ObservableSupplierImpl.setIgnoreThreadChecksForTesting(false);
    }

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTabModelSelectorObservableSupplier = new ObservableSupplierImpl<>(); });
        mHandler = TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> new MinimizeAppAndCloseTabBackPressHandler(
                                mTabModelSelectorObservableSupplier, mShouldCloseTab,
                                mSendToBackground));
    }

    @Test
    @SmallTest
    public void testMinimizeAppAndCloseTab() {
        HistogramDelta d1 = new HistogramDelta(MinimizeAppAndCloseTabBackPressHandler.HISTOGRAM,
                MinimizeAppAndCloseTabType.MINIMIZE_APP_AND_CLOSE_TAB);
        Tab tab = Mockito.mock(Tab.class);
        Mockito.when(mTabModelSelector.getCurrentTab()).thenReturn(tab);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTabModelSelectorObservableSupplier.set(mTabModelSelector); });
        Mockito.when(mShouldCloseTab.test(tab)).thenReturn(true);
        UserDataHost userDataHost = new UserDataHost();
        Mockito.when(tab.getUserDataHost()).thenReturn(userDataHost);
        TabAssociatedApp.from(tab);
        Mockito.when(tab.getLaunchType()).thenReturn(TabLaunchType.FROM_EXTERNAL_APP);
        mHandler.handleBackPress();

        Mockito.verify(mSendToBackground,
                       Mockito.description("App should be minimized with tab being closed"))
                .onResult(tab);
        Assert.assertEquals(1, d1.getDelta());
    }

    @Test
    @SmallTest
    public void testCloseTab() {
        HistogramDelta d1 = new HistogramDelta(MinimizeAppAndCloseTabBackPressHandler.HISTOGRAM,
                MinimizeAppAndCloseTabType.CLOSE_TAB);
        Tab tab = Mockito.mock(Tab.class);
        Mockito.when(mTabModelSelector.getCurrentTab()).thenReturn(tab);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTabModelSelectorObservableSupplier.set(mTabModelSelector); });
        Mockito.when(mShouldCloseTab.test(tab)).thenReturn(true);
        UserDataHost userDataHost = new UserDataHost();
        Mockito.when(tab.getUserDataHost()).thenReturn(userDataHost);
        TabAssociatedApp.from(tab);
        Mockito.when(tab.getLaunchType()).thenReturn(TabLaunchType.FROM_LONGPRESS_FOREGROUND);
        mHandler.handleBackPress();

        Mockito.verify(mSendToBackground,
                       Mockito.never().description(
                               "Tab should be closed without minimizing the app."))
                .onResult(tab);
        Assert.assertEquals(1, d1.getDelta());
    }

    @Test
    @SmallTest
    public void testMinimizeApp() {
        HistogramDelta d1 = new HistogramDelta(MinimizeAppAndCloseTabBackPressHandler.HISTOGRAM,
                MinimizeAppAndCloseTabType.MINIMIZE_APP);
        Tab tab = Mockito.mock(Tab.class);
        Mockito.when(mTabModelSelector.getCurrentTab()).thenReturn(tab);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTabModelSelectorObservableSupplier.set(mTabModelSelector); });
        Mockito.when(mShouldCloseTab.test(tab)).thenReturn(false);
        mHandler.handleBackPress();

        Mockito.verify(mSendToBackground,
                       Mockito.description("App should be minimized without closing any tab"))
                .onResult(null);
        Assert.assertEquals(1, d1.getDelta());
    }
}
