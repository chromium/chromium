// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.graphics.drawable.GradientDrawable;
import android.view.View;
import android.widget.ImageView;
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
    private static final int BORDER_WIDTH = 5;

    /**
     * Binds properties related to an individual item within the RecyclerView.
     */
    static void bindListItem(PropertyModel model, View view, PropertyKey propertyKey) {
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
            setBorder(model, view);
        } else if (ContinuousSearchListProperties.BORDER_COLOR == propertyKey) {
            setBorder(model, view);
        } else if (ContinuousSearchListProperties.CLICK_LISTENER == propertyKey) {
            view.setOnClickListener(model.get(ContinuousSearchListProperties.CLICK_LISTENER));
        } else if (ContinuousSearchListProperties.BACKGROUND_COLOR == propertyKey) {
            if (view.getBackground() != null) {
                GradientDrawable drawable = (GradientDrawable) view.getBackground();
                drawable.mutate();
                drawable.setColor(model.get(ContinuousSearchListProperties.BACKGROUND_COLOR));
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

    /**
     * Binds properties related to the root view, that includes the RecyclerView.
     */
    static void bindRootView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (ContinuousSearchListProperties.BACKGROUND_COLOR == propertyKey) {
            view.setBackgroundColor(model.get(ContinuousSearchListProperties.BACKGROUND_COLOR));
        } else if (ContinuousSearchListProperties.FOREGROUND_COLOR == propertyKey) {
            ImageView buttonDismiss = view.findViewById(R.id.button_dismiss);
            buttonDismiss.setColorFilter(
                    model.get(ContinuousSearchListProperties.FOREGROUND_COLOR));
        } else if (ContinuousSearchListProperties.DISMISS_CLICK_CALLBACK == propertyKey) {
            ImageView buttonDismiss = view.findViewById(R.id.button_dismiss);
            buttonDismiss.setOnClickListener(
                    model.get(ContinuousSearchListProperties.DISMISS_CLICK_CALLBACK));
        }
    }

    private static void setBorder(PropertyModel model, View view) {
        if (view.getBackground() == null) return;

        GradientDrawable drawable = (GradientDrawable) view.getBackground();
        drawable.mutate();
        drawable.setStroke(model.get(ContinuousSearchListProperties.IS_SELECTED) ? BORDER_WIDTH : 0,
                model.get(ContinuousSearchListProperties.BORDER_COLOR));
    }
}
