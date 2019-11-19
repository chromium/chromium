// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabContext;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestion;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionsObserver;
import org.chromium.chrome.tab_ui.R;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * One of the concrete {@link MessageService} that only serve {@link MessageType.TAB_SUGGESTION}.
 */
public class TabSuggestionMessageService extends MessageService implements TabSuggestionsObserver {
    static final int CLOSE_SUGGESTION_ACTION_ENABLING_THRESHOLD = 1;

    /**
     * This is the data type that this MessageService is serving to its Observer.
     */
    public class TabSuggestionMessageData implements MessageData {
        public final TabSuggestion tabSuggestion;
        public final MessageCardView.ReviewActionProvider reviewActionProvider;
        public final MessageCardView.DismissActionProvider dismissActionProvider;

        public TabSuggestionMessageData(TabSuggestion tabSuggestion,
                MessageCardView.ReviewActionProvider reviewActionProvider,
                MessageCardView.DismissActionProvider dismissActionProvider) {
            this.tabSuggestion = tabSuggestion;
            this.reviewActionProvider = reviewActionProvider;
            this.dismissActionProvider = dismissActionProvider;
        }
    }

    private final Context mContext;
    private final TabModelSelector mTabModelSelector;
    private TabSelectionEditorCoordinator
            .TabSelectionEditorController mTabSelectionEditorController;

    private TabSuggestion mCurrentBestTabSuggestion;

    public TabSuggestionMessageService(Context context, TabModelSelector tabModelSelector,
            TabSelectionEditorCoordinator
                    .TabSelectionEditorController tabSelectionEditorController) {
        super(MessageType.TAB_SUGGESTION);
        mContext = context;
        mTabModelSelector = tabModelSelector;
        mTabSelectionEditorController = tabSelectionEditorController;
    }

    @VisibleForTesting
    void review() {
        assert mCurrentBestTabSuggestion != null;

        mTabSelectionEditorController.configureToolbar(getActionString(mCurrentBestTabSuggestion),
                getActionProvider(mCurrentBestTabSuggestion),
                getEnablingThreshold(mCurrentBestTabSuggestion), null);

        mTabSelectionEditorController.show(
                getTabList(), mCurrentBestTabSuggestion.getTabsInfo().size());
    }

    private String getActionString(TabSuggestion tabSuggestion) {
        switch (tabSuggestion.getAction()) {
            case TabSuggestion.TabSuggestionAction.CLOSE:
                return mContext.getString(R.string.tab_suggestion_close_tab_action_button);
            default:
                assert false;
        }
        return null;
    }

    private int getEnablingThreshold(TabSuggestion tabSuggestion) {
        switch (tabSuggestion.getAction()) {
            case TabSuggestion.TabSuggestionAction.CLOSE:
                return CLOSE_SUGGESTION_ACTION_ENABLING_THRESHOLD;
            default:
                assert false;
        }
        return -1;
    }

    @VisibleForTesting
    TabSelectionEditorActionProvider getActionProvider(TabSuggestion tabSuggestion) {
        switch (tabSuggestion.getAction()) {
            case TabSuggestion.TabSuggestionAction.CLOSE:
                return new TabSelectionEditorActionProvider(mTabSelectionEditorController,
                        TabSelectionEditorActionProvider.TabSelectionEditorAction.CLOSE) {
                    @Override
                    void processSelectedTabs(
                            List<Tab> selectedTabs, TabModelSelector tabModelSelector) {
                        super.processSelectedTabs(selectedTabs, tabModelSelector);
                        // TODO(crbug.com/1023699) : call TabSuggestion component suggestion
                        // acceptance callback.
                    }
                };
            default:
                assert false;
        }

        return null;
    }

    private List<Tab> getTabList() {
        List<Tab> tabs = new ArrayList<>();

        Set<Integer> suggestedTabIds = new HashSet<>();
        List<TabContext.TabInfo> suggestedTabInfo = mCurrentBestTabSuggestion.getTabsInfo();
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

    private void dismiss() {
        // TODO(crbug.com/1023699): run dismiss callback from onNewSuggestion().
    }

    // TabSuggestionObserver implementations.
    @Override
    public void onNewSuggestion(List<TabSuggestion> tabSuggestions) {
        if (tabSuggestions.size() == 0) return;

        mCurrentBestTabSuggestion = tabSuggestions.get(0);
        sendAvailabilityNotification(new TabSuggestionMessageData(
                mCurrentBestTabSuggestion, this::review, this::dismiss));
    }

    @Override
    public void onTabSuggestionInvalidated() {
        mCurrentBestTabSuggestion = null;
        sendInvalidNotification();
    }
}
