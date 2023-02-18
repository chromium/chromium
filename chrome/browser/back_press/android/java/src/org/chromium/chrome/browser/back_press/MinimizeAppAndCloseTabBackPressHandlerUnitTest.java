// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.os.Build.VERSION_CODES;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.back_press.MinimizeAppAndCloseTabBackPressHandler.MinimizeAppAndCloseTabType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.function.Predicate;

/**
 * Unit tests for {@link MinimizeAppAndCloseTabBackPressHandler}.
 */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseJUnit4ClassRunner.class)
public class MinimizeAppAndCloseTabBackPressHandlerUnitTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ExpectedException thrown = ExpectedException.none();

    @Mock
    private Callback<Tab> mSendToBackground;

    @Mock
    private Predicate<Tab> mShouldCloseTab;

    @Mock
    private Tab mTab;

    @Mock
    private Runnable mFinalCallback;

    private MinimizeAppAndCloseTabBackPressHandler mHandler;
    private ObservableSupplierImpl<Tab> mActivityTabSupplier;

    @BeforeClass
    public static void setUpClass() {
        ObservableSupplierImpl.setIgnoreThreadChecksForTesting(true);
    }

    @AfterClass
    public static void afterClass() {
        ObservableSupplierImpl.setIgnoreThreadChecksForTesting(false);
    }

    @Before
    public void setUp() {
        createBackPressHandler();
    }

    @After
    public void tearDown() {
        MinimizeAppAndCloseTabBackPressHandler.setVersionForTesting(null);
    }

    @Test
    @SmallTest
    public void testMinimizeAppAndCloseTab() {
        var histogram = HistogramWatcher.newSingleRecordWatcher(
                MinimizeAppAndCloseTabBackPressHandler.HISTOGRAM,
                MinimizeAppAndCloseTabType.MINIMIZE_APP_AND_CLOSE_TAB);
        Mockito.when(mShouldCloseTab.test(mTab)).thenReturn(true);
        UserDataHost userDataHost = new UserDataHost();
        Mockito.when(mTab.getUserDataHost()).thenReturn(userDataHost);
        TabAssociatedApp.from(mTab);
        Mockito.when(mTab.getLaunchType()).thenReturn(TabLaunchType.FROM_EXTERNAL_APP);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mActivityTabSupplier.set(mTab); });
        Assert.assertTrue(mHandler.getHandleBackPressChangedSupplier().get());
        mHandler.handleBackPress();

        verify(mSendToBackground,
                Mockito.description("App should be minimized with tab being closed"))
                .onResult(mTab);
        histogram.assertExpected();
        verify(mFinalCallback).run();
    }

    @Test
    @SmallTest
    public void testMinimizeAppAndCloseTab_SystemBack() {
        createBackPressHandler(true);
        // Expect no change.
        testMinimizeAppAndCloseTab();
    }

    @Test
    @SmallTest
    public void testCloseTab() {
        var histogram = HistogramWatcher.newSingleRecordWatcher(
                MinimizeAppAndCloseTabBackPressHandler.HISTOGRAM,
                MinimizeAppAndCloseTabType.CLOSE_TAB);
        Mockito.when(mShouldCloseTab.test(mTab)).thenReturn(true);
        UserDataHost userDataHost = new UserDataHost();
        Mockito.when(mTab.getUserDataHost()).thenReturn(userDataHost);
        TabAssociatedApp.from(mTab);
        Mockito.when(mTab.getLaunchType()).thenReturn(TabLaunchType.FROM_LONGPRESS_FOREGROUND);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mActivityTabSupplier.set(mTab); });
        Assert.assertTrue(mHandler.getHandleBackPressChangedSupplier().get());
        mHandler.handleBackPress();

        verify(mSendToBackground,
                Mockito.never().description("Tab should be closed without minimizing the app."))
                .onResult(mTab);
        histogram.assertExpected();
        verify(mFinalCallback).run();
    }

    @Test
    @SmallTest
    public void testCloseTab_SystemBack() {
        createBackPressHandler(true);
        // Expect no change.
        testCloseTab();
    }

    @Test
    @SmallTest
    public void testMinimizeApp() {
        var histogram = HistogramWatcher.newSingleRecordWatcher(
                MinimizeAppAndCloseTabBackPressHandler.HISTOGRAM,
                MinimizeAppAndCloseTabType.MINIMIZE_APP);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mActivityTabSupplier.set(mTab); });
        Mockito.when(mShouldCloseTab.test(mTab)).thenReturn(false);
        Assert.assertTrue(mHandler.getHandleBackPressChangedSupplier().get());
        mHandler.handleBackPress();

        verify(mSendToBackground,
                Mockito.description("App should be minimized without closing any tab"))
                .onResult(null);
        histogram.assertExpected();
        verify(mFinalCallback).run();
    }

    @Test
    @SmallTest
    public void testMinimizeApp_SystemBack() {
        createBackPressHandler(true);

        var histogram = HistogramWatcher.newBuilder()
                                .expectNoRecords(MinimizeAppAndCloseTabBackPressHandler.HISTOGRAM)
                                .build();
        TestThreadUtils.runOnUiThreadBlocking(() -> { mActivityTabSupplier.set(mTab); });
        Mockito.when(mShouldCloseTab.test(mTab)).thenReturn(false);

        Assert.assertFalse("Back press should be handled by OS.",
                mHandler.getHandleBackPressChangedSupplier().get());
        histogram.assertExpected();
        verify(mFinalCallback, never()).run();
    }

    @Test
    @SmallTest
    public void testMinimizeApp_NoValidTab() {
        var histogram = HistogramWatcher.newSingleRecordWatcher(
                MinimizeAppAndCloseTabBackPressHandler.HISTOGRAM,
                MinimizeAppAndCloseTabType.MINIMIZE_APP);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mActivityTabSupplier.set(null); });
        Assert.assertTrue(mHandler.getHandleBackPressChangedSupplier().get());
        mHandler.handleBackPress();

        verify(mSendToBackground).onResult(null);
        verify(mSendToBackground,
                Mockito.description("App should be minimized without closing any tab"))
                .onResult(null);
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testMinimizeApp_NoValidTab_SystemBack() {
        createBackPressHandler(true);

        var histogram = HistogramWatcher.newBuilder().expectNoRecords(
                MinimizeAppAndCloseTabBackPressHandler.HISTOGRAM);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mActivityTabSupplier.set(null); });

        Assert.assertFalse("Back press should be handled by OS.",
                mHandler.getHandleBackPressChangedSupplier().get());

        thrown.expect(Matchers.instanceOf(AssertionError.class));
        thrown.expectMessage(
                "Should be disabled when there is no valid tab and back press will be consumed.");

        mHandler.handleBackPress();

        verify(mSendToBackground, Mockito.never()).onResult(Mockito.any());
        verify(mShouldCloseTab, Mockito.never()).test(Mockito.any());
    }

    private void createBackPressHandler() {
        createBackPressHandler(false);
    }

    private void createBackPressHandler(boolean systemBack) {
        TestValues testValues = new TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.BACK_GESTURE_REFACTOR, true);
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.BACK_GESTURE_REFACTOR, "system_back", systemBack + "");
        FeatureList.setTestValues(testValues);
        if (systemBack) {
            MinimizeAppAndCloseTabBackPressHandler.setVersionForTesting(VERSION_CODES.TIRAMISU);
        }
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTabSupplier = new ObservableSupplierImpl<>(); });
        mHandler = TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> new MinimizeAppAndCloseTabBackPressHandler(mActivityTabSupplier,
                                mShouldCloseTab, mSendToBackground, mFinalCallback));
    }
}
