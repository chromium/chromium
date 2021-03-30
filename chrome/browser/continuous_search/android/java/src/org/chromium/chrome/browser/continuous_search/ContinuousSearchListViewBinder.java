// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.graphics.PorterDuff.Mode;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * Responsible for binding the {@link PropertyModel} for a search result item to a View.
 */
class ContinuousSearchListViewBinder {
    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (ContinuousSearchListProperties.LABEL == propertyKey) {
            TextView textView = view.findViewById(R.id.continuous_search_list_item_text);
            textView.setText(model.get(ContinuousSearchListProperties.LABEL));
        } else if (ContinuousSearchListProperties.URL == propertyKey) {
            GURL url = model.get(ContinuousSearchListProperties.URL);
            TextView textView = view.findViewById(R.id.continuous_search_list_item_description);
            if (textView == null) return;

            String domain = "";
            if (url != null) {
                domain = UrlUtilities.getDomainAndRegistry(url.getSpec(), true);
            }
            textView.setText(domain);
        } else if (ContinuousSearchListProperties.IS_SELECTED == propertyKey) {
            view.setSelected(model.get(ContinuousSearchListProperties.IS_SELECTED));
        } else if (ContinuousSearchListProperties.CLICK_LISTENER == propertyKey) {
            view.setOnClickListener(model.get(ContinuousSearchListProperties.CLICK_LISTENER));
        } else if (ContinuousSearchListProperties.BACKGROUND_COLOR == propertyKey) {
            if (view.getBackground() != null) {
                view.getBackground().setColorFilter(
                        model.get(ContinuousSearchListProperties.BACKGROUND_COLOR), Mode.SRC_IN);
            }
        } else if (ContinuousSearchListProperties.TITLE_TEXT_STYLE == propertyKey) {
            TextView textTitle = view.findViewById(R.id.continuous_search_list_item_text);
            if (textTitle != null) {
                ApiCompatibilityUtils.setTextAppearance(
                        textTitle, model.get(ContinuousSearchListProperties.TITLE_TEXT_STYLE));
            }
        } else if (ContinuousSearchListProperties.DESCRIPTION_TEXT_STYLE == propertyKey) {
            TextView textDescription =
                    view.findViewById(R.id.continuous_search_list_item_description);
            if (textDescription != null) {
                ApiCompatibilityUtils.setTextAppearance(textDescription,
                        model.get(ContinuousSearchListProperties.DESCRIPTION_TEXT_STYLE));
            }
        }
    }
}
