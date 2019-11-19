// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.List;

/**
 * Represents the output of the {@link TabSuggestions} pipeline.
 */
public final class TabSuggestion {
    /** Types of Suggestion Actions */
    @IntDef({TabSuggestion.TabSuggestionAction.GROUP, TabSuggestion.TabSuggestionAction.CLOSE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabSuggestionAction {
        int GROUP = 0;
        int CLOSE = 1;
    }

    private final List<TabContext.TabInfo> mTabsInfo;
    private final @TabSuggestionAction int mAction;
    private final String mProviderName;
    private final Integer mTabGroupId;

    public TabSuggestion(List<TabContext.TabInfo> tabsInfo, @TabSuggestionAction int action,
            String providerName) {
        this(tabsInfo, action, providerName, null);
    }

    public TabSuggestion(List<TabContext.TabInfo> tabsInfo, @TabSuggestionAction int action,
            String providerName, Integer tabGroupId) {
        mTabsInfo = Collections.unmodifiableList(tabsInfo);
        mAction = action;
        mProviderName = providerName;
        mTabGroupId = tabGroupId;
    }

    /**
     * Returns the list of the suggested tabs
     */
    public List<TabContext.TabInfo> getTabsInfo() {
        return mTabsInfo;
    }

    /**
     * Returns the suggested action
     */
    public @TabSuggestionAction int getAction() {
        return mAction;
    }

    /**
     * Returns the provider's name
     */
    public String getProviderName() {
        return mProviderName;
    }

    /**
     * Checks if the suggestion is for an existing group
     * @return true if the suggestion updates an existing group
     */
    public boolean hasExistingGroupId() {
        return getExistingTabGroupId() != null;
    }

    /**
     * If the suggestion is for an existing group, this will return the group id. Call @{link
     * hasExistingGroupId} before calling this getter.
     * @return existing group Id
     */
    public Integer getExistingTabGroupId() {
        return mTabGroupId;
    }
}
