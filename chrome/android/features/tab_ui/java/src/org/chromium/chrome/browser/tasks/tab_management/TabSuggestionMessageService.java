// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionFeedback.TabSuggestionResponse.ACCEPTED;
import static org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionFeedback.TabSuggestionResponse.DISMISSED;
import static org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionFeedback.TabSuggestionResponse.NOT_CONSIDERED;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabContext;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestion;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionFeedback;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionsObserver;
import org.chromium.chrome.tab_ui.R;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * One of the concrete {@link MessageService} that only serve {@link MessageType#TAB_SUGGESTION}.
 */
public class TabSuggestionMessageService extends MessageService implements TabSuggestionsObserver {
    static final int CLOSE_SUGGESTION_ACTION_ENABLING_THRESHOLD = 1;
    static final int GROUP_SUGGESTION_ACTION_ENABLING_THRESHOLD = 2;
    private static boolean sSuggestionAvailableForTesting;

    /**
     * This is the data type that this MessageService is serving to its Observer.
     */
    public class TabSuggestionMessageData implements MessageData {
        private final TabSuggestion mTabSuggestion;
        private final Callback<TabSuggestionFeedback> mTabSuggestionFeedback;
        public TabSuggestionMessageData(
                TabSuggestion tabSuggestion, Callback<TabSuggestionFeedback> feedbackCallback) {
            mTabSuggestion = tabSuggestion;
            mTabSuggestionFeedback = feedbackCallback;
        }

        /**
         * @return The suggested tabs count.
         */
        public int getSize() {
            return mTabSuggestion.getTabsInfo().size();
        }

        /**
         * @return The suggested action type.
         */
        @TabSuggestion.TabSuggestionAction
        public int getActionType() {
            return mTabSuggestion.getAction();
        }

        /**
         * @return The {@link MessageCardView.ReviewActionProvider} for the associated
         *         {@link TabSuggestion}.
         */
        public MessageCardView.ReviewActionProvider getReviewActionProvider() {
            return new MessageCardView.ReviewActionProvider() {
                @Override
                public void review() {
                    TabSuggestionMessageService.this.review(mTabSuggestion, mTabSuggestionFeedback);
                }
            };
        }

        /**
         * @return The {@link MessageCardView.DismissActionProvider} for the associated
         *         {@link TabSuggestion}.
         */
        public MessageCardView.DismissActionProvider getDismissActionProvider() {
            return new MessageCardView.DismissActionProvider() {
                @Override
                public void dismiss(int messageType) {
                    TabSuggestionMessageService.this.dismiss(
                            mTabSuggestion, mTabSuggestionFeedback);
                }
            };
        }
    }

    private final Context mContext;
    private final TabModelSelector mTabModelSelector;
    private TabSelectionEditorCoordinator
            .TabSelectionEditorController mTabSelectionEditorController;

    public TabSuggestionMessageService(Context context, TabModelSelector tabModelSelector,
            TabSelectionEditorCoordinator
                    .TabSelectionEditorController tabSelectionEditorController) {
        super(MessageType.TAB_SUGGESTION);
        mContext = context;
        mTabModelSelector = tabModelSelector;
        mTabSelectionEditorController = tabSelectionEditorController;
    }

    @VisibleForTesting
    void review(@NonNull TabSuggestion tabSuggestion,
            @NonNull Callback<TabSuggestionFeedback> feedbackCallback) {
        mTabSelectionEditorController.configureToolbar(getActionString(tabSuggestion),
                getActionButtonContentDescriptionTemplate(tabSuggestion),
                getActionProvider(tabSuggestion, feedbackCallback),
                getEnablingThreshold(tabSuggestion),
                getNavigationProvider(tabSuggestion, feedbackCallback));

        mTabSelectionEditorController.show(getTabListFromSuggestion(tabSuggestion),
                tabSuggestion.getTabsInfo().size(), /*recyclerViewPosition=*/null);
    }

    private String getActionString(TabSuggestion tabSuggestion) {
        switch (tabSuggestion.getAction()) {
            case TabSuggestion.TabSuggestionAction.CLOSE:
                return mContext.getString(R.string.tab_suggestion_close_tab_action_button);
            case TabSuggestion.TabSuggestionAction.GROUP:
                return mContext.getString(R.string.tab_selection_editor_group);
            default:
                assert false;
        }
        return null;
    }

    private int getActionButtonContentDescriptionTemplate(TabSuggestion tabSuggestion) {
        switch (tabSuggestion.getAction()) {
            case TabSuggestion.TabSuggestionAction.CLOSE:
                return R.plurals.accessibility_tab_suggestion_close_tab_action_button;
            case TabSuggestion.TabSuggestionAction.GROUP:
                return R.plurals.accessibility_tab_selection_editor_group_button;
            default:
                assert false;
        }
        return 0;
    }

    private int getEnablingThreshold(TabSuggestion tabSuggestion) {
        switch (tabSuggestion.getAction()) {
            case TabSuggestion.TabSuggestionAction.CLOSE:
                return CLOSE_SUGGESTION_ACTION_ENABLING_THRESHOLD;
            case TabSuggestion.TabSuggestionAction.GROUP:
                return GROUP_SUGGESTION_ACTION_ENABLING_THRESHOLD;
            default:
                assert false;
        }
        return -1;
    }

    @VisibleForTesting
    TabSelectionEditorActionProvider getActionProvider(
            TabSuggestion tabSuggestion, Callback<TabSuggestionFeedback> feedbackCallback) {
        int action;
        switch (tabSuggestion.getAction()) {
            case TabSuggestion.TabSuggestionAction.CLOSE:
                action = TabSelectionEditorActionProvider.TabSelectionEditorAction.CLOSE;
                break;
            case TabSuggestion.TabSuggestionAction.GROUP:
                action = TabSelectionEditorActionProvider.TabSelectionEditorAction.GROUP;
                break;
            default:
                assert false;
                return null;
        }

        return new TabSelectionEditorActionProvider(mTabSelectionEditorController, action) {
            @Override
            void processSelectedTabs(List<Tab> selectedTabs, TabModelSelector tabModelSelector) {
                int totalTabCountBeforeProcess = tabModelSelector.getCurrentModel().getCount();
                List<Integer> selectedTabIds = new ArrayList<>();
                for (int i = 0; i < selectedTabs.size(); i++) {
                    selectedTabIds.add(selectedTabs.get(i).getId());
                }
                accept(selectedTabIds, totalTabCountBeforeProcess, tabSuggestion, feedbackCallback);

                super.processSelectedTabs(selectedTabs, tabModelSelector);
            }
        };
    }

    @VisibleForTesting
    TabSelectionEditorCoordinator.TabSelectionEditorNavigationProvider getNavigationProvider(
            TabSuggestion tabSuggestion,
            @NonNull Callback<TabSuggestionFeedback> feedbackCallback) {
        return new TabSelectionEditorCoordinator.TabSelectionEditorNavigationProvider(
                mContext, mTabSelectionEditorController) {
            @Override
            public void goBack() {
                super.goBack();

                feedbackCallback.onResult(
                        new TabSuggestionFeedback(tabSuggestion, DISMISSED, null, 0));
            }
        };
    }

    private List<Tab> getTabListFromSuggestion(TabSuggestion tabSuggestion) {
        List<Tab> tabs = new ArrayList<>();

        Set<Integer> suggestedTabIds = new HashSet<>();
        List<TabContext.TabInfo> suggestedTabInfo = tabSuggestion.getTabsInfo();
        for (int i = 0; i < suggestedTabInfo.size(); i++) {
            suggestedTabIds.add(suggestedTabInfo.get(i).id);
            tabs.add(mTabModelSelector.getTabById(suggestedTabInfo.get(i).id));
        }

        tabs.addAll(getNonSuggestedTabs(suggestedTabIds));
        return tabs;
    }

    private List<Tab> getNonSuggestedTabs(Set<Integer> suggestedTabIds) {
        List<Tab> tabs = new ArrayList<>();
        TabModelFilter tabModelFilter =
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
        List<Tab> filteredTab = tabModelFilter.getTabsWithNoOtherRelatedTabs();

        for (int i = 0; i < filteredTab.size(); i++) {
            Tab tab = filteredTab.get(i);
            if (!suggestedTabIds.contains(tab.getId())) tabs.add(tab);
        }
        return tabs;
    }

    @VisibleForTesting
    public void dismiss(@NonNull TabSuggestion tabSuggestion,
            @NonNull Callback<TabSuggestionFeedback> feedbackCallback) {
        feedbackCallback.onResult(
                new TabSuggestionFeedback(tabSuggestion, NOT_CONSIDERED, null, 0));
    }

    private void accept(List<Integer> selectedTabIds, int totalTabCount,
            @NonNull TabSuggestion tabSuggestion,
            @NonNull Callback<TabSuggestionFeedback> feedbackCallback) {
        feedbackCallback.onResult(
                new TabSuggestionFeedback(tabSuggestion, ACCEPTED, selectedTabIds, totalTabCount));
    }

    // TabSuggestionObserver implementations.
    @Override
    public void onNewSuggestion(List<TabSuggestion> tabSuggestions,
            Callback<TabSuggestionFeedback> tabSuggestionFeedback) {
        if (tabSuggestions.size() == 0) return;

        assert tabSuggestionFeedback != null;

        sSuggestionAvailableForTesting = true;
        for (TabSuggestion tabSuggestion : tabSuggestions) {
            sendAvailabilityNotification(
                    new TabSuggestionMessageData(tabSuggestion, tabSuggestionFeedback));
        }
    }

    @Override
    public void onTabSuggestionInvalidated() {
        sSuggestionAvailableForTesting = false;
        sendInvalidNotification();
    }

    @VisibleForTesting
    public static boolean isSuggestionAvailableForTesting() {
        return sSuggestionAvailableForTesting;
    }
}
