// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Baseline method for identifying Tab candidates which the user may want
 * to close or group due to lack of engagement. It returns all Tabs. This is
 * used for testing code upstream from the provider - not the provider
 * itself. This provider is very deterministic and creates very stable
 * instrumentation tests and tests for {@link TabSuggestionsOrchestrator}
 * and {@link TabSuggestionsClientFetcher}
 */
public class BaselineTabSuggestionProvider implements TabSuggestionProvider {
    // Default action is TabSuggestion.TabSuggestionAction.CLOSE;
    private final @TabSuggestion.TabSuggestionAction int mAction;

    BaselineTabSuggestionProvider() {
        this(TabSuggestion.TabSuggestionAction.CLOSE);
    }

    BaselineTabSuggestionProvider(@TabSuggestion.TabSuggestionAction int action) {
        mAction = action;
    }

    @Override
    public List<TabSuggestion> suggest(TabContext tabContext) {
        if (tabContext == null || tabContext.getUngroupedTabs() == null
                || tabContext.getUngroupedTabs().size() < 1) {
            return null;
        }
        List<TabContext.TabInfo> tabs = new ArrayList<>();
        tabs.addAll(tabContext.getUngroupedTabs());
        return Arrays.asList(new TabSuggestion(tabs, mAction, getSuggestionProvider()));
    }

    private String getSuggestionProvider() {
        switch (mAction) {
            case TabSuggestion.TabSuggestionAction.CLOSE:
                return TabSuggestionsRanker.SuggestionProviders.STALE_TABS_SUGGESTION_PROVIDER;
        }
        return "";
    }
}
