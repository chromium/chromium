// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion.toolbar;

import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.components.visited_url_ranking.url_grouping.CachedSuggestions;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestion;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestionsService;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponse;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponseMetadata;

import java.util.ArrayList;
import java.util.List;

@NullMarked
public class GroupSuggestionsButtonControllerImpl implements GroupSuggestionsButtonController {

    private final GroupSuggestionsService mGroupSuggestionsService;
    private @Nullable CachedSuggestions mCachedSuggestions;
    private boolean mWasCachedSuggestionShown;

    public GroupSuggestionsButtonControllerImpl(GroupSuggestionsService groupSuggestionsService) {
        this.mGroupSuggestionsService = groupSuggestionsService;
    }

    @Override
    public boolean shouldShowButton(Tab tab, @WindowId int windowId) {
        if (mCachedSuggestions != null) {
            clearCachedSuggestion(UserResponse.NOT_SHOWN);
        }

        mCachedSuggestions = mGroupSuggestionsService.getCachedSuggestions(windowId);

        boolean suggestionIncludesTab = isSuggestionValidForTab(tab);

        if (!suggestionIncludesTab) {
            clearCachedSuggestion(UserResponse.NOT_SHOWN);
            return false;
        }

        return true;
    }

    @Override
    public void onButtonShown(Tab tab) {
        if (isSuggestionValidForTab(tab)) {
            mWasCachedSuggestionShown = true;
        } else {
            clearCachedSuggestion(UserResponse.NOT_SHOWN);
        }
    }

    @Override
    public void onButtonHidden() {
        if (mWasCachedSuggestionShown) {
            clearCachedSuggestion(UserResponse.IGNORED);
        } else {
            clearCachedSuggestion(UserResponse.NOT_SHOWN);
        }
    }

    @Override
    public void onButtonClicked(Tab tab, TabGroupModelFilter tabGroupModelFilter) {
        if (!isSuggestionValidForTab(tab)) {
            clearCachedSuggestion(UserResponse.UNKNOWN);
            return;
        }

        var tabIdsToGroup = getTabGroupListForTab(tab);

        if (tabIdsToGroup == null) {
            clearCachedSuggestion(UserResponse.UNKNOWN);
            return;
        }

        ArrayList<Integer> tabIdList = new ArrayList<>(tabIdsToGroup.length);
        for (int tabId : tabIdsToGroup) {
            if (tabId != tab.getId()) {
                tabIdList.add(tabId);
            }
        }

        var tabModel = tabGroupModelFilter.getTabModel();
        List<Tab> tabsToGroup =
                TabModelUtils.getTabsById(tabIdList, tabModel, /* allowClosing= */ false);
        tabGroupModelFilter.mergeListOfTabsToGroup(tabsToGroup, tab, /* notify= */ true);

        clearCachedSuggestion(UserResponse.ACCEPTED);
    }

    private void clearCachedSuggestion(@UserResponse int userResponse) {
        if (!isSuggestionValid()) {
            return;
        }

        var suggestionsList = getGroupSuggestionList();
        var suggestionId = suggestionsList.isEmpty() ? 0 : suggestionsList.get(0).suggestionId;

        mCachedSuggestions.userResponseMetadataCallback.onResult(
                new UserResponseMetadata(suggestionId, userResponse));
        mCachedSuggestions = null;
        mWasCachedSuggestionShown = false;
    }

    @EnsuresNonNullIf("mCachedSuggestions")
    private boolean isSuggestionValid() {
        return mCachedSuggestions != null
                && mCachedSuggestions.groupSuggestions != null
                && mCachedSuggestions.groupSuggestions.groupSuggestions != null;
    }

    private boolean isSuggestionValidForTab(Tab tab) {
        if (!isSuggestionValid()) {
            return false;
        }

        for (var suggestion : getGroupSuggestionList()) {
            var tabIds = suggestion.tabIds;
            for (var suggestionTabId : tabIds) {
                if (suggestionTabId == tab.getId()) {
                    return true;
                }
            }
        }

        return false;
    }

    private List<GroupSuggestion> getGroupSuggestionList() {
        if (mCachedSuggestions == null
                || mCachedSuggestions.groupSuggestions == null
                || mCachedSuggestions.groupSuggestions.groupSuggestions == null) {
            return List.of();
        }

        return mCachedSuggestions.groupSuggestions.groupSuggestions;
    }

    private int @Nullable [] getTabGroupListForTab(Tab tab) {
        if (!isSuggestionValidForTab(tab)) {
            return null;
        }

        for (var suggestion : getGroupSuggestionList()) {
            var tabIds = suggestion.tabIds;
            for (var suggestionTabId : tabIds) {
                if (suggestionTabId == tab.getId()) {
                    return tabIds;
                }
            }
        }

        return null;
    }
}
