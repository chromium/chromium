// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;

import java.util.HashSet;
import java.util.Set;

/**
 * Captures the percentage of tabs used, for metrics. This is done by finding the ratio of the
 * number of tabs used, to the total number of tabs available between ChromeTabbedActivity onResume
 * and onStop.
 */
public class TabUsageTracker
        implements StartStopWithNativeObserver, DestroyObserver, PauseResumeWithNativeObserver {
    private static final String PERCENTAGE_OF_TABS_USED_HISTOGRAM =
            "Android.ActivityStop.PercentageOfTabsUsed";
    private static final String NUMBER_OF_TABS_USED_HISTOGRAM =
            "Android.ActivityStop.NumberOfTabsUsed";

    private final Set<Integer> mTabsUsed = new HashSet<>();

    private int mInitialTabCount;
    private int mNewlyAddedTabCount;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final TabModelSelector mModelSelector;
    private TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;
    private boolean mApplicationResumed;
    private CallbackController mCallbackController = new CallbackController();

    /**
     * This method is used to initialize the TabUsageTracker.
     *
     * @param lifecycleDispatcher LifecycleDispatcher used to subscribe class to lifecycle events.
     * @param modelSelector TabModelSelector used to subscribe to TabModelSelectorTabModelObserver
     *     to capture when tabs are selected or new tabs are added.
     */
    public static void initialize(
            ActivityLifecycleDispatcher lifecycleDispatcher, TabModelSelector modelSelector) {
        new TabUsageTracker(lifecycleDispatcher, modelSelector);
    }

    public TabUsageTracker(
            ActivityLifecycleDispatcher lifecycleDispatcher, TabModelSelector modelSelector) {
        mInitialTabCount = 0;
        mNewlyAddedTabCount = 0;
        mModelSelector = modelSelector;
        mApplicationResumed = false;

        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
    }

    @Override
    public void onDestroy() {
        mCallbackController.destroy();
        mLifecycleDispatcher.unregister(this);
    }

    @Override
    public void onStartWithNative() {}

    /**
     * Records 2 histograms.
     * 1. Percentage of tabs used.
     * 2. Number of tabs used.
     */
    @Override
    public void onStopWithNative() {
        // If onResume was never called, return early to omit invalid samples.
        if (!mApplicationResumed) return;

        int totalTabCount = mInitialTabCount + mNewlyAddedTabCount;
        float totalTabsUsedPercentage = (float) mTabsUsed.size() / (float) totalTabCount * 100;

        RecordHistogram.recordPercentageHistogram(
                PERCENTAGE_OF_TABS_USED_HISTOGRAM, Math.round(totalTabsUsedPercentage));
        RecordHistogram.recordCount100Histogram(NUMBER_OF_TABS_USED_HISTOGRAM, mTabsUsed.size());

        mTabsUsed.clear();
        mNewlyAddedTabCount = 0;
        mInitialTabCount = 0;
        mTabModelSelectorTabModelObserver.destroy();
        mApplicationResumed = false;
    }

    /**
     * Initializes the tab count and the selected tab when CTA is resumed and starts observing for
     * tab selections or any new tab creations.
     */
    @Override
    public void onResumeWithNative() {
        TabModelUtils.runOnTabStateInitialized(
                mModelSelector,
                mCallbackController.makeCancelable(
                        (tabModelSelector) -> {
                            mInitialTabCount = tabModelSelector.getTotalTabCount();
                        }));

        Tab currentlySelectedTab = mModelSelector.getCurrentTab();
        if (currentlySelectedTab != null) mTabsUsed.add(currentlySelectedTab.getId());

        mTabModelSelectorTabModelObserver =
                new TabModelSelectorTabModelObserver(mModelSelector) {
                    @Override
                    public void didAddTab(
                            Tab tab, int type, int creationState, boolean markedForSelection) {
                        mNewlyAddedTabCount++;
                    }

                    @Override
                    public void didSelectTab(Tab tab, int type, int lastId) {
                        mTabsUsed.add(tab.getId());
                    }
                };
        mApplicationResumed = true;
    }

    @Override
    public void onPauseWithNative() {}

    public TabModelSelectorTabModelObserver getTabModelSelectorTabModelObserverForTests() {
        return mTabModelSelectorTabModelObserver;
    }
}
