// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.util.Pair;

import org.chromium.base.StrictModeContext;
import org.chromium.ui.modelutil.MVCListAdapter;

/**
 * Creates and helps set up instances of an appropriate implementation of
 * {@link OmniboxSuggestionsDropdown}.
 * TODO(crbug.com/1075602): remove this class.
 */
class OmniboxSuggestionsDropdownFactory {
    /**
     * Provides a {@link OmniboxSuggestionsDropdown} implementation with a matching adapter.
     * @param context Android context in which provided implementation will work.
     * @param modelList A model list the adapter will work with.
     * @return Implementation of the dropdown and adapter as a pair.
     */
    static Pair<OmniboxSuggestionsDropdown, MVCListAdapter> provideDropdownAndAdapter(
            Context context, MVCListAdapter.ModelList modelList) {
        return provideRecyclerView(context, modelList);
    }

    private static Pair<OmniboxSuggestionsDropdown, MVCListAdapter> provideRecyclerView(
            Context context, MVCListAdapter.ModelList modelList) {
        OmniboxSuggestionsRecyclerView dropdown;
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            dropdown = new OmniboxSuggestionsRecyclerView(context);
        }
        OmniboxSuggestionsRecyclerViewAdapter adapter =
                new OmniboxSuggestionsRecyclerViewAdapter(modelList);
        dropdown.setAdapter(adapter);
        return Pair.create(dropdown, adapter);
    }
}
