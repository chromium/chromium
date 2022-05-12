// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.chrome.browser.history_clusters.HistoryClustersToolbarProperties.QueryState;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class HistoryClustersViewBinder {
    public static void bindVisitView(PropertyModel model, View view, PropertyKey key) {
        HistoryClustersItemView itemView = (HistoryClustersItemView) view;
        if (key == HistoryClustersItemProperties.CLICK_HANDLER) {
            OnClickListener clickListener = model.get(HistoryClustersItemProperties.CLICK_HANDLER);
            itemView.setOnClickListener(clickListener);
        } else if (key == HistoryClustersItemProperties.ICON_DRAWABLE) {
            itemView.setIconDrawable(model.get(HistoryClustersItemProperties.ICON_DRAWABLE));
        } else if (key == HistoryClustersItemProperties.TITLE) {
            itemView.setTitleText(model.get(HistoryClustersItemProperties.TITLE));
        } else if (key == HistoryClustersItemProperties.URL) {
            itemView.setHostText(model.get(HistoryClustersItemProperties.URL));
        }
    }

    public static void bindToolbar(
            PropertyModel model, HistoryClustersToolbar toolbar, PropertyKey key) {
        if (key == HistoryClustersToolbarProperties.QUERY_STATE) {
            QueryState queryState = model.get(HistoryClustersToolbarProperties.QUERY_STATE);
            if (queryState.isSearching()) {
                toolbar.showSearchView();
                toolbar.setSearchText(queryState.getQuery());
            } else {
                toolbar.hideSearchView();
            }
        }
    }

    public static void bindListLayout(
            PropertyModel model, SelectableListLayout listLayout, PropertyKey key) {
        if (key == HistoryClustersToolbarProperties.QUERY_STATE) {
            QueryState queryState = model.get(HistoryClustersToolbarProperties.QUERY_STATE);
            if (queryState.isSearching()) {
                listLayout.onStartSearch("");
            } else {
                listLayout.onEndSearch();
            }
        }
    }
}