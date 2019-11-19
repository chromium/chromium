// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import androidx.annotation.StringDef;

import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * Ranker which ranks all suggestions based on static rules.
 */
public final class TabSuggestionsRanker {
    /**
     * List of all known providers (server & client).
     * TODO(crbug.com/970933) decouple server providers from client.
     */
    @StringDef({SuggestionProviders.STALE_TABS_SUGGESTION_PROVIDER})
    public @interface SuggestionProviders {
        String STALE_TABS_SUGGESTION_PROVIDER = "StaleTabSuggestionProvider";
    }

    /**
     * Ranks suggestions based on the number of tabs first and the score of the provider in case of
     * a tie. This logic is subject to change in the future.
     * @param suggestions to be ranked
     * @param providerConfigs per-provider configurations
     * @return sorted suggestions list where first suggestion in the list is the most preferred
     */
    public static List<TabSuggestion> getRankedSuggestions(List<TabSuggestion> suggestions,
            Map<String, TabSuggestionProviderConfiguration> providerConfigs) {
        if (suggestions.isEmpty()) {
            return suggestions;
        }

        Collections.sort(suggestions, (a, b) -> {
            if (a == b) return 0;

            if (a.getTabsInfo().size() == b.getTabsInfo().size()) {
                int aScore = providerConfigs.containsKey(a.getProviderName())
                        ? providerConfigs.get(a.getProviderName()).score
                        : 0;
                int bScore = providerConfigs.containsKey(b.getProviderName())
                        ? providerConfigs.get(b.getProviderName()).score
                        : 0;

                return Integer.compare(bScore, aScore);
            }

            return Integer.compare(b.getTabsInfo().size(), a.getTabsInfo().size());
        });

        return suggestions;
    }
}
