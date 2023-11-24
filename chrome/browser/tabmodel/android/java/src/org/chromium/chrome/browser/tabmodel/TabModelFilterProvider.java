// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.VisibleForTesting;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * This class is responsible for creating {@link TabModelFilter}s to be applied on the
 * {@link TabModel}s. It always owns two {@link TabModelFilter}s, one for normal {@link TabModel}
 * and one for incognito {@link TabModel}.
 */
public class TabModelFilterProvider implements TabModelSelectorObserver {
    @VisibleForTesting public List<TabModelFilter> mTabModelFilterList = Collections.emptyList();
    private final List<TabModelObserver> mPendingTabModelObserver = new ArrayList<>();

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public TabModelFilterProvider() {}

    public void init(TabModelFilterFactory tabModelFilterFactory, List<TabModel> tabModels) {
        assert mTabModelFilterList.isEmpty();
        assert tabModels.size() > 0;

        List<TabModelFilter> filters = new ArrayList<>();
        for (int i = 0; i < tabModels.size(); i++) {
            filters.add(tabModelFilterFactory.createTabModelFilter(tabModels.get(i)));
        }

        mTabModelFilterList = Collections.unmodifiableList(filters);
        // Registers the pending observers.
        for (TabModelObserver observer : mPendingTabModelObserver) {
            for (TabModelFilter tabModelFilter : mTabModelFilterList) {
                tabModelFilter.addObserver(observer);
            }
        }
        mPendingTabModelObserver.clear();
    }

    /**
     * This method adds {@link TabModelObserver} to both {@link TabModelFilter}s. Caches the
     * observer until {@link TabModelFilter}s are created.
     * @param observer {@link TabModelObserver} to add.
     */
    public void addTabModelFilterObserver(TabModelObserver observer) {
        if (mTabModelFilterList.isEmpty()) {
            mPendingTabModelObserver.add(observer);
            return;
        }

        for (int i = 0; i < mTabModelFilterList.size(); i++) {
            mTabModelFilterList.get(i).addObserver(observer);
        }
    }

    /**
     * This method removes {@link TabModelObserver} from both {@link TabModelFilter}s.
     * @param observer {@link TabModelObserver} to remove.
     */
    public void removeTabModelFilterObserver(TabModelObserver observer) {
        if (mTabModelFilterList.isEmpty() && !mPendingTabModelObserver.isEmpty()) {
            mPendingTabModelObserver.remove(observer);
            return;
        }

        for (int i = 0; i < mTabModelFilterList.size(); i++) {
            mTabModelFilterList.get(i).removeObserver(observer);
        }
    }

    /**
     * This method returns a specific {@link TabModelFilter}.
     * @param isIncognito Use to indicate which {@link TabModelFilter} to return.
     * @return A {@link TabModelFilter}. This returns null, if this called before native library is
     * initialized.
     */
    public TabModelFilter getTabModelFilter(boolean isIncognito) {
        for (int i = 0; i < mTabModelFilterList.size(); i++) {
            if (mTabModelFilterList.get(i).isIncognito() == isIncognito) {
                return mTabModelFilterList.get(i);
            }
        }
        return null;
    }

    /**
     * This method returns the current {@link TabModelFilter}.
     * @return The current {@link TabModelFilter}. This returns null, if this called before native
     * library is initialized.
     */
    public TabModelFilter getCurrentTabModelFilter() {
        for (int i = 0; i < mTabModelFilterList.size(); i++) {
            if (mTabModelFilterList.get(i).isCurrentlySelectedFilter()) {
                return mTabModelFilterList.get(i);
            }
        }
        return null;
    }

    /** This method destroys all owned {@link TabModelFilter}. */
    public void destroy() {
        for (int i = 0; i < mTabModelFilterList.size(); i++) {
            mTabModelFilterList.get(i).destroy();
        }
        mPendingTabModelObserver.clear();
    }

    private void markTabStateInitialized() {
        for (TabModelFilter filter : mTabModelFilterList) {
            filter.markTabStateInitialized();
        }
    }

    // Override TabModelSelectorObserver.
    @Override
    public void onTabStateInitialized() {
        markTabStateInitialized();
    }

    /** Reset the internal filter list to allow initialization again. */
    public void resetTabModelFilterListForTesting() {
        mTabModelFilterList = Collections.emptyList();
    }
}
