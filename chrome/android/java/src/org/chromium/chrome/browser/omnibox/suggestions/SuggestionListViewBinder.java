// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Handles property updates to the suggestion list component.
 */
class SuggestionListViewBinder {
    /** Holds the view components needed to renderer the suggestion list. */
    public static class SuggestionListViewHolder {
        public final ViewGroup container;
        public final OmniboxSuggestionsDropdown dropdown;

        public SuggestionListViewHolder(ViewGroup container, OmniboxSuggestionsDropdown dropdown) {
            this.container = container;
            this.dropdown = dropdown;
        }
    }

    /**
     * @see
     * PropertyModelChangeProcessor.ViewBinder#bind(Object,
     * Object, Object)
     */
    public static void bind(
            PropertyModel model, SuggestionListViewHolder view, PropertyKey propertyKey) {
        if (SuggestionListProperties.VISIBLE.equals(propertyKey)) {
            boolean visible = model.get(SuggestionListProperties.VISIBLE);
            // Actual View showing the dropdown.
            View dropdownView = view.dropdown.getViewGroup();
            if (visible) {
                view.container.setVisibility(View.VISIBLE);
                if (dropdownView.getParent() == null) view.container.addView(dropdownView);
                view.dropdown.show();
            } else {
                view.dropdown.hide();
                UiUtils.removeViewFromParent(dropdownView);
                view.container.setVisibility(View.INVISIBLE);
            }
        } else if (SuggestionListProperties.EMBEDDER.equals(propertyKey)) {
            view.dropdown.setEmbedder(model.get(SuggestionListProperties.EMBEDDER));
        } else if (SuggestionListProperties.OBSERVER.equals(propertyKey)) {
            view.dropdown.setObserver(model.get(SuggestionListProperties.OBSERVER));
        } else if (SuggestionListProperties.SUGGESTION_MODELS.equals(propertyKey)) {
            // This should only ever be bound once.
            model.get(SuggestionListProperties.SUGGESTION_MODELS)
                    .addObserver(new ListObservable.ListObserver<Void>() {
                        @Override
                        public void onItemRangeChanged(ListObservable<Void> source, int index,
                                int count, @Nullable Void payload) {
                            view.dropdown.resetSelection();
                        }
                    });
        } else if (SuggestionListProperties.IS_INCOGNITO.equals(propertyKey)) {
            view.dropdown.refreshPopupBackground(model.get(SuggestionListProperties.IS_INCOGNITO));
        }
    }
}
