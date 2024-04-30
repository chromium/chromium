// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.ResultStrength;
import org.chromium.chrome.browser.tab_resumption.TabResumptionDataProvider.SuggestionsResult;

import java.util.List;

public class SmartTabResumptionDataProvider extends TabResumptionDataProvider {
    private final TabResumptionBridge mBridge;

    public SmartTabResumptionDataProvider(TabResumptionBridge bridge) {
        mBridge = bridge;
    }

    /** Implements {@link TabResumptionDataProvider} */
    @Override
    public void destroy() {
        mBridge.destroy();
    }

    /** Implements {@link TabResumptionDataProvider} */
    @Override
    public void fetchSuggestions(Callback<SuggestionsResult> suggestionsCallback) {
        // TODO(crbug.com/337858147): Interface with mBridge to get actual suggestions.
        List<SuggestionEntry> suggestions = null;
        suggestionsCallback.onResult(
                new SuggestionsResult(ResultStrength.FORCED_NULL, suggestions));
    }
}
