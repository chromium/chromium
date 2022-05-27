// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

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
        } else if (key == HistoryClustersItemProperties.VISIBILITY) {
            itemView.setVisibility(model.get(HistoryClustersItemProperties.VISIBILITY));
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

    public static void bindClusterView(PropertyModel model, View view, PropertyKey key) {
        HistoryClusterView clusterView = (HistoryClusterView) view;
        if (key == HistoryClustersItemProperties.CLICK_HANDLER) {
            clusterView.setOnClickListener(model.get(HistoryClustersItemProperties.CLICK_HANDLER));
        } else if (key == HistoryClustersItemProperties.END_BUTTON_DRAWABLE) {
            clusterView.setEndButtonDrawable(
                    model.get(HistoryClustersItemProperties.END_BUTTON_DRAWABLE));
        } else if (key == HistoryClustersItemProperties.ICON_DRAWABLE) {
            clusterView.setIconDrawable(model.get(HistoryClustersItemProperties.ICON_DRAWABLE));
        } else if (key == HistoryClustersItemProperties.LABEL) {
            clusterView.setLabel(model.get(HistoryClustersItemProperties.LABEL));
        } else if (key == HistoryClustersItemProperties.TITLE) {
            clusterView.setTitle(model.get(HistoryClustersItemProperties.TITLE));
        }
    }

    public static void bindRelatedSearchesView(PropertyModel model, View view, PropertyKey key) {
        HistoryClustersRelatedSearchesChipLayout relatedSearchesChipLayout =
                (HistoryClustersRelatedSearchesChipLayout) view;
        if (key == HistoryClustersItemProperties.CHIP_CLICK_HANDLER) {
            relatedSearchesChipLayout.setOnChipClickHandler(
                    model.get(HistoryClustersItemProperties.CHIP_CLICK_HANDLER));
        } else if (key == HistoryClustersItemProperties.RELATED_SEARCHES) {
            List<String> relatedSearches =
                    model.get(HistoryClustersItemProperties.RELATED_SEARCHES);
            relatedSearchesChipLayout.setRelatedSearches(relatedSearches);
        } else if (key == HistoryClustersItemProperties.VISIBILITY) {
            relatedSearchesChipLayout.setVisibility(
                    model.get(HistoryClustersItemProperties.VISIBILITY));
        }
    }

    public static void bindToggleView(
            PropertyModel propertyModel, View view, PropertyKey propertyKey) {
        // The toggle view's appearance and behavior are dictated by our parent component, so we
        // don't manipulate it here.
    }
}