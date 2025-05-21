// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import static org.mockito.Mockito.verify;

import android.os.Build.VERSION_CODES;

import androidx.activity.BackEventCompat;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.back_press.MinimizeAppAndCloseTabBackPressHandler.MinimizeAppAndCloseTabType;
import org.chromium.chrome.browser.back_press.MinimizeAppAndCloseTabBackPressHandler.TabClosureType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabLaunchType;

import java.util.concurrent.ExecutionException;
import java.util.function.Predicate;

/** Unit tests for {@link MinimizeAppAndCloseTabBackPressHandler}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseJUnit4ClassRunner.class)
public class MinimizeAppAndCloseTabBackPressHandlerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public ExpectedException thrown = ExpectedException.none();

    @Mock private Callback<Tab> mSendToBackground;

    @Mock private Predicate<Tab> mShouldCloseTab;

    @Mock private Predicate<Tab> mMinimizationShouldCloseTab;
    @Mock private Callback<Tab> mCloseTabUponMinimization;

    @Mock private Tab mTab;

    private MinimizeAppAndCloseTabBackPressHandler mHandler;
    private ObservableSupplierImpl<Tab> mActivityTabSupplier;

    @Before
    public void setUp() {
        createBackPressHandler();
    }

    @Test
    @SmallTest
    public void testMinimizeAppAndCloseTab() throws ExecutionException {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MinimizeAppAndCloseTabBackPressHandler.HISTOGRAM,
                                MinimizeAppAndCloseTabType.MINIMIZE_APP_AND_CLOSE_TAB)
                        .expectIntRecord(
                                "Android.BackPress.TabClosureType",
                                TabClosureType.CHROME_MINIMIZATION)
                        .build();
        Mockito.when(mShouldCloseTab.test(mTab)).thenReturn(true);
        Mockito.when(mMinimizationShouldCloseTab.test(mTab)).thenReturn(true);
        Mockito.when(mTab.getLaunchType()).thenReturn(TabLaunchType.FROM_EXTERNAL_APP);
        ThreadUtils.runOnUiThreadBlocking(() -> mActivityTabSupplier.set(mTab));
        Assert.assertTrue(mHandler.getHandleBackPressChangedSupplier().get());
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mHandler.handleOnBackStarted(
                                new BackEventCompat(0, 0, 0, BackEventCompat.EDGE_LEFT)));
        ThreadUtils.runOnUiThreadBlocking(mHandler::handleBackPress);
        verify(
                        mSendToBackground,
                        Mockito.description("App should be minimized with tab being closed"))
                .onResult(mTab);
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testMinimizeAppAndCloseTab_SystemBack() throws ExecutionException {
        createBackPressHandler(true);
        // Expect no change.
        testMinimizeAppAndCloseTab();
    }

    @Test
    @SmallTest
    public void testCloseTab() throws ExecutionException {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MinimizeAppAndCloseTabBackPressHandler.HISTOGRAM,
                                MinimizeAppAndCloseTabType.CLOSE_TAB)
                        .expectIntRecord(
                                "Android.BackPress.TabClosureType",
                                TabClosureType.WITHOUT_MINIMIZATION)
                        .build();
        Mockito.when(mShouldCloseTab.test(mTab)).thenReturn(true);
        UserDataHost userDataHost = ThreadUtils.runOnUiThreadBlocking(() -> new UserDataHost());
        Mockito.when(mTab.getUserDataHost()).thenReturn(userDataHost);
        ThreadUtils.runOnUiThreadBlocking(() -> TabAssociatedApp.from(mTab));
        Mockito.when(mTab.getLaunchType()).thenReturn(TabLaunchType.FROM_LONGPRESS_FOREGROUND);
        ThreadUtils.runOnUiThreadBlocking(() -> mActivityTabSupplier.set(mTab));
        Assert.assertTrue(mHandler.getHandleBackPressChangedSupplier().get());
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mHandler.handleOnBackStarted(
                                new BackEventCompat(0, 0, 0, BackEventCompat.EDGE_LEFT)));
        ThreadUtils.runOnUiThreadBlocking(mHandler::handleBackPress);

        verify(
                        mSendToBackground,
                        Mockito.never()
                                .description("Tab should be closed without minimizing the app."))
                .onResult(mTab);
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testCloseTab_SystemBack() throws ExecutionException {
        createBackPressHandler(true);
        // Expect no change.
        testCloseTab();
    }

    @Test
    @SmallTest
    public void testMinimizeApp_SystemBack() {
        createBackPressHandler(true);

        var histogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(MinimizeAppAndCloseTabBackPressHandler.HISTOGRAM)
                        .build();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTabSupplier.set(mTab);
                });
        Mockito.when(mShouldCloseTab.test(mTab)).thenReturn(false);

        Assert.assertFalse(
                "Back press should be handled by OS.",
                mHandler.getHandleBackPressChangedSupplier().get());
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.ALLOW_TAB_CLOSING_UPON_MINIMIZATION})
    public void testCloseTabDuringMinimization() {
        createBackPressHandler(true, true);
        Mockito.when(mShouldCloseTab.test(mTab)).thenReturn(true);
        Mockito.when(mMinimizationShouldCloseTab.test(mTab)).thenReturn(true);
        Mockito.when(mTab.getLaunchType()).thenReturn(TabLaunchType.FROM_EXTERNAL_APP);
        ThreadUtils.runOnUiThreadBlocking(() -> mActivityTabSupplier.set(mTab));
        Assert.assertFalse(mHandler.getHandleBackPressChangedSupplier().get());

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.BackPress.TabClosureType", TabClosureType.OS_MINIMIZATION);
        ThreadUtils.runOnUiThreadBlocking(mHandler::onSystemNavigation);
        verify(
                        mCloseTabUponMinimization,
                        Mockito.description("Tab should be closed during minimizing the app."))
                .onResult(mTab);
        histogram.assertExpected();
    }

    private void createBackPressHandler() {
        createBackPressHandler(false, false);
    }

    private void createBackPressHandler(boolean systemBack) {
        createBackPressHandler(systemBack, false);
    }

    private void createBackPressHandler(boolean systemBack, boolean systemMinimize) {
        if (systemMinimize) {
            MinimizeAppAndCloseTabBackPressHandler.setVersionForTesting(VERSION_CODES.BAKLAVA);
        } else if (systemBack) {
            MinimizeAppAndCloseTabBackPressHandler.setVersionForTesting(VERSION_CODES.TIRAMISU);
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTabSupplier = new ObservableSupplierImpl<>();
                });
        mHandler =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                new MinimizeAppAndCloseTabBackPressHandler(
                                        mActivityTabSupplier,
                                        mShouldCloseTab,
                                        mMinimizationShouldCloseTab,
                                        mCloseTabUponMinimization,
                                        mSendToBackground));
    }
}
