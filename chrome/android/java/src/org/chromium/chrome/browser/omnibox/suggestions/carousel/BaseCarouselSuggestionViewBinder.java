// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import android.view.View;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.Collection;

/**
 * Binder for the Carousel suggestions.
 */
public final class BaseCarouselSuggestionViewBinder {
    /** @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object) */
    public static void bind(PropertyModel model, BaseCarouselSuggestionView view, PropertyKey key) {
        if (key == BaseCarouselSuggestionViewProperties.TILES) {
            final Collection<ListItem> items =
                    model.get(BaseCarouselSuggestionViewProperties.TILES);
            final SimpleRecyclerViewAdapter adapter = view.getAdapter();
            if (items != null) {
                adapter.getModelList().set(items);
            } else {
                adapter.getModelList().clear();
            }
        } else if (key == BaseCarouselSuggestionViewProperties.TITLE) {
            view.getHeaderTextView().setText(model.get(BaseCarouselSuggestionViewProperties.TITLE));
        } else if (key == BaseCarouselSuggestionViewProperties.SHOW_TITLE) {
            final boolean showTitle = model.get(BaseCarouselSuggestionViewProperties.SHOW_TITLE);
            final View headerView = view.getHeaderView();
            final int verticalPad = view.getResources().getDimensionPixelSize(
                    R.dimen.omnibox_carousel_suggestion_padding);
            if (showTitle) {
                headerView.setVisibility(View.VISIBLE);
                view.setPaddingRelative(0, 0, 0, verticalPad);
            } else {
                headerView.setVisibility(View.GONE);
                view.setPaddingRelative(0, verticalPad, 0, verticalPad);
            }
        }
    }
}
