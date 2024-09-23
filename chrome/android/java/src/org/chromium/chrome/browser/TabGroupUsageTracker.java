// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;

/** Tracks TabGroup usages related statistics. */
public class TabGroupUsageTracker implements PauseResumeWithNativeObserver, DestroyObserver {
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final TabModelSelector mTabModelSelector;
    private final Supplier<Boolean> mIsWarmOnResumeSupplier;
    private CallbackController mCallbackController = new CallbackController();

    /**
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the activity.
     * @param tabModelSelector The {@link TabModelSelector} for the activity.
     * @param isWarmOnResumeSupplier Whether the activity is warm on resume.
     */
    public static void initialize(
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            TabModelSelector tabModelSelector,
            Supplier<Boolean> isWarmOnResumeSupplier) {
        new TabGroupUsageTracker(
                activityLifecycleDispatcher, tabModelSelector, isWarmOnResumeSupplier);
    }

    private TabGroupUsageTracker(
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            TabModelSelector tabModelSelector,
            Supplier<Boolean> isWarmOnResumeSupplier) {
        mIsWarmOnResumeSupplier = isWarmOnResumeSupplier;

        mTabModelSelector = tabModelSelector;
        TabModelUtils.runOnTabStateInitialized(
                tabModelSelector,
                mCallbackController.makeCancelable(
                        unusedTabModelSelector -> recordTabGroupCount()));

        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        activityLifecycleDispatcher.register(this);
    }

    @Override
    public void onDestroy() {
        mCallbackController.destroy();
        mActivityLifecycleDispatcher.unregister(this);
    }

    @Override
    public void onResumeWithNative() {
        // Since we use AsyncTask for restoring tabs, this method can be called before or after
        // restoring all tabs. Therefore, we skip recording the count here during cold start and
        // record that elsewhere when TabModel emits the restoreCompleted signal.
        if (!mIsWarmOnResumeSupplier.get()) return;

        recordTabGroupCount();
    }

    @Override
    public void onPauseWithNative() {}

    private void recordTabGroupCount() {
        TabModelFilterProvider provider = mTabModelSelector.getTabModelFilterProvider();
        TabGroupModelFilter normalFilter = (TabGroupModelFilter) provider.getTabModelFilter(false);
        TabGroupModelFilter incognitoFilter =
                (TabGroupModelFilter) provider.getTabModelFilter(true);
        int groupCount = normalFilter.getTabGroupCount() + incognitoFilter.getTabGroupCount();
        RecordHistogram.recordCount1MHistogram("TabGroups.UserGroupCount", groupCount);
    }
}
