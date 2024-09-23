// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.TabUsageTracker;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Test suite to verify that the TabUsageTracker correctly records the number of tabs used and the
 * percentage of tabs used.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabUsageTrackerTest {
    @Mock TabModelSelector mTabModelSelector;
    @Mock ActivityLifecycleDispatcher mDispatcher;
    @Mock TabModel mTabModel;

    private static final int INITIAL_TAB_COUNT = 0;
    private static final String NUMBER_OF_TABS_USED = "Android.ActivityStop.NumberOfTabsUsed";
    private static final String PERCENTAGE_OF_TABS_USED =
            "Android.ActivityStop.PercentageOfTabsUsed";

    private TabUsageTracker mTabUsageTracker;

    @Before
    public void setUp() throws TimeoutException {
        MockitoAnnotations.initMocks(this);
        List<TabModel> tabModels = new ArrayList<>();
        tabModels.add(mTabModel);

        Mockito.when(mTabModel.getCount()).thenReturn(INITIAL_TAB_COUNT);
        Mockito.when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        Mockito.when(mTabModelSelector.getModels()).thenReturn(tabModels);
        Mockito.when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);

        mTabUsageTracker = new TabUsageTracker(mDispatcher, mTabModelSelector);
    }

    @Test
    @SmallTest
    public void testOnStop_RecordsHistogram_NoInitialTabs() {
        // Arrange
        mTabUsageTracker.onResumeWithNative();
        Tab tab1 = getMockedTab(1);
        Tab tab2 = getMockedTab(2);

        // Act: Create 2 tabs, select 1 tab and call onStop.
        TabModelSelectorTabModelObserver observer =
                mTabUsageTracker.getTabModelSelectorTabModelObserverForTests();
        observer.didAddTab(
                tab1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND, false);
        observer.didAddTab(
                tab2, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND, false);
        observer.didSelectTab(tab1, TabLaunchType.FROM_CHROME_UI, 0);

        mTabUsageTracker.onStopWithNative();

        // Assert
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(NUMBER_OF_TABS_USED, 1));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(PERCENTAGE_OF_TABS_USED, 50));
    }

    @Test
    @SmallTest
    public void testOnStop_RecordsHistogram_HasInitialTabs() {
        // Arrange
        Tab tab1 = getMockedTab(1);
        Tab selectedTab = getMockedTab(3);
        // Start with 5 existing tabs and 1 selected tab.
        Mockito.when(mTabModelSelector.getTotalTabCount()).thenReturn(5);
        Mockito.when(mTabModelSelector.getCurrentTab()).thenReturn(selectedTab);
        mTabUsageTracker.onResumeWithNative();

        // Act: Create 1 tab, select 1 tab and call onStop.
        TabModelSelectorTabModelObserver observer =
                mTabUsageTracker.getTabModelSelectorTabModelObserverForTests();
        observer.didAddTab(
                tab1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND, false);
        observer.didSelectTab(tab1, TabLaunchType.FROM_CHROME_UI, 3);

        mTabUsageTracker.onStopWithNative();

        // Assert that number of tabs used is 2 and percentage is 2/6 * 100 = 33
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(NUMBER_OF_TABS_USED, 2));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(PERCENTAGE_OF_TABS_USED, 33));
    }

    @Test
    @SmallTest
    public void testOnStop_CalledBeforeOnResume_DoesNotRecordHistogram() {
        mTabUsageTracker.onStopWithNative();

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(NUMBER_OF_TABS_USED));
        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(PERCENTAGE_OF_TABS_USED));
    }

    @Test
    @SmallTest
    public void testOnDestroy() {
        mTabUsageTracker.onDestroy();

        Mockito.verify(mDispatcher).unregister(mTabUsageTracker);
    }

    private Tab getMockedTab(int id) {
        Tab tab = Mockito.mock(Tab.class);
        Mockito.when(tab.getId()).thenReturn(id);
        return tab;
    }
}
