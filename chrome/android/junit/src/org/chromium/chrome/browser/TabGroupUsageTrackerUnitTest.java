// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.ui.test.util.MockitoHelper;

/** Unit tests for {@link TabGroupUsageTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupUsageTrackerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Captor private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;

    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModelFilterProvider mTabModelFilterProvider;
    @Mock private TabGroupModelFilter mRegularTabModelFilter;
    @Mock private TabGroupModelFilter mIncognitoTabModelFilter;

    private TabGroupUsageTracker mUsageTracker;
    private boolean mIsWarmOnResume;

    @Before
    public void setUp() {
        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelFilterProvider.getTabModelFilter(false)).thenReturn(mRegularTabModelFilter);
        when(mTabModelFilterProvider.getTabModelFilter(true)).thenReturn(mIncognitoTabModelFilter);
    }

    @After
    public void tearDown() {
        if (mUsageTracker != null) {
            mUsageTracker.onDestroy();
            verify(mActivityLifecycleDispatcher).unregister(mUsageTracker);
        }
    }

    private void init(int regularGroups, int incognitoGroups) {
        MockitoHelper.doCallback(
                        0,
                        (usageTracker) -> {
                            mUsageTracker = (TabGroupUsageTracker) usageTracker;
                        })
                .when(mActivityLifecycleDispatcher)
                .register(any());
        when(mRegularTabModelFilter.getTabGroupCount()).thenReturn(regularGroups);
        when(mIncognitoTabModelFilter.getTabGroupCount()).thenReturn(incognitoGroups);
        TabGroupUsageTracker.initialize(
                mActivityLifecycleDispatcher, mTabModelSelector, () -> mIsWarmOnResume);
    }

    @Test
    public void testRecordOnCreateNotInitialized() {
        mIsWarmOnResume = false;
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);

        int regularGroups = 6;
        int incognitoGroups = 7;
        init(regularGroups, incognitoGroups);

        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "TabGroups.UserGroupCount", regularGroups + incognitoGroups);
        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        watcher.assertExpected();
    }

    @Test
    public void testRecordOnCreateInitialized() {
        mIsWarmOnResume = false;
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);

        int regularGroups = 1;
        int incognitoGroups = 2;
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "TabGroups.UserGroupCount", regularGroups + incognitoGroups);
        init(regularGroups, incognitoGroups);
        watcher.assertExpected();

        verify(mTabModelSelector, never()).addObserver(any());
    }

    @Test
    public void testRecordOnResumeWarm() {
        mIsWarmOnResume = true;
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);

        int regularGroups = 4;
        int incognitoGroups = 3;
        init(regularGroups, incognitoGroups);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "TabGroups.UserGroupCount", regularGroups + incognitoGroups);
        mUsageTracker.onResumeWithNative();
        watcher.assertExpected();
    }

    @Test
    public void testRecordOnResumeCold() {
        mIsWarmOnResume = false;
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);

        int regularGroups = 0;
        int incognitoGroups = 0;
        init(regularGroups, incognitoGroups);

        var watcher =
                HistogramWatcher.newBuilder().expectNoRecords("TabGroups.UserGroupCount").build();
        mUsageTracker.onResumeWithNative();
        watcher.assertExpected();
    }
}
