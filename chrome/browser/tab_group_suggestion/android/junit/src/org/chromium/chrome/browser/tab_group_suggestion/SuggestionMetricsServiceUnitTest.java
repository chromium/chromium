// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.base.ApplicationStatus.onStateChangeForTesting;

import android.app.Activity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_suggestion.SuggestionMetricsService.GroupCreationSource;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** Unit tests for {@link SuggestionMetricsService}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SuggestionMetricsServiceUnitTest {
    private static final int WINDOW_ID = 0;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock private Activity mActivity;
    @Mock private Tab mTab1;

    @Captor private ArgumentCaptor<StartStopWithNativeObserver> mStartStopObserverCaptor;

    private final SettableNullableObservableSupplier<Tab> mCurrentTabSupplier =
            ObservableSuppliers.createNullable();
    private final Token mGtsGroupId = new Token(1, 1);
    private SuggestionMetricsService mSuggestionMetricsService;

    @Before
    public void setUp() {
        onStateChangeForTesting(mActivity, ActivityState.CREATED);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.getCurrentTabSupplier()).thenReturn(mCurrentTabSupplier);

        mSuggestionMetricsService = new SuggestionMetricsService();
        mSuggestionMetricsService.initializeTracker(
                WINDOW_ID, mActivity, mTabModelSelector, mLifecycleDispatcher);
        verify(mLifecycleDispatcher).register(mStartStopObserverCaptor.capture());

        when(mTab1.getTabGroupId()).thenReturn(mGtsGroupId);
    }

    @After
    public void tearDown() {
        ApplicationStatus.destroyForJUnitTests();
    }

    private void onStart() {
        mStartStopObserverCaptor.getValue().onStartWithNative();
    }

    private void onStop() {
        mStartStopObserverCaptor.getValue().onStopWithNative();
    }

    private void onDestroy() {
        onStateChangeForTesting(mActivity, ActivityState.DESTROYED);
    }

    private void selectTab(Tab tab) {
        mCurrentTabSupplier.set(tab);
    }

    @Test
    public void testOnSuggestionAccepted() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SuggestionMetricsTracker.TOTAL_SWITCHES_HISTOGRAM_PREFIX + "GTS", 1)
                        .expectIntRecord(
                                SuggestionMetricsTracker.PER_GROUP_SWITCHES_HISTOGRAM_PREFIX
                                        + "GTS",
                                1)
                        .build();

        mSuggestionMetricsService.onSuggestionAccepted(
                WINDOW_ID, GroupCreationSource.GTS_SUGGESTION, mGtsGroupId);
        selectTab(mTab1);

        onStop();
        watcher.assertExpected();
    }

    @Test
    public void testLifecycle_onStartWithNative() {
        mSuggestionMetricsService.onSuggestionAccepted(
                WINDOW_ID, GroupCreationSource.GTS_SUGGESTION, mGtsGroupId);
        selectTab(mTab1);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                SuggestionMetricsTracker.TOTAL_SWITCHES_HISTOGRAM_PREFIX + "GTS")
                        .build();

        onStart();
        onStop();
        watcher.assertExpected();
    }

    @Test
    public void testLifecycle_onStopWithNative() {
        mSuggestionMetricsService.onSuggestionAccepted(
                WINDOW_ID, GroupCreationSource.GTS_SUGGESTION, mGtsGroupId);
        selectTab(mTab1);

        HistogramWatcher watcher1 =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SuggestionMetricsTracker.TOTAL_SWITCHES_HISTOGRAM_PREFIX + "GTS", 1)
                        .build();
        onStop();
        watcher1.assertExpected();

        HistogramWatcher watcher2 =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                SuggestionMetricsTracker.TOTAL_SWITCHES_HISTOGRAM_PREFIX + "GTS")
                        .build();
        onStop();
        watcher2.assertExpected();
    }

    @Test
    public void testLifecycle_onDestroy() {
        onStateChangeForTesting(mActivity, ActivityState.PAUSED);

        StartStopWithNativeObserver observer = mStartStopObserverCaptor.getValue();

        assertTrue(mCurrentTabSupplier.hasObservers());

        onDestroy();

        verify(mLifecycleDispatcher).unregister(observer);
        assertFalse(mCurrentTabSupplier.hasObservers());
    }

    @Test(expected = AssertionError.class)
    public void testOnSuggestionAccepted_uninitializedWindow() {
        mSuggestionMetricsService.onSuggestionAccepted(
                WINDOW_ID + 1, GroupCreationSource.GTS_SUGGESTION, mGtsGroupId);
    }
}
