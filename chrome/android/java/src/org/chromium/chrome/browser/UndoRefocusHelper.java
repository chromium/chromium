// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Refocus on previously selected tab if the selected tab closure was undone. */
public class UndoRefocusHelper {
    private final Set<Integer> mTabsClosedFromTabStrip;
    private final TabModelSelector mModelSelector;
    private final ObservableSupplier<LayoutManagerImpl> mLayoutManagerObservableSupplier;

    private LayoutManagerImpl mLayoutManager;
    private LayoutStateProvider.LayoutStateObserver mLayoutStateObserver;
    private TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;
    private int mSelectedTabIdWhenTabClosed = Tab.INVALID_TAB_ID;
    private boolean mTabSwitcherActive;
    private Callback<LayoutManagerImpl> mLayoutManagerSupplierCallback;
    private boolean mIsTablet;

    /**
     * @param modelSelector TabModelSelector used to subscribe to TabModelSelectorTabModelObserver
     *     to capture when tabs are being closed or the closure is being undone.
     * @param layoutManagerObservableSupplier This supplies the LayoutManager implementation to
     *     observe the layout state when it's available.
     * @param isTablet Whether the current device is a tablet.
     */
    public UndoRefocusHelper(
            TabModelSelector modelSelector,
            ObservableSupplier<LayoutManagerImpl> layoutManagerObservableSupplier,
            boolean isTablet) {
        mLayoutManagerObservableSupplier = layoutManagerObservableSupplier;
        mModelSelector = modelSelector;
        mTabsClosedFromTabStrip = new HashSet<>();
        mTabSwitcherActive = false;
        mIsTablet = isTablet;

        observeTabModel();
        observeLayoutState();
    }

    public void destroy() {
        mTabModelSelectorTabModelObserver.destroy();
        mLayoutManagerObservableSupplier.removeObserver(mLayoutManagerSupplierCallback);
        if (mLayoutManager != null) {
            mLayoutManager.removeObserver(mLayoutStateObserver);
        }
    }

