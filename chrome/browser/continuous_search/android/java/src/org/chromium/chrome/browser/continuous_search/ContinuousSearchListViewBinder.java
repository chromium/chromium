// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.text.TextUtils;
import android.text.TextUtils.TruncateAt;
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
import org.chromium.ui.widget.ChipView;
import org.chromium.url.GURL;

/**
 * Responsible for binding the {@link PropertyModel} for a search result item to a View.
 */
class ContinuousSearchListViewBinder {
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
        ContinuousSearchChipView chipView = view.findViewById(R.id.csn_chip);
        boolean showResultTitle = model.get(ListItemProperties.SHOW_RESULT_TITLE);

        if (ListItemProperties.SHOW_RESULT_TITLE == propertyKey && showResultTitle) {
            chipView.initTwoLineChipView();
        } else if (ListItemProperties.LABEL == propertyKey && showResultTitle) {
            assert chipView.twoLineChipViewInitialized();
            setupTextView(chipView.getPrimaryTextView(), model.get(ListItemProperties.LABEL),
                    view.getResources().getDimensionPixelSize(R.dimen.csn_chip_text_max_width),
                    TruncateAt.END);
        } else if (ListItemProperties.URL == propertyKey) {
            GURL url = model.get(ListItemProperties.URL);
            TextView textView = showResultTitle ? chipView.getSecondaryTextView()
                                                : chipView.getPrimaryTextView();

            String safeUrl = "";
            if (url != null) {
                // Schemes are omitted as these are pre-navigation URLs and we have very limited UI
                // surface.
                //
                // NOTE: the Google SRP does show schemes so consider revisiting this in future.
                safeUrl = UrlFormatter.formatUrlForSecurityDisplay(
                        url, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
            }
            setupTextView(textView, safeUrl,
                    view.getResources().getDimensionPixelSize(R.dimen.csn_chip_text_max_width),
                    TruncateAt.START);
        } else if (ListItemProperties.IS_SELECTED == propertyKey) {
            setBorder(chipView, model);
        } else if (ListItemProperties.BORDER_COLOR == propertyKey) {
            setBorder(chipView, model);
        } else if (ListItemProperties.CLICK_LISTENER == propertyKey) {
            view.setOnClickListener(model.get(ListItemProperties.CLICK_LISTENER));
        } else if (ListItemProperties.BACKGROUND_COLOR == propertyKey) {
            chipView.setBackgroundColor(model.get(ListItemProperties.BACKGROUND_COLOR));
        } else if (ListItemProperties.PRIMARY_TEXT_STYLE == propertyKey) {
            ApiCompatibilityUtils.setTextAppearance(chipView.getPrimaryTextView(),
                    model.get(ListItemProperties.PRIMARY_TEXT_STYLE));
        } else if (ListItemProperties.SECONDARY_TEXT_STYLE == propertyKey) {
            ApiCompatibilityUtils.setTextAppearance(chipView.getSecondaryTextView(),
                    model.get(ListItemProperties.SECONDARY_TEXT_STYLE));
        }
    }

    private static void setupTextView(
            TextView textView, String text, int maxWidth, TextUtils.TruncateAt truncateAt) {
        textView.setEllipsize(truncateAt);
        textView.setMaxLines(1);
        textView.setTextDirection(View.TEXT_DIRECTION_LTR);
        textView.setMaxWidth(maxWidth);
        textView.setText(text);
    }

    private static void setBorder(ChipView chipView, PropertyModel model) {
        chipView.setBorder(chipView.getResources().getDimensionPixelSize(R.dimen.chip_border_width),
                model.get(ListItemProperties.IS_SELECTED)
                        ? model.get(ListItemProperties.BORDER_COLOR)
                        : model.get(ListItemProperties.BACKGROUND_COLOR));
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
