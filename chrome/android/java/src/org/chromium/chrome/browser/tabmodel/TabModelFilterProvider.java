// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * This class is responsible for creating {@link TabModelFilter}s to be applied on the
 * {@link TabModel}s. It always owns two {@link TabModelFilter}s, one for normal {@link TabModel}
 * and one for incognito {@link TabModel}.
 */
public class TabModelFilterProvider {
    private List<TabModelFilter> mTabModelFilterList = Collections.emptyList();

    TabModelFilterProvider() {}

    TabModelFilterProvider(List<TabModel> tabModels) {
        List<TabModelFilter> filters = new ArrayList<>();
        for (int i = 0; i < tabModels.size(); i++) {
            filters.add(createTabModelFilter(tabModels.get(i)));
        }

        mTabModelFilterList = Collections.unmodifiableList(filters);
    }

    /**
     * This method adds {@link TabModelObserver} to both {@link TabModelFilter}s.
     * @param observer {@link TabModelObserver} to add.
     */
    public void addTabModelFilterObserver(TabModelObserver observer) {
        for (int i = 0; i < mTabModelFilterList.size(); i++) {
            mTabModelFilterList.get(i).addObserver(observer);
        }
    }

    /**
     * This method removes {@link TabModelObserver} from both {@link TabModelFilter}s.
     * @param observer {@link TabModelObserver} to remove.
     */
    public void removeTabModelFilterObserver(TabModelObserver observer) {
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

    /**
     * This method destroys all owned {@link TabModelFilter}.
     */
    public void destroy() {
        for (int i = 0; i < mTabModelFilterList.size(); i++) {
            mTabModelFilterList.get(i).destroy();
        }
    }

    /**
     * Return a {@link TabModelFilter} based on feature flags.
     * @param model The {@link TabModel} that the {@link TabModelFilter} acts on.
     * @return a {@link TabModelFilter}.
     */
    private TabModelFilter createTabModelFilter(TabModel model) {
        if (FeatureUtilities.isTabGroupsAndroidEnabled()) {
            TabManagementDelegate tabManagementDelegate = TabManagementModuleProvider.getDelegate();
            if (tabManagementDelegate != null) {
                return tabManagementDelegate.createTabGroupModelFilter(model);
            }
        }
        return new EmptyTabModelFilter(model);
    }
}
