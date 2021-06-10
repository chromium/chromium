// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.graphics.drawable.GradientDrawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchListProperties.ListItemProperties;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchListProperties.ProviderProperties;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * Responsible for binding the {@link PropertyModel} for a search result item to a View.
 */
class ContinuousSearchListViewBinder {
    private static final int BORDER_WIDTH = 5;

    static void bindProvider(PropertyModel model, View view, PropertyKey propertyKey) {
        if (ProviderProperties.LABEL == propertyKey) {
            TextView textView = view.findViewById(R.id.continuous_search_provider_label);
            textView.setText(model.get(ProviderProperties.LABEL));
        } else if (ProviderProperties.ICON_RESOURCE == propertyKey) {
            TextView textView = view.findViewById(R.id.continuous_search_provider_label);
            // Add the icon at the start of the provider label
            textView.setCompoundDrawablesRelativeWithIntrinsicBounds(
                    model.get(ProviderProperties.ICON_RESOURCE), 0, 0, 0);
        } else if (ProviderProperties.CLICK_LISTENER == propertyKey) {
            view.setOnClickListener(model.get(ProviderProperties.CLICK_LISTENER));
        } else if (ProviderProperties.TEXT_STYLE == propertyKey) {
            TextView textView = view.findViewById(R.id.continuous_search_provider_label);
            ApiCompatibilityUtils.setTextAppearance(
                    textView, model.get(ProviderProperties.TEXT_STYLE));
        }
    }

    /**
     * Binds properties related to an individual item within the RecyclerView.
     */
    static void bindListItem(PropertyModel model, View view, PropertyKey propertyKey) {
        if (ListItemProperties.LABEL == propertyKey) {
            TextView textView = view.findViewById(R.id.continuous_search_list_item_text);
            textView.setText(model.get(ListItemProperties.LABEL));
        } else if (ListItemProperties.URL == propertyKey) {
            GURL url = model.get(ListItemProperties.URL);
            TextView textView = view.findViewById(R.id.continuous_search_list_item_description);

            String safeUrl = "";
            if (url != null) {
                // Schemes are omitted as these are pre-navigation URLs and we have very limited UI
                // surface.
                //
                // NOTE: the Google SRP does show schemes so consider revisiting this in future.
                safeUrl = UrlFormatter.formatUrlForSecurityDisplay(
                        url, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
            }
            textView.setTextDirection(View.TEXT_DIRECTION_LTR);
            textView.setText(safeUrl);
        } else if (ListItemProperties.IS_SELECTED == propertyKey) {
            setBorder(model, view);
        } else if (ListItemProperties.BORDER_COLOR == propertyKey) {
            setBorder(model, view);
        } else if (ListItemProperties.CLICK_LISTENER == propertyKey) {
            view.setOnClickListener(model.get(ListItemProperties.CLICK_LISTENER));
        } else if (ListItemProperties.BACKGROUND_COLOR == propertyKey) {
            GradientDrawable drawable = (GradientDrawable) view.getBackground();
            drawable.mutate();
            drawable.setColor(model.get(ListItemProperties.BACKGROUND_COLOR));
        } else if (ListItemProperties.TITLE_TEXT_STYLE == propertyKey) {
            TextView textTitle = view.findViewById(R.id.continuous_search_list_item_text);
            ApiCompatibilityUtils.setTextAppearance(
                    textTitle, model.get(ListItemProperties.TITLE_TEXT_STYLE));
        } else if (ListItemProperties.DESCRIPTION_TEXT_STYLE == propertyKey) {
            TextView textDescription =
                    view.findViewById(R.id.continuous_search_list_item_description);
            ApiCompatibilityUtils.setTextAppearance(
                    textDescription, model.get(ListItemProperties.DESCRIPTION_TEXT_STYLE));
        }
    }

    private static void setBorder(PropertyModel model, View view) {
        GradientDrawable drawable = (GradientDrawable) view.getBackground();
        drawable.mutate();
        drawable.setStroke(model.get(ListItemProperties.IS_SELECTED) ? BORDER_WIDTH : 0,
                model.get(ListItemProperties.BORDER_COLOR));
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
}
