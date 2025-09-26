// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion.toolbar;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tab_group_suggestion.SuggestionMetricsService.GroupCreationSource.CPA_SUGGESTION;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_suggestion.SuggestionMetricsService;
import org.chromium.chrome.browser.tab_group_suggestion.SuggestionMetricsServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter.MergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.components.visited_url_ranking.url_grouping.CachedSuggestions;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestion;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestions;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestionsService;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponse;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponseMetadata;

import java.util.ArrayList;
import java.util.List;

@NullMarked
public class GroupSuggestionsButtonControllerImpl implements GroupSuggestionsButtonController {

    /** A data class used to associate a cached suggestion to a window. */
    private static class WindowedSuggestion {
        public final @WindowId int windowId;
        public final CachedSuggestions cachedSuggestions;

        WindowedSuggestion(@WindowId int windowId, CachedSuggestions cachedSuggestions) {
            this.windowId = windowId;
            this.cachedSuggestions = cachedSuggestions;
        }
    }

    private final GroupSuggestionsService mGroupSuggestionsService;
    private @Nullable WindowedSuggestion mWindowedSuggestion;
    private boolean mWasCachedSuggestionShown;

    public GroupSuggestionsButtonControllerImpl(GroupSuggestionsService groupSuggestionsService) {
        this.mGroupSuggestionsService = groupSuggestionsService;
    }

    @Override
    public boolean shouldShowButton(Tab tab, @WindowId int windowId) {
        if (mWindowedSuggestion != null) {
            clearCachedSuggestion(UserResponse.NOT_SHOWN);
        }

        mWindowedSuggestion = initWindowedSuggestion(windowId);
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
        tabGroupModelFilter.mergeListOfTabsToGroup(
                tabsToGroup,
                tab,
                /* indexInGroup= */ null,
                /* notify= */ MergeNotificationType.NOTIFY_ALWAYS);

        SuggestionMetricsService metricsService =
                SuggestionMetricsServiceFactory.getForProfile(tab.getProfile());
        assert tab.getTabGroupId() != null;
        assert metricsService != null;
        assert mWindowedSuggestion != null;

        metricsService.onSuggestionAccepted(
                mWindowedSuggestion.windowId, CPA_SUGGESTION, tab.getTabGroupId());

        clearCachedSuggestion(UserResponse.ACCEPTED);
    }

    private void clearCachedSuggestion(@UserResponse int userResponse) {
        if (!isSuggestionValid()) {
            return;
        }

        var suggestionsList = getGroupSuggestionList();
        var suggestionId = suggestionsList.isEmpty() ? 0 : suggestionsList.get(0).suggestionId;

        assumeNonNull(mWindowedSuggestion)
                .cachedSuggestions
                .userResponseMetadataCallback
                .onResult(new UserResponseMetadata(suggestionId, userResponse));
        mWindowedSuggestion = null;
        mWasCachedSuggestionShown = false;
    }

    private boolean isSuggestionValid() {
        GroupSuggestions groupSuggestions = getGroupSuggestions(mWindowedSuggestion);
        return groupSuggestions != null && groupSuggestions.groupSuggestions != null;
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
        GroupSuggestions groupSuggestions = getGroupSuggestions(mWindowedSuggestion);
        if (groupSuggestions == null || groupSuggestions.groupSuggestions == null) {
            return List.of();
        }

        return groupSuggestions.groupSuggestions;
    }

    private static @Nullable GroupSuggestions getGroupSuggestions(
            @Nullable WindowedSuggestion windowedSuggestion) {
        if (windowedSuggestion == null) return null;
        return windowedSuggestion.cachedSuggestions.groupSuggestions;
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

    @Override
    public void destroy() {
        if (mWindowedSuggestion == null) return;

        mWindowedSuggestion.cachedSuggestions.userResponseMetadataCallback.destroy();
        mWindowedSuggestion = null;
    }

    private @Nullable WindowedSuggestion initWindowedSuggestion(@WindowId int windowId) {
        CachedSuggestions cachedSuggestions =
                mGroupSuggestionsService.getCachedSuggestions(windowId);
        if (cachedSuggestions == null) return null;

        return new WindowedSuggestion(windowId, cachedSuggestions);
    }
}
