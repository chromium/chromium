// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.IS_HIGHLIGHTED;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TAB_ID;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Set;

/** Used to highlight tabs in the tab list. */
@NullMarked
public class TabListHighlighter {
    private final ModelList mModelList;

    /**
     * @param modelList The {@link ModelList} containing the tabs to be highlighted.
     */
    public TabListHighlighter(ModelList modelList) {
        mModelList = modelList;
    }

    /**
     * Highlights all tabs in the model list whose IDs are present in the given set. This method
     * sets their {@link TabProperties#IS_HIGHLIGHTED} property to true.
     *
     * <p>Does nothing if the tab is already highlighted. Highlighting a set of tabs does not remove
     * the highlights from previously highlighted tabs.
     *
     * @param tabIds A set of tab IDs to highlight.
     */
    public void highlightTabs(Set<@TabId Integer> tabIds) {
        for (ListItem listItem : mModelList) {
            PropertyModel model = listItem.model;
            if (model.containsKey(TAB_ID) && model.containsKey(IS_HIGHLIGHTED)) {
                @TabId int tabId = model.get(TAB_ID);
                if (tabIds.contains(tabId)) {
                    model.set(IS_HIGHLIGHTED, true);
                }
            }
        }
    }

    /**
     * Removes the highlights for all tabs in the model list. This method sets their {@link
     * TabProperties#IS_HIGHLIGHTED} property to false.
     */
    public void unhighlightTabs() {
        for (ListItem listItem : mModelList) {
            PropertyModel model = listItem.model;
            if (isHighLighted(model)) {
                model.set(IS_HIGHLIGHTED, false);
            }
        }
    }

    private boolean isHighLighted(PropertyModel model) {
        return model.containsKey(TAB_ID)
                && model.containsKey(IS_HIGHLIGHTED)
                && model.get(IS_HIGHLIGHTED);
    }
}
