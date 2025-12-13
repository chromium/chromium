// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
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
@NullMarked
public class TabUsageTracker
        implements StartStopWithNativeObserver, DestroyObserver, PauseResumeWithNativeObserver {
    private static final String PERCENTAGE_OF_TABS_USED_HISTOGRAM =
            "Android.ActivityStop.PercentageOfTabsUsed";
    private static final String NUMBER_OF_TABS_USED_HISTOGRAM =
            "Android.ActivityStop.NumberOfTabsUsed";
    private static final String PERCENTAGE_OF_PINNED_TABS_USED_HISTOGRAM =
            "Android.ActivityStop.PercentageOfPinnedTabsUsed";
    private static final String NUMBER_OF_PINNED_TABS_USED_HISTOGRAM =
            "Android.ActivityStop.NumberOfPinnedTabsUsed";

    private final Set<Integer> mTabsUsed = new HashSet<>();
    private final Set<Integer> mPinnedTabsUsed = new HashSet<>();

    private int mInitialTabCount;
    private int mNewlyAddedTabCount;
    private int mInitialPinnedTabCount;
    private int mNewlyAddedPinnedTabCount;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final TabModelSelector mModelSelector;
    private @Nullable TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;
    private boolean mApplicationResumed;
    private final CallbackController mCallbackController = new CallbackController();

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
        mInitialPinnedTabCount = 0;
        mNewlyAddedPinnedTabCount = 0;
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
     * Records 4 histograms.
     * 1. Percentage of tabs used.
     * 2. Number of tabs used.
     * 3. Percentage of pinned tabs used.
     * 4. Number of pinned tabs used.
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

        int totalPinnedTabCount = mInitialPinnedTabCount + mNewlyAddedPinnedTabCount;
        float totalPinnedTabsUsedPercentage =
                (float) mPinnedTabsUsed.size() / (float) totalPinnedTabCount * 100;
        RecordHistogram.recordPercentageHistogram(
                PERCENTAGE_OF_PINNED_TABS_USED_HISTOGRAM,
                Math.round(totalPinnedTabsUsedPercentage));
        RecordHistogram.recordCount100Histogram(
                NUMBER_OF_PINNED_TABS_USED_HISTOGRAM, mPinnedTabsUsed.size());

        mTabsUsed.clear();
        mPinnedTabsUsed.clear();
        mNewlyAddedTabCount = 0;
        mInitialTabCount = 0;
        mNewlyAddedPinnedTabCount = 0;
        mInitialPinnedTabCount = 0;
        assumeNonNull(mTabModelSelectorTabModelObserver);
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
                            mInitialPinnedTabCount = tabModelSelector.getTotalPinnedTabCount();
                        }));

        Tab currentlySelectedTab = mModelSelector.getCurrentTab();
        if (currentlySelectedTab != null) {
            mTabsUsed.add(currentlySelectedTab.getId());
            if (currentlySelectedTab.getIsPinned()) {
                mPinnedTabsUsed.add(currentlySelectedTab.getId());
            }
        }

        mTabModelSelectorTabModelObserver =
                new TabModelSelectorTabModelObserver(mModelSelector) {
                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        mNewlyAddedTabCount++;
                        if (tab.getIsPinned()) {
                            mNewlyAddedPinnedTabCount++;
                        }
                    }

                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        mTabsUsed.add(tab.getId());
                        if (tab.getIsPinned()) {
                            mPinnedTabsUsed.add(tab.getId());
                        }
                    }
                };
        mApplicationResumed = true;
    }

    @Override
    public void onPauseWithNative() {}

    public @Nullable
            TabModelSelectorTabModelObserver getTabModelSelectorTabModelObserverForTests() {
        return mTabModelSelectorTabModelObserver;
    }
}
