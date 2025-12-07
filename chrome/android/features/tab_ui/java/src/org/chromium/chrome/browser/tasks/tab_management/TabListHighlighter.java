// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.HIGHLIGHT_STATE;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TAB_ID;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabCardHighlightState;
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
     * sets their {@link TabProperties#HIGHLIGHT_STATE} property to true.
     *
     * <p>Does nothing if the tab is already highlighted. Highlighting a set of tabs does not remove
     * the highlights from previously highlighted tabs.
     *
     * @param tabIds A set of tab IDs to highlight.
     */
    public void highlightTabs(Set<@TabId Integer> tabIds) {
        for (ListItem listItem : mModelList) {
            PropertyModel model = listItem.model;
            if (model.containsKey(TAB_ID) && model.containsKey(HIGHLIGHT_STATE)) {
                @TabId int tabId = model.get(TAB_ID);
                if (tabIds.contains(tabId)) {
                    model.set(HIGHLIGHT_STATE, TabCardHighlightState.TO_BE_HIGHLIGHTED);
                }
            }
        }
    }

    /**
     * Removes the highlights for all tabs in the model list. This method sets their {@link
     * TabProperties#HIGHLIGHT_STATE} property to false.
     */
    public void unhighlightTabs() {
        for (ListItem listItem : mModelList) {
            PropertyModel model = listItem.model;
            if (isHighLighted(model)) {
                model.set(HIGHLIGHT_STATE, TabCardHighlightState.NOT_HIGHLIGHTED);
            }
        }
    }

    private boolean isHighLighted(PropertyModel model) {
        return model.containsKey(TAB_ID)
                && model.containsKey(HIGHLIGHT_STATE)
                && model.get(HIGHLIGHT_STATE) != TabCardHighlightState.NOT_HIGHLIGHTED;
    }
}