    private void observeTabModel() {
        mTabModelSelectorTabModelObserver =
                new TabModelSelectorTabModelObserver(mModelSelector) {
                    @Override
                    public void willCloseTab(Tab tab, boolean didCloseAlone) {
                        // Tabs not closed alone are handled in #willCloseMultipleTabs and
                        // #willCloseAllTabs
                        if (!didCloseAlone || tab.isIncognito()) return;

                        int tabId = tab.getId();
                        if (!mTabSwitcherActive && mIsTablet) {
                            mTabsClosedFromTabStrip.add(tabId);
                        }

                        maybeSetSelectedTabId(tab);
                    }

                    @Override
                    public void willCloseMultipleTabs(boolean allowUndo, List<Tab> tabs) {
                        if (!allowUndo || tabs.isEmpty()) return;

                        // Record metric only once for the set.
                        // Use the first id to track the set.
                        if (!mTabSwitcherActive && mIsTablet) {
                            mTabsClosedFromTabStrip.add(tabs.get(0).getId());
                        }
                        for (Tab tab : tabs) {
                            if (maybeSetSelectedTabId(tab)) {
                                break;
                            }
                        }
                    }

                    @Override
                    public void willCloseAllTabs(boolean incognito) {
                        if (incognito) return;
                        int selectedTabIdx = mModelSelector.getModel(false).index();
                        // Selected tab will be invalid when there are already no tabs in a window
                        // when this method is invoked.
                        if (selectedTabIdx == TabList.INVALID_TAB_INDEX) return;

                        Tab selectedTab = mModelSelector.getModel(false).getTabAt(selectedTabIdx);
                        maybeSetSelectedTabId(selectedTab);
                        // Record metric only once for the set.
                        // Use the selected id to track the set.
                        if (!mTabSwitcherActive && mIsTablet) {
                            mTabsClosedFromTabStrip.add(selectedTab.getId());
                        }
                    }

                    @Override
                    public void didSelectTab(Tab tab, int type, int lastId) {
                        // Undoing a selected tab closure, after manually switching tabs shouldn't
                        // switch focus to the reopened tab.
                        if (type == TabSelectionType.FROM_USER
                                || type == TabSelectionType.FROM_OMNIBOX
                                || type == TabSelectionType.FROM_NEW) {
                            resetSelectionsForUndo();
                        }
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        int id = tab.getId();
                        recordClosureCancellation(id);
                        if (mSelectedTabIdWhenTabClosed == id) {
                            selectPreviouslySelectedTab();
                        }
                    }

                    @Override
                    public void allTabsClosureUndone() {
                        if (mSelectedTabIdWhenTabClosed != Tab.INVALID_TAB_ID) {
                            selectPreviouslySelectedTab();
                        }

                        resetSelectionsForUndo();
                        mTabsClosedFromTabStrip.clear();
                    }

                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        if (!tab.isIncognito()) {
                            if (tab.getId() == mSelectedTabIdWhenTabClosed) {
                                resetSelectionsForUndo();
                            }
                            mTabsClosedFromTabStrip.remove(tab.getId());
                        }
                    }

                    @Override
                    public void allTabsClosureCommitted(boolean isIncognito) {
                        if (!isIncognito) {
                            resetSelectionsForUndo();
                            mTabsClosedFromTabStrip.clear();
                        }
                    }

                    private boolean maybeSetSelectedTabId(Tab tab) {
                        TabModel model = mModelSelector.getModel(false);
                        int tabId = tab.getId();
                        int selTabIndex = model.index();
                        if (selTabIndex != TabModel.INVALID_TAB_INDEX
                                && selTabIndex < model.getCount()) {
                            Tab selectedTab = model.getTabAt(selTabIndex);
                            if (selectedTab != null && tabId == selectedTab.getId()) {
                                mSelectedTabIdWhenTabClosed = tabId;
                                return true;
                            }
                        }
                        return false;
                    }

                    private void recordClosureCancellation(int id) {
                        // Only record the action if the tab was previously closed from the tab
                        // strip.
                        if (mTabsClosedFromTabStrip.contains(id)) {
                            RecordUserAction.record("TabletTabStrip.UndoCloseTab");
                            mTabsClosedFromTabStrip.remove(id);
                        }
                    }
                };
    }

    private void observeLayoutState() {
        mLayoutManagerSupplierCallback = this::onLayoutManagerAvailable;
        mLayoutManagerObservableSupplier.addObserver(mLayoutManagerSupplierCallback);
        @Nullable LayoutManagerImpl layoutManager = mLayoutManagerObservableSupplier.get();
        if (layoutManager != null && layoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER)) {
            mTabSwitcherActive = true;
        }
    }

    private void onLayoutManagerAvailable(LayoutManagerImpl layoutManager) {
        mLayoutManager = layoutManager;
        mLayoutStateObserver =
                new LayoutStateProvider.LayoutStateObserver() {
                    @Override
                    public void onFinishedShowing(int layoutType) {
                        if (layoutType != LayoutType.TAB_SWITCHER) {
                            return;
                        }
                        mTabSwitcherActive = true;
                    }

                    @Override
                    public void onFinishedHiding(int layoutType) {
                        if (layoutType != LayoutType.TAB_SWITCHER) {
                            return;
                        }
                        mTabSwitcherActive = false;
                    }
                };

        mLayoutManager.addObserver(mLayoutStateObserver);
    }

    /** If a tab closure is undone, this selects tab if it was previously selected. */
    private void selectPreviouslySelectedTab() {
        if (mSelectedTabIdWhenTabClosed == Tab.INVALID_TAB_ID) return;

        TabModelUtils.selectTabById(
                mModelSelector, mSelectedTabIdWhenTabClosed, TabSelectionType.FROM_UNDO);
        resetSelectionsForUndo();
    }

    /**
     * After a tab closure has been committed or user manually selects a different tab, these values
     * are reset so the next undo closure action does not reselect the reopened tab.
     */
    private void resetSelectionsForUndo() {
        mSelectedTabIdWhenTabClosed = Tab.INVALID_TAB_ID;
    }
}
