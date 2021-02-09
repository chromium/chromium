// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.view.View;
import android.widget.TextView;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Responsible for binding the {@link PropertyModel} for a search result item to a View.
 */
public class SearchResultListViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (SearchResultListProperties.LABEL == propertyKey) {
            TextView textView = (TextView) view.findViewById(R.id.continuous_search_list_item_text);
            textView.setText(model.get(SearchResultListProperties.LABEL));
        } else if (SearchResultListProperties.IS_SELECTED == propertyKey) {
            view.setSelected(model.get(SearchResultListProperties.IS_SELECTED));
        } else if (SearchResultListProperties.CLICK_LISTENER == propertyKey) {
            view.setOnClickListener(model.get(SearchResultListProperties.CLICK_LISTENER));
        }
    }
}
