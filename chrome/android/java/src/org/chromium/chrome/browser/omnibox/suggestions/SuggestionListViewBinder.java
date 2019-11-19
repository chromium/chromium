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
    /**
     * Holds the view components needed to renderer the suggestion list.
     */
    public static class SuggestionListViewHolder {
        public final ViewGroup container;
        public final OmniboxSuggestionsList listView;

        public SuggestionListViewHolder(ViewGroup container, OmniboxSuggestionsList list) {
            this.container = container;
            this.listView = list;
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
            if (visible) {
                view.container.setVisibility(View.VISIBLE);
                if (view.listView.getParent() == null) view.container.addView(view.listView);
                view.listView.show();
            } else {
                view.listView.setVisibility(View.GONE);
                UiUtils.removeViewFromParent(view.listView);
                view.container.setVisibility(View.INVISIBLE);
            }
        } else if (SuggestionListProperties.EMBEDDER.equals(propertyKey)) {
            view.listView.setEmbedder(model.get(SuggestionListProperties.EMBEDDER));
        } else if (SuggestionListProperties.SUGGESTION_MODELS.equals(propertyKey)) {
            // This should only ever be bound once.
            model.get(SuggestionListProperties.SUGGESTION_MODELS)
                    .addObserver(new ListObservable.ListObserver<Void>() {
                        @Override
                        public void onItemRangeChanged(ListObservable<Void> source, int index,
                                int count, @Nullable Void payload) {
                            view.listView.setSelection(0);
                        }
                    });
        } else if (SuggestionListProperties.IS_INCOGNITO.equals(propertyKey)) {
            view.listView.refreshPopupBackground(model.get(SuggestionListProperties.IS_INCOGNITO));
        }
    }
}
