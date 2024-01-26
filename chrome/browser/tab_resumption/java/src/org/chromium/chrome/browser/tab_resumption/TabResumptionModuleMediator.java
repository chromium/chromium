// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.text.TextUtils;

import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** The Mediator for the tab resumption module. */
public class TabResumptionModuleMediator {
    private static final int MAX_TILES_NUMBER = 2;

    private final PropertyModel mModel;

    public TabResumptionModuleMediator(PropertyModel model) {
        mModel = model;
    }

    void destroy() {}

    /** Returns the current time in ms since the epoch. */
    long getCurrentTimeMs() {
        return System.currentTimeMillis();
    }

    /**
     * @return Whether the given suggestion is qualified to be shown in UI.
     */
    private static boolean isSuggestionValid(SuggestionEntry entry) {
        return !TextUtils.isEmpty(entry.title);
    }

    /**
     * Filters `suggestions` to choose up to MAX_TILES_NUMBER top ones, the returns the data in a
     * new SuggestionBundle instance.
     *
     * @param suggestions Retrieved suggestions with basic filtering, from most recent to least.
     */
    private SuggestionBundle makeSuggestionBundle(List<SuggestionEntry> suggestions) {
        long currentTimeMs = getCurrentTimeMs();
        SuggestionBundle bundle = new SuggestionBundle(currentTimeMs);
        for (SuggestionEntry entry : suggestions) {
            if (isSuggestionValid(entry)) {
                bundle.entries.add(entry);
                if (bundle.entries.size() >= MAX_TILES_NUMBER) {
                    break;
                }
            }
        }

        return bundle;
    }

    /** Fetches new suggestions, creates SuggestionBundle, then updates `mModel`. */
    public void reload() {
        TabResumptionDataProvider dataProvider =
                (TabResumptionDataProvider) mModel.get(TabResumptionModuleProperties.DATA_PROVIDER);
        assert dataProvider != null;

        dataProvider.fetchSuggestions(
                (List<SuggestionEntry> suggestions) -> {
                    SuggestionBundle bundle = null;
                    if (suggestions != null) {
                        if (suggestions.size() == 0) {
                            // TODO(crbug.com/1515325): Record metrics here.
                        } else {
                            bundle = makeSuggestionBundle(suggestions);
                        }
                    }
                    mModel.set(
                            TabResumptionModuleProperties.SUGGESTION_BUNDLE,
                            bundle); // Triggers render.
                    mModel.set(TabResumptionModuleProperties.IS_VISIBLE, bundle != null);
                    // TODO(crbug.com/1515325): Record metrics here if `bundle != null`.
                });
    }
}
