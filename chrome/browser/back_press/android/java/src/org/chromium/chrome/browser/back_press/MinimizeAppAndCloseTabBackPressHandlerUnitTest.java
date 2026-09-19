// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;

import android.os.Build.VERSION_CODES;

import androidx.activity.BackEventCompat;

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
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.back_press.MinimizeAppAndCloseTabBackPressHandler.MinimizeAppAndCloseTabType;
import org.chromium.chrome.browser.back_press.MinimizeAppAndCloseTabBackPressHandler.TabClosureType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;

import java.util.function.Predicate;

/** Unit tests for {@link MinimizeAppAndCloseTabBackPressHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MinimizeAppAndCloseTabBackPressHandlerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public ExpectedException thrown = ExpectedException.none();

    @Mock private Callback<Tab> mSendToBackground;

    @Mock private Predicate<Tab> mShouldCloseTab;

    @Mock private Predicate<Tab> mMinimizationShouldCloseTab;
    @Mock private Callback<Tab> mCloseTabUponMinimization;

    @Mock private Tab mTab;

    private MinimizeAppAndCloseTabBackPressHandler mHandler;
    private SettableNullableObservableSupplier<Tab> mActivityTabSupplier;

    @Before
    public void setUp() {
        createBackPressHandler();
    }

    @Test
    public void testMinimizeAppAndCloseTab() {
        testMinimizeAppAndCloseTabImpl();
    }

    private void testMinimizeAppAndCloseTabImpl() {
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
        mActivityTabSupplier.set(mTab);
        Assert.assertTrue(mHandler.getHandleBackPressChangedSupplier().get());
        mHandler.handleOnBackStarted(new BackEventCompat(0, 0, 0, BackEventCompat.EDGE_LEFT));
        mHandler.handleBackPress();
        verify(
                        mSendToBackground,
                        Mockito.description("App should be minimized with tab being closed"))
                .onResult(mTab);
        histogram.assertExpected();
    }

    @Test
    public void testMinimizeAppAndCloseTab_SystemBack() {
        createBackPressHandler(true);
        // Expect no change.
        testMinimizeAppAndCloseTabImpl();
    }

    @Test
    public void testCloseTab() {
        testCloseTabImpl();
    }

    private void testCloseTabImpl() {
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
        UserDataHost userDataHost = new UserDataHost();
        Mockito.when(mTab.getUserDataHost()).thenReturn(userDataHost);
        TabAssociatedApp.from(mTab);
        Mockito.when(mTab.getLaunchType()).thenReturn(TabLaunchType.FROM_LONGPRESS_FOREGROUND);
        mActivityTabSupplier.set(mTab);
        Assert.assertTrue(mHandler.getHandleBackPressChangedSupplier().get());
        mHandler.handleOnBackStarted(new BackEventCompat(0, 0, 0, BackEventCompat.EDGE_LEFT));
        mHandler.handleBackPress();

        verify(
                        mSendToBackground,
                        Mockito.never()
                                .description("Tab should be closed without minimizing the app."))
                .onResult(mTab);
        histogram.assertExpected();
    }

    @Test
    public void testCloseTab_SystemBack() {
        createBackPressHandler(true);
        // Expect no change.
        testCloseTabImpl();
    }

    @Test
    public void testMinimizeApp_SystemBack() {
        createBackPressHandler(true);

        var histogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(MinimizeAppAndCloseTabBackPressHandler.HISTOGRAM)
                        .build();
        mActivityTabSupplier.set(mTab);
        Mockito.when(mShouldCloseTab.test(mTab)).thenReturn(false);

        Assert.assertFalse(
                "Back press should be handled by OS.",
                mHandler.getHandleBackPressChangedSupplier().get());
        histogram.assertExpected();
    }

    @Test
    public void testCloseTabDuringMinimization() {
        createBackPressHandler(true, true);
        Mockito.when(mShouldCloseTab.test(mTab)).thenReturn(true);
        Mockito.when(mMinimizationShouldCloseTab.test(mTab)).thenReturn(true);
        Mockito.when(mTab.getLaunchType()).thenReturn(TabLaunchType.FROM_EXTERNAL_APP);
        mActivityTabSupplier.set(mTab);
        Assert.assertFalse(mHandler.getHandleBackPressChangedSupplier().get());

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.BackPress.TabClosureType", TabClosureType.OS_MINIMIZATION);
        mHandler.onSystemNavigation();
        verify(
                        mCloseTabUponMinimization,
                        Mockito.description("Tab should be closed during minimizing the app."))
                .onResult(mTab);
        histogram.assertExpected();
    }

    @Test
    public void testHandleBackPress_NavigatesBackIfCanGoBack() {
        Mockito.when(mTab.canGoBack()).thenReturn(true);
        Mockito.when(mShouldCloseTab.test(mTab)).thenReturn(true);
        mActivityTabSupplier.set(mTab);

        int result = mHandler.handleBackPress();

        Assert.assertEquals(BackPressResult.SUCCESS, result);
        verify(mTab).goBack();
        verify(mSendToBackground, Mockito.never()).onResult(any());
    }

    @Test
    public void testInvokeBackActionOnEscape() {
        Assert.assertFalse(
                "invokeBackActionOnEscape should return false.",
                mHandler.invokeBackActionOnEscape());
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
        } else {
            MinimizeAppAndCloseTabBackPressHandler.setVersionForTesting(VERSION_CODES.S_V2);
        }
        mActivityTabSupplier = ObservableSuppliers.createNullable();
        mHandler =
                new MinimizeAppAndCloseTabBackPressHandler(
                        mActivityTabSupplier,
                        mShouldCloseTab,
                        mMinimizationShouldCloseTab,
                        mCloseTabUponMinimization,
                        mSendToBackground);
    }
}
