// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;

/** Records tab switcher related metrics for the Hub. */
public class HubTabSwitcherMetricsRecorder {
    private final Callback<Boolean> mOnHubVisiblityChanged = this::onHubVisiblilityChanged;
    private final Callback<TabModel> mOnTabModelChanged = this::onTabModelChanged;
    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                    onTabSelected(tab, lastId);
                }
            };

    private final @NonNull TabModelSelector mTabModelSelector;
    private final @NonNull ObservableSupplier<Boolean> mHubVisibilitySupplier;
    private final @NonNull ObservableSupplier<Pane> mFocusedPaneSupplier;

    private @Nullable TabModel mTabModelWhenShown;
    private @Nullable Integer mPaneIdWhenShown;
    private int mTabIdWhenShown = Tab.INVALID_TAB_ID;
    private int mIndexInModelWhenSwitched = TabModel.INVALID_TAB_INDEX;

    /**
     * @param tabModelSelector The {@link TabModelSelector} for the window.
     * @param hubVisibilitySupplier The supplier for whether the Hub is visible.
     * @param focusedPaneSupplier The supplier of the focused {@link Pane}.
     */
    public HubTabSwitcherMetricsRecorder(
            @NonNull TabModelSelector tabModelSelector,
            @NonNull ObservableSupplier<Boolean> hubVisibilitySupplier,
            @NonNull ObservableSupplier<Pane> focusedPaneSupplier) {
        mTabModelSelector = tabModelSelector;

        mHubVisibilitySupplier = hubVisibilitySupplier;
        hubVisibilitySupplier.addObserver(mOnHubVisiblityChanged);

        mFocusedPaneSupplier = focusedPaneSupplier;
    }

    /** Destroys the metrics recorder and removes any observers. */
    public void destroy() {
        mHubVisibilitySupplier.removeObserver(mOnHubVisiblityChanged);
        detachObserversAndReset();
    }

    private void onTabSelected(@Nullable Tab tab, int lastId) {
        if (tab == null || mPaneIdWhenShown == null) return;

        Pane currentPane = mFocusedPaneSupplier.get();
        if (currentPane == null) return;

        TabModel tabModel = mTabModelSelector.getCurrentModel();
        Tab previousTab = tabModel.getTabById(lastId);
        if (previousTab == null) return;

        if (mPaneIdWhenShown.intValue() == currentPane.getPaneId()) {
            if (tab.getId() == mTabIdWhenShown) {
                // TODO(crbug.com/40132120): Differentiate list.
                if (!TabUiFeatureUtilities.shouldUseListMode()) {
                    RecordUserAction.record("MobileTabReturnedToCurrentTab.TabGrid");
                }

                RecordUserAction.record("MobileTabReturnedToCurrentTab");
                RecordHistogram.recordSparseHistogram("Tabs.TabOffsetOfSwitch.GridTabSwitcher", 0);
            } else {
                TabModelFilter filter =
                        mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
                int previousIndex = filter.indexOf(previousTab);
                int currentIndex = filter.indexOf(tab);
                if (previousIndex != currentIndex) {
                    if (!filter.isTabInTabGroup(tab)) {
                        RecordUserAction.record("MobileTabSwitched.GridTabSwitcher");
                    }
                    // The sign on this metric is inverted from the direction of travel in the tab
                    // switcher and tab model. This was a pre-existing issue in TabSwitcherMediator.
                    RecordHistogram.recordSparseHistogram(
                            "Tabs.TabOffsetOfSwitch.GridTabSwitcher", previousIndex - currentIndex);
                }
            }
        } else {
            int currentIndex = TabModelUtils.getTabIndexById(tabModel, tab.getId());
            if (currentIndex == mIndexInModelWhenSwitched) {
                // TabModelImpl logs this action only when a different index is set within a
                // TabModelImpl. If we switch between normal tab model and incognito tab model and
                // leave the index the same (i.e. after switched tab model and select the
                // highlighted tab), TabModelImpl doesn't catch this case. Therefore, we record it
                // here.
                RecordUserAction.record("MobileTabSwitched");
            }

            TabModelFilter filter =
                    mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
            if (!filter.isTabInTabGroup(tab)) {
                RecordUserAction.record("MobileTabSwitched.GridTabSwitcher");
            }
        }
    }

    private void onHubVisiblilityChanged(boolean visible) {
        if (visible) {
            attachObserversAndInit();
        } else {
            detachObserversAndReset();
        }
    }

    private void attachObserversAndInit() {
        Pane pane = mFocusedPaneSupplier.get();
        if (pane != null) {
            mPaneIdWhenShown = pane.getPaneId();
        }
        TabModel tabModel = mTabModelSelector.getCurrentModel();
        mTabModelWhenShown = tabModel;
        mTabIdWhenShown = TabModelUtils.getCurrentTabId(tabModel);

        mTabModelSelector.getCurrentTabModelSupplier().addObserver(mOnTabModelChanged);
        mTabModelSelector.getModel(true).addObserver(mTabModelObserver);
        mTabModelSelector.getModel(false).addObserver(mTabModelObserver);
    }

    private void detachObserversAndReset() {
        mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mOnTabModelChanged);
        mTabModelSelector.getModel(true).removeObserver(mTabModelObserver);
        mTabModelSelector.getModel(false).removeObserver(mTabModelObserver);

        mTabModelWhenShown = null;
        mPaneIdWhenShown = null;
        mTabIdWhenShown = Tab.INVALID_TAB_ID;
        mIndexInModelWhenSwitched = TabModel.INVALID_TAB_INDEX;
    }

    private void onTabModelChanged(TabModel tabModel) {
        if (tabModel == null) return;

        if (mTabModelWhenShown == tabModel) {
            mIndexInModelWhenSwitched = TabModel.INVALID_TAB_INDEX;
            return;
        }

        mIndexInModelWhenSwitched = tabModel.index();
    }
}
