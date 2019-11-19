// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAttributeKeys;
import org.chromium.chrome.browser.tab.TabAttributes;

/**
 * Implementation of the TabModelOrderController based off of tab_strip_model_order_controller.cc
 * and tab_strip_model.cc
 */
public class TabModelOrderControllerImpl implements TabModelOrderController {
    private static final int NO_TAB = -1;
    private final TabModelSelector mTabModelSelector;

    public TabModelOrderControllerImpl(TabModelSelector modelSelector) {
        mTabModelSelector = modelSelector;
    }

    @Override
    public int determineInsertionIndex(@TabLaunchType int type, int position, Tab newTab) {
        if (type == TabLaunchType.FROM_BROWSER_ACTIONS) return -1;
        if (linkClicked(type)) {
            position = determineInsertionIndex(type, newTab);
        }

        if (willOpenInForeground(type, newTab.isIncognito())) {
            // Forget any existing relationships, we don't want to make things
            // too confusing by having multiple groups active at the same time.
            forgetAllOpeners();
        }

        return position;
    }

    @Override
    public int determineInsertionIndex(@TabLaunchType int type, Tab newTab) {
        TabModel currentModel = mTabModelSelector.getCurrentModel();

        if (sameModelType(currentModel, newTab)) {
            Tab currentTab = TabModelUtils.getCurrentTab(currentModel);
            if (currentTab == null) {
                assert (currentModel.getCount() == 0);
                return 0;
            }
            int currentId = currentTab.getId();
            int currentIndex = TabModelUtils.getTabIndexById(currentModel, currentId);

            if (willOpenInForeground(type, newTab.isIncognito())) {
                // If the tab was opened in the foreground, insert it adjacent to its parent tab if
                // that exists and that tab is not the current selected tab, else insert the tab
                // adjacent to the current tab that opened that link.
                Tab parentTab = TabModelUtils.getTabById(currentModel, newTab.getParentId());
                if (parentTab != null && currentTab != parentTab) {
                    int parentTabIndex =
                            TabModelUtils.getTabIndexById(currentModel, parentTab.getId());
                    return parentTabIndex + 1;
                }
                return currentIndex + 1;
            } else {
                // If the tab was opened in the background, position at the end of
                // it's 'group'.
                int index = getIndexOfLastTabOpenedBy(currentId, currentIndex);
                if (index != NO_TAB) {
                    return index + 1;
                } else {
                    return currentIndex + 1;
                }
            }
        } else {
            // If the tab is opening in the other model type, just put it at the end.
            return mTabModelSelector.getModel(newTab.isIncognito()).getCount();
        }
    }

    /**
     * Returns the index of the last tab in the model opened by the specified
     * opener, starting at startIndex. To clarify, the tabs are traversed in the
     * descending order of their position in the model. This means that the tab
     * furthest in the stack with the given opener id will be returned.
     *
     * @param openerId The opener of interest.
     * @param startIndex The start point of the search.
     * @return The last tab if found, NO_TAB otherwise.
     */
    private int getIndexOfLastTabOpenedBy(int openerId, int startIndex) {
        TabModel currentModel = mTabModelSelector.getCurrentModel();
        int count = currentModel.getCount();
        for (int i = count - 1; i >= startIndex; i--) {
            Tab tab = currentModel.getTabAt(i);
            if (tab.getParentId() == openerId
                    && TabAttributes.from(tab).get(TabAttributeKeys.GROUPED_WITH_PARENT, true)) {
                return i;
            }
        }
        return NO_TAB;
    }

    /**
     * Clear the opener attribute on all tabs in the model.
     */
    void forgetAllOpeners() {
        TabModel currentModel = mTabModelSelector.getCurrentModel();
        int count = currentModel.getCount();
        for (int i = 0; i < count; i++) {
            TabAttributes.from(currentModel.getTabAt(i))
                    .set(TabAttributeKeys.GROUPED_WITH_PARENT, false);
        }
    }

    /**
     * Determine if a launch type is the result of linked being clicked.
     */
    static boolean linkClicked(@TabLaunchType int type) {
        return type == TabLaunchType.FROM_LINK || type == TabLaunchType.FROM_LONGPRESS_FOREGROUND
                || type == TabLaunchType.FROM_LONGPRESS_BACKGROUND;
    }

    @Override
    public boolean willOpenInForeground(@TabLaunchType int type, boolean isNewTabIncognito) {
        // Restore is handling the active index by itself.
        if (type == TabLaunchType.FROM_RESTORE || type == TabLaunchType.FROM_BROWSER_ACTIONS) {
            return false;
        }
        return type != TabLaunchType.FROM_LONGPRESS_BACKGROUND
                || (!mTabModelSelector.isIncognitoSelected() && isNewTabIncognito);
    }

    /**
     * @return {@code true} If both tabs have the same model type, {@code false} otherwise.
     */
    static boolean sameModelType(TabModel model, Tab tab) {
        return model.isIncognito() == tab.isIncognito();
    }
}
