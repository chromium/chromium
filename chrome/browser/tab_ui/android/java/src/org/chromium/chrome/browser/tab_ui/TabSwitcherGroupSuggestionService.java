// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_suggestion.GroupSuggestionsServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.components.visited_url_ranking.url_grouping.CachedSuggestions;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestion;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestions;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestionsService;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponse;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponseMetadata;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Orchestrates fetching and showing tab group suggestions in the Tab Switcher. */
@NullMarked
public class TabSwitcherGroupSuggestionService {
    private static final int NUM_TABS_IN_FORCED_SUGGESTION = 3;

    /** Observes lifecycle events for tab group suggestions. */
    public interface SuggestionLifecycleObserver {
        /** Called when the user accepts the suggestion. */
        default void onSuggestionAccepted() {}

        /** Called when the user explicitly dismisses the suggestion. */
        default void onSuggestionDismissed() {}

        /** Called when the suggestion is ignored. */
        default void onSuggestionIgnored() {}

        /** Called when any response is received. */
        default void onAnySuggestionResponse() {}

        /**
         * Called when a suggestion is shown.
         *
         * @param tabIds The tab IDs included in the group suggestion.
         */
        default void onShowSuggestion(List<@TabId Integer> tabIds) {}
    }

    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void didMoveTab(Tab tab, int newIndex, int curIndex) {
                    clearSuggestions();
                }

                @Override
                public void tabClosureUndone(Tab tab) {
                    clearSuggestions();
                }

                @Override
                public void tabRemoved(Tab tab) {
                    clearSuggestions();
                }

                @Override
                public void willCloseTab(Tab tab, boolean didCloseAlone) {
                    clearSuggestions();
                }

                @Override
                public void willAddTab(Tab tab, @TabLaunchType int type) {
                    clearSuggestions();
                }

