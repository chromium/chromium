// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.view.View;
import android.view.View.OnClickListener;

import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.LayoutParams;

import org.chromium.components.browser_ui.widget.MoreProgressButton;
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
        } else if (key == HistoryClustersItemProperties.CLUSTER_VISIT) {
            itemView.setItem(model.get(HistoryClustersItemProperties.CLUSTER_VISIT));
        } else if (key == HistoryClustersItemProperties.DIVIDER_IS_THICK) {
            itemView.setHasThickDivider(model.get(HistoryClustersItemProperties.DIVIDER_IS_THICK));
        } else if (key == HistoryClustersItemProperties.DIVIDER_VISIBLE) {
            itemView.setDividerVisibility(model.get(HistoryClustersItemProperties.DIVIDER_VISIBLE));
        } else if (key == HistoryClustersItemProperties.END_BUTTON_CLICK_HANDLER) {
            itemView.setEndButtonClickHandler(
                    model.get(HistoryClustersItemProperties.END_BUTTON_CLICK_HANDLER));
        } else if (key == HistoryClustersItemProperties.END_BUTTON_VISIBLE) {
            itemView.setEndButtonVisibility(
                    model.get(HistoryClustersItemProperties.END_BUTTON_VISIBLE));
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
                String query = queryState.getQuery();
                toolbar.setSearchText(query, query.isEmpty() || toolbar.isSearchTextFocused());
                if (!toolbar.isSearching()) {
                    toolbar.showSearchView(queryState.getQuery().isEmpty());
                }
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
                listLayout.onStartSearch(queryState.getSearchEmptyString());
            } else {
                listLayout.onEndSearch();
            }
        }
    }

    public static void bindClusterView(PropertyModel model, View view, PropertyKey key) {
        HistoryClusterView clusterView = (HistoryClusterView) view;
        if (key == HistoryClustersItemProperties.ACCESSIBILITY_STATE) {
            clusterView.setAccessibilityState(
                    model.get(HistoryClustersItemProperties.ACCESSIBILITY_STATE));
        } else if (key == HistoryClustersItemProperties.CLICK_HANDLER) {
            OnClickListener clickListener = model.get(HistoryClustersItemProperties.CLICK_HANDLER);
            clusterView.setOnClickListener(clickListener);
        } else if (key == HistoryClustersItemProperties.DIVIDER_IS_THICK) {
            clusterView.setHasThickDivider(
                    model.get(HistoryClustersItemProperties.DIVIDER_IS_THICK));
        } else if (key == HistoryClustersItemProperties.DIVIDER_VISIBLE) {
            clusterView.setDividerVisibility(
                    model.get(HistoryClustersItemProperties.DIVIDER_VISIBLE));
        } else if (key == HistoryClustersItemProperties.END_BUTTON_DRAWABLE) {
            clusterView.setEndButtonDrawable(
                    model.get(HistoryClustersItemProperties.END_BUTTON_DRAWABLE));
        } else if (key == HistoryClustersItemProperties.ICON_DRAWABLE) {
            clusterView.setIconDrawable(model.get(HistoryClustersItemProperties.ICON_DRAWABLE));
        } else if (key == HistoryClustersItemProperties.LABEL) {
            clusterView.setLabel(model.get(HistoryClustersItemProperties.LABEL));
        } else if (key == HistoryClustersItemProperties.START_ICON_BACKGROUND_RES) {
            clusterView.setStartIconBackgroundRes(
                    model.get(HistoryClustersItemProperties.START_ICON_BACKGROUND_RES));
        } else if (key == HistoryClustersItemProperties.START_ICON_VISIBILITY) {
            clusterView.setIconDrawableVisibility(
                    model.get(HistoryClustersItemProperties.START_ICON_VISIBILITY));
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
        } else if (key == HistoryClustersItemProperties.DIVIDER_IS_THICK) {
            relatedSearchesChipLayout.setHasThickDivider(
                    model.get(HistoryClustersItemProperties.DIVIDER_IS_THICK));
        } else if (key == HistoryClustersItemProperties.DIVIDER_VISIBLE) {
            relatedSearchesChipLayout.setDividerVisibility(
                    model.get(HistoryClustersItemProperties.DIVIDER_VISIBLE));
        } else if (key == HistoryClustersItemProperties.RELATED_SEARCHES) {
            List<String> relatedSearches =
                    model.get(HistoryClustersItemProperties.RELATED_SEARCHES);
            relatedSearchesChipLayout.setRelatedSearches(relatedSearches);
        } else if (key == HistoryClustersItemProperties.VISIBILITY) {
            relatedSearchesChipLayout.setVisibility(
                    model.get(HistoryClustersItemProperties.VISIBILITY));
        }
    }

    public static void noopBindView(
            PropertyModel propertyModel, View view, PropertyKey propertyKey) {
        // This view's appearance and behavior are dictated by our parent component, so we
        // don't manipulate it here.
    }

    public static void bindMoreProgressView(
            PropertyModel propertyModel, View view, PropertyKey key) {
        MoreProgressButton button = (MoreProgressButton) view;
        if (key == HistoryClustersItemProperties.CLICK_HANDLER) {
            button.setOnClickRunnable(
                    () ->
                            propertyModel
                                    .get(HistoryClustersItemProperties.CLICK_HANDLER)
                                    .onClick(null));
        } else if (key == HistoryClustersItemProperties.PROGRESS_BUTTON_STATE) {
            button.setState(propertyModel.get(HistoryClustersItemProperties.PROGRESS_BUTTON_STATE));
        } else if (key == HistoryClustersItemProperties.SHOW_VERTICALLY_CENTERED) {
            boolean showVerticallyCentered =
                    propertyModel.get(HistoryClustersItemProperties.SHOW_VERTICALLY_CENTERED);
            RecyclerView.LayoutParams layoutParams = (LayoutParams) button.getLayoutParams();
            if (showVerticallyCentered) {
                layoutParams.height = LayoutParams.MATCH_PARENT;
            } else {
                layoutParams.height = LayoutParams.WRAP_CONTENT;
            }
        }
    }
}
