// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static java.util.Comparator.comparingInt;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.metrics.RecordHistogram;
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

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Orchestrates fetching and showing tab group suggestions in the Tab Switcher. */
@NullMarked
public class TabSwitcherGroupSuggestionService {
    /* Tab gaps equal to this or beyond will not be permitted to be shown. */
    private static final int TAB_GAP_LIMIT = 2;
    private static final int NUM_TABS_IN_FORCED_SUGGESTION = 14;
    private static final String SUGGESTION_UI_HISTOGRAM_NAME =
            "GroupSuggestionsService.SuggestionUiEvent";

    /**
     * Events related to the group suggestion service UI.
     *
     * <p>These values are persisted in histograms. See "SuggestionUiEvent" in
     * src/tools/metrics/histograms/metadata/visited_url_ranking/enums.xml.
     */
    @IntDef({
        SuggestionUiEvent.UNKNOWN,
        SuggestionUiEvent.TAB_SWITCHER_OPENED,
        SuggestionUiEvent.REQUEST_STARTED,
        SuggestionUiEvent.REQUEST_NO_RESULT,
        SuggestionUiEvent.REQUEST_HAS_SUGGESTION,
        SuggestionUiEvent.REQUEST_HAS_MULTIPLE_SUGGESTIONS,
        SuggestionUiEvent.INVALIDATED_DUE_TO_GAP,
        SuggestionUiEvent.INVALIDATED_DUE_TO_NO_SELECTED_TAB,
        SuggestionUiEvent.INVALIDATED_DUE_TO_TAB_STATE,
        SuggestionUiEvent.INVALIDATED_DUE_TO_EMPTY_SUGGESTION,
        SuggestionUiEvent.INVALIDATED_DUE_TO_PINNED_TAB,
        SuggestionUiEvent.SHOWN,
        SuggestionUiEvent.IGNORED,
        SuggestionUiEvent.REJECTED,
        SuggestionUiEvent.ACCEPTED,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface SuggestionUiEvent {
        int UNKNOWN = 0;

        /** The user opened the tab switcher. */
        int TAB_SWITCHER_OPENED = 1;

        /** A request for suggestions was started. */
        int REQUEST_STARTED = 2;

        /** The request for suggestions yielded no results. */
        int REQUEST_NO_RESULT = 3;

        /** The request for suggestions yielded a single suggestion. */
        int REQUEST_HAS_SUGGESTION = 4;

        /** The request for suggestions yielded multiple suggestions. */
        int REQUEST_HAS_MULTIPLE_SUGGESTIONS = 5;

        /** The suggestion was invalidated due to a gap between tabs in the suggestion. */
        int INVALIDATED_DUE_TO_GAP = 6;

        /**
         * The suggestion was invalidated due to the selected tab not being in the group suggestion.
         */
        int INVALIDATED_DUE_TO_NO_SELECTED_TAB = 7;

        /**
         * The suggestion was invalidated due to a tab in the suggestion being frozen, closing, or
         * the tab ID not being present in the UI model.
         */
        int INVALIDATED_DUE_TO_TAB_STATE = 8;

        /** The suggestion was invalidated due to having null or empty fields. */
        int INVALIDATED_DUE_TO_EMPTY_SUGGESTION = 9;

        /** The suggestion was invalidated due to a pinned tab being included. */
        int INVALIDATED_DUE_TO_PINNED_TAB = 10;

        /** The suggestion was shown to the user. */
        int SHOWN = 11;

        /** The user ignored the suggestion. */
        int IGNORED = 12;

        /** The user rejected the suggestion. */
        int REJECTED = 13;

        /** The user accepted the suggestion. */
        int ACCEPTED = 14;

        int MAX_VALUE = ACCEPTED;
    }

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
         * @param tabIdsSortedByIndex The tab IDs included in the group suggestion, sorted by tab
         *     index.
         */
        default void onShowSuggestion(List<@TabId Integer> tabIdsSortedByIndex) {}
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
                public void onTabClosePending(
                        List<Tab> tabs, boolean isAllTabs, @TabClosingSource int closingSource) {
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
                public void willMoveTabGroup(Token tabGroupId, int currentIndex) {
                    clearSuggestions();
                }

                @Override
                public void willMoveTabOutOfGroup(
                        Tab movedTab, @Nullable Token destinationTabGroupId) {
                    clearSuggestions();
                }

                @Override
                public void didCreateNewGroup(Tab destinationTab, TabGroupModelFilter filter) {
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
    private final ObservableSupplier<@Nullable TabGroupModelFilter>
            mCurrentTabGroupModelFilterSupplier;
    private final SuggestionLifecycleObserverHandler mSuggestionLifecycleObserverHandler;
    private final GroupSuggestionsService mGroupSuggestionsService;
    private final Callback<@Nullable TabGroupModelFilter> mOnTabGroupModelFilterChanged =
            new ValueChangedCallback<>(this::onTabGroupModelFilterChanged);

    /**
     * @param windowId The ID of the current window.
     * @param currentTabGroupModelFilterSupplier The supplier for the current {@link
     *     TabGroupModelFilter}.
     * @param profile The profile used for tab group suggestions.
     * @param suggestionLifecycleObserverHandler Listens for user responses to a group suggestion.
     */
    public TabSwitcherGroupSuggestionService(
            @WindowId int windowId,
            ObservableSupplier<@Nullable TabGroupModelFilter> currentTabGroupModelFilterSupplier,
            Profile profile,
            SuggestionLifecycleObserverHandler suggestionLifecycleObserverHandler) {
        mWindowId = windowId;
        mCurrentTabGroupModelFilterSupplier = currentTabGroupModelFilterSupplier;
        mSuggestionLifecycleObserverHandler = suggestionLifecycleObserverHandler;

        mGroupSuggestionsService = GroupSuggestionsServiceFactory.getForProfile(profile);

        mCurrentTabGroupModelFilterSupplier.addSyncObserverAndCallIfNonNull(
                mOnTabGroupModelFilterChanged);
    }

    public void destroy() {
        mCurrentTabGroupModelFilterSupplier.removeObserver(mOnTabGroupModelFilterChanged);
        mSuggestionLifecycleObserverHandler.onSuggestionIgnored();
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
        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        clearSuggestions();
        if (filter == null || isIncognitoMode(filter)) return;

        recordGroupSuggestionHistogram(SuggestionUiEvent.REQUEST_STARTED);
        CachedSuggestions cachedSuggestions =
                mGroupSuggestionsService.getCachedSuggestions(mWindowId);

        if (cachedSuggestions == null) {
            recordGroupSuggestionHistogram(SuggestionUiEvent.REQUEST_NO_RESULT);
            return;
        }
        GroupSuggestions groupSuggestions = cachedSuggestions.groupSuggestions;

        if (groupSuggestions == null
                || groupSuggestions.groupSuggestions == null
                || groupSuggestions.groupSuggestions.isEmpty()) {
            recordGroupSuggestionHistogram(SuggestionUiEvent.INVALIDATED_DUE_TO_EMPTY_SUGGESTION);
            return;
        }

        Callback<UserResponseMetadata> userResponseCallback =
                cachedSuggestions.userResponseMetadataCallback;

        List<GroupSuggestion> groupSuggestionsList = groupSuggestions.groupSuggestions;
        if (groupSuggestionsList.size() == 1) {
            recordGroupSuggestionHistogram(SuggestionUiEvent.REQUEST_HAS_SUGGESTION);
        } else {
            recordGroupSuggestionHistogram(SuggestionUiEvent.REQUEST_HAS_MULTIPLE_SUGGESTIONS);
        }

        GroupSuggestion suggestion = groupSuggestionsList.get(0);

        TabModel tabModel = filter.getTabModel();
        Map<@TabId Integer, Integer> tabIdsToIndices = getTabIdToIndicesMap(tabModel);
        List<Tab> tabsSortedByIndex = getTabsSortedByIndex(tabModel, tabIdsToIndices, suggestion);

        if (tabsSortedByIndex == null || !canShowSuggestion(tabIdsToIndices, tabsSortedByIndex)) {
            userResponseCallback.onResult(
                    new UserResponseMetadata(suggestion.suggestionId, UserResponse.NOT_SHOWN));
            return;
        }

        List<@TabId Integer> tabIdsSortedByIndex = new ArrayList<>();
        for (Tab tab : tabsSortedByIndex) {
            tabIdsSortedByIndex.add(tab.getId());
        }

        showSuggestion(suggestion, tabIdsSortedByIndex, userResponseCallback);
    }

    private static Map<@TabId Integer, Integer> getTabIdToIndicesMap(TabModel tabModel) {
        Map<@TabId Integer, Integer> tabIdsToIndices = new HashMap<>();
        int index = 0;
        for (Tab tab : tabModel) {
            assert tab != null;
            tabIdsToIndices.put(tab.getId(), index);
            index++;
        }
        return tabIdsToIndices;
    }

    private @Nullable List<Tab> getTabsSortedByIndex(
            TabModel tabModel,
            Map<@TabId Integer, Integer> tabIdsToIndices,
            GroupSuggestion suggestion) {
        List<Tab> tabs = new ArrayList<>();
        boolean isAnyTabSelected = false;
        for (@TabId int tabId : suggestion.tabIds) {
            Tab tab = tabModel.getTabById(tabId);
            if (tab == null
                    || tab.isFrozen()
                    || tab.isClosing()
                    || !tabIdsToIndices.containsKey(tabId)) {
                recordGroupSuggestionHistogram(SuggestionUiEvent.INVALIDATED_DUE_TO_TAB_STATE);
                return null;
            }
            tabs.add(tab);
            isAnyTabSelected |= tab.isActivated();
        }
        if (!isAnyTabSelected) {
            recordGroupSuggestionHistogram(SuggestionUiEvent.INVALIDATED_DUE_TO_NO_SELECTED_TAB);
            return null;
        }

        tabs.sort(comparingInt(tab -> tabIdsToIndices.get(tab.getId())));
        return tabs;
    }

    private boolean canShowSuggestion(
            Map<@TabId Integer, Integer> tabIdsToIndices, List<Tab> tabsSortedByIndex) {
        int prevIndex = TabModel.INVALID_TAB_INDEX;
        for (Tab tab : tabsSortedByIndex) {
            if (tab.getIsPinned()) {
                recordGroupSuggestionHistogram(SuggestionUiEvent.INVALIDATED_DUE_TO_PINNED_TAB);
                return false;
            }

            @TabId int tabId = tab.getId();

            assert tabIdsToIndices.containsKey(tabId);
            int currIndex = tabIdsToIndices.get(tabId);

            // No gap of over 1 tab in length is allowed.
            if (prevIndex != TabModel.INVALID_TAB_INDEX && currIndex > prevIndex + TAB_GAP_LIMIT) {
                recordGroupSuggestionHistogram(SuggestionUiEvent.INVALIDATED_DUE_TO_GAP);
                return false;
            }

            if (prevIndex == TabModel.INVALID_TAB_INDEX || currIndex > prevIndex) {
                prevIndex = currIndex;
            }
        }
        return true;
    }

    private boolean isIncognitoMode(TabGroupModelFilter filter) {
        return filter.getTabModel().isIncognitoBranded();
    }

    /** Clears tab group suggestions if present. */
    public void clearSuggestions() {
        mSuggestionLifecycleObserverHandler.onSuggestionIgnored();
    }

    /** Records a histogram for a {@link SuggestionUiEvent}. */
    public static void recordGroupSuggestionHistogram(@SuggestionUiEvent int suggestionUiEvent) {
        RecordHistogram.recordEnumeratedHistogram(
                SUGGESTION_UI_HISTOGRAM_NAME, suggestionUiEvent, SuggestionUiEvent.MAX_VALUE);
    }

    /** Forces a tab group suggestion for testing purposes. */
    public void forceTabGroupSuggestion() {
        assert ChromeFeatureList.sTabSwitcherGroupSuggestionsTestModeAndroid.isEnabled()
                : "Forcing suggestions is only allowed in test mode.";

        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        clearSuggestions();
        if (filter == null) return;

        TabModel tabModel = filter.getTabModel();
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

        // To order it by index, reverse the list.
        Collections.reverse(tabIds);

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
        showSuggestion(groupSuggestion, tabIds, ignored -> {});
    }

    /**
     * Shows a single tab group suggestion to the user.
     *
     * @param suggestion The suggestion to show.
     * @param tabIdsSortedByIndex The tabs ordered by index.
     * @param callback The callback to invoke with the user's response.
     */
    private void showSuggestion(
            GroupSuggestion suggestion,
            List<@TabId Integer> tabIdsSortedByIndex,
            Callback<UserResponseMetadata> callback) {
        mSuggestionLifecycleObserverHandler.updateSuggestionDetails(
                suggestion.suggestionId, callback);
        mSuggestionLifecycleObserverHandler.onShowSuggestion(tabIdsSortedByIndex);
    }
}