                @Override
                public void tabPendingClosure(Tab tab, @TabClosingSource int closingSource) {
                    clearSuggestions();
                }
            };

    private final TabGroupModelFilterObserver mTabGroupModelFilterObserver =
            new TabGroupModelFilterObserver() {
                @Override
                public void willMergeTabToGroup(
                        Tab movedTab, int newRootId, @Nullable Token tabGroupId) {
                    clearSuggestions();
                }

                @Override
                public void willMoveTabGroup(int tabModelOldIndex, int tabModelNewIndex) {
                    clearSuggestions();
                }

                @Override
                public void willMoveTabOutOfGroup(
                        Tab movedTab, @Nullable Token destinationTabGroupId) {
                    clearSuggestions();
                }

                @Override
                public void didCreateGroup(
                        List<Tab> tabs,
                        List<Integer> tabOriginalIndex,
                        List<Integer> tabOriginalRootId,
                        List<Token> tabOriginalTabGroupId,
                        @Nullable String destinationGroupTitle,
                        int destinationGroupColorId,
                        boolean destinationGroupTitleCollapsed) {
                    clearSuggestions();
                }

                @Override
                public void didRemoveTabGroup(
                        int oldRootId,
                        @Nullable Token oldTabGroupId,
                        @DidRemoveTabGroupReason int removalReason) {
                    clearSuggestions();
                }

                @Override
                public void willCloseTabGroup(Token tabGroupId, boolean isHiding) {
                    clearSuggestions();
                }
            };
    private final @WindowId int mWindowId;
    private final ObservableSupplier<TabGroupModelFilter> mCurrentTabGroupModelFilterSupplier;
    private final SuggestionLifecycleObserver mSuggestionLifecycleObserver;
    private final GroupSuggestionsService mGroupSuggestionsService;
    private final ValueChangedCallback<TabGroupModelFilter> mOnTabGroupModelFilterChanged =
            new ValueChangedCallback<>(this::onTabGroupModelFilterChanged);
    private @Nullable SuggestionLifecycleObserverHandler mSuggestionLifecycleObserverHandler;

    /**
     * @param windowId The ID of the current window.
     * @param currentTabGroupModelFilterSupplier The supplier for the current {@link
     *     TabGroupModelFilter}.
     * @param profile The profile used for tab group suggestions.
     * @param suggestionLifecycleObserver Listens for user responses to a group suggestion.
     */
    public TabSwitcherGroupSuggestionService(
            @WindowId int windowId,
            ObservableSupplier<TabGroupModelFilter> currentTabGroupModelFilterSupplier,
            Profile profile,
            SuggestionLifecycleObserver suggestionLifecycleObserver) {
        mWindowId = windowId;
        mCurrentTabGroupModelFilterSupplier = currentTabGroupModelFilterSupplier;
        mSuggestionLifecycleObserver = suggestionLifecycleObserver;

        mGroupSuggestionsService = GroupSuggestionsServiceFactory.getForProfile(profile);

        mOnTabGroupModelFilterChanged.onResult(
                assumeNonNull(
                        mCurrentTabGroupModelFilterSupplier.addObserver(
                                mOnTabGroupModelFilterChanged)));
    }

    public void destroy() {
        mCurrentTabGroupModelFilterSupplier.removeObserver(mOnTabGroupModelFilterChanged);
    }

    private void onTabGroupModelFilterChanged(
            @Nullable TabGroupModelFilter newFilter, @Nullable TabGroupModelFilter oldFilter) {
        if (oldFilter != null) {
            oldFilter.removeObserver(mTabModelObserver);
            oldFilter.removeTabGroupObserver(mTabGroupModelFilterObserver);
        }

        if (newFilter != null) {
            newFilter.addObserver(mTabModelObserver);
            newFilter.addTabGroupObserver(mTabGroupModelFilterObserver);
        }
    }

    /** Shows tab group suggestions if needed. */
    public void maybeShowSuggestions() {
        clearSuggestions();

        CachedSuggestions cachedSuggestions =
                mGroupSuggestionsService.getCachedSuggestions(mWindowId);

        if (cachedSuggestions == null) return;
        GroupSuggestions groupSuggestions = cachedSuggestions.groupSuggestions;

        if (groupSuggestions == null
                || groupSuggestions.groupSuggestions == null
                || groupSuggestions.groupSuggestions.isEmpty()) {
            return;
        }

        Callback<UserResponseMetadata> userResponseCallback =
                cachedSuggestions.userResponseMetadataCallback;

        List<GroupSuggestion> groupSuggestionsList = groupSuggestions.groupSuggestions;

        // Mark all suggestions except the first one as "not shown".
        for (int i = 1; i < groupSuggestionsList.size(); i++) {
            GroupSuggestion groupSuggestion = groupSuggestionsList.get(i);
            userResponseCallback.onResult(
                    new UserResponseMetadata(groupSuggestion.suggestionId, UserResponse.NOT_SHOWN));
        }
        showSuggestion(groupSuggestionsList.get(0), userResponseCallback);
    }

    /** Clears tab group suggestions if present. */
    public void clearSuggestions() {
        if (mSuggestionLifecycleObserverHandler != null) {
            mSuggestionLifecycleObserverHandler.onSuggestionIgnored();
            mSuggestionLifecycleObserverHandler = null;
        }
    }

    /** Forces a tab group suggestion for testing purposes. */
    public void forceTabGroupSuggestion() {
        assert ChromeFeatureList.sTabSwitcherGroupSuggestionsTestModeAndroid.isEnabled()
                : "Forcing suggestions is only allowed in test mode.";

        TabModel tabModel = mCurrentTabGroupModelFilterSupplier.get().getTabModel();
        List<Integer> tabIds = new ArrayList<>();

        // Collect the bottom-most tabs that are not already in a group.
        for (int i = tabModel.getCount() - 1;
                i >= 0 && tabIds.size() < NUM_TABS_IN_FORCED_SUGGESTION;
                i--) {
            Tab tab = tabModel.getTabAtChecked(i);
            if (tab.getTabGroupId() == null) {
                tabIds.add(tab.getId());
            }
        }

        int[] tabIdsArray = new int[tabIds.size()];
        for (int i = 0; i < tabIds.size(); i++) {
            tabIdsArray[i] = tabIds.get(i);
        }

        GroupSuggestion groupSuggestion =
                new GroupSuggestion(
                        tabIdsArray,
                        /* suggestionId= */ 1,
                        /* suggestionReason= */ 1,
                        /* suggestedName= */ "",
                        /* promoHeader= */ "",
                        /* promoContents= */ "");
        showSuggestion(groupSuggestion, ignored -> {});
    }

    /**
     * Shows a single tab group suggestion to the user.
     *
     * @param suggestion The suggestion to show.
     * @param callback The callback to invoke with the user's response.
     */
    private void showSuggestion(
            GroupSuggestion suggestion, Callback<UserResponseMetadata> callback) {
        Set<Integer> suggestionTabIds = new HashSet<>();
        for (int tabId : suggestion.tabIds) {
            suggestionTabIds.add(tabId);
        }

        mSuggestionLifecycleObserverHandler =
                new SuggestionLifecycleObserverHandler(
                        suggestion.suggestionId, callback, mSuggestionLifecycleObserver);
        mSuggestionLifecycleObserverHandler.onShowSuggestion(new ArrayList<>(suggestionTabIds));
    }
}
