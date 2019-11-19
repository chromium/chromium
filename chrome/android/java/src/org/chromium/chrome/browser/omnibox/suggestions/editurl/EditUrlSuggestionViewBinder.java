// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.editurl;

import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v7.content.res.AppCompatResources;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A mechanism for binding the {@link EditUrlSuggestionProperties} to its view. */
public class EditUrlSuggestionViewBinder {
    public static void bind(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        if (EditUrlSuggestionProperties.TITLE_TEXT == propertyKey) {
            TextView titleView = view.findViewById(R.id.title_text_view);
            titleView.setText(model.get(EditUrlSuggestionProperties.TITLE_TEXT));
        } else if (EditUrlSuggestionProperties.URL_TEXT == propertyKey) {
            TextView urlView = view.findViewById(R.id.full_url_text_view);
            urlView.setText(model.get(EditUrlSuggestionProperties.URL_TEXT));
        } else if (EditUrlSuggestionProperties.BUTTON_CLICK_LISTENER == propertyKey) {
            view.findViewById(R.id.url_edit_icon)
                    .setOnClickListener(
                            model.get(EditUrlSuggestionProperties.BUTTON_CLICK_LISTENER));
            view.findViewById(R.id.url_copy_icon)
                    .setOnClickListener(
                            model.get(EditUrlSuggestionProperties.BUTTON_CLICK_LISTENER));
            view.findViewById(R.id.url_share_icon)
                    .setOnClickListener(
                            model.get(EditUrlSuggestionProperties.BUTTON_CLICK_LISTENER));
        } else if (EditUrlSuggestionProperties.TEXT_CLICK_LISTENER == propertyKey) {
            view.setOnClickListener(model.get(EditUrlSuggestionProperties.TEXT_CLICK_LISTENER));
        } else if (SuggestionCommonProperties.SHOW_SUGGESTION_ICONS == propertyKey) {
            boolean showIcons = model.get(SuggestionCommonProperties.SHOW_SUGGESTION_ICONS);
            view.findViewById(R.id.edit_url_space)
                    .setVisibility(showIcons ? View.GONE : View.VISIBLE);
            view.findViewById(R.id.edit_url_favicon)
                    .setVisibility(showIcons ? View.VISIBLE : View.GONE);
            updateSiteFavicon(view.findViewById(R.id.edit_url_favicon), model);
        } else if (EditUrlSuggestionProperties.SITE_FAVICON == propertyKey
                || SuggestionCommonProperties.USE_DARK_COLORS == propertyKey) {
            updateSiteFavicon(view.findViewById(R.id.edit_url_favicon), model);
        }
        // TODO(mdjones): Support SuggestionCommonProperties.*
    }

    private static void updateSiteFavicon(ImageView view, PropertyModel model) {
        if (!model.get(SuggestionCommonProperties.SHOW_SUGGESTION_ICONS)) return;

        Bitmap bitmap = model.get(EditUrlSuggestionProperties.SITE_FAVICON);
        if (bitmap != null) {
            view.setImageBitmap(bitmap);
        } else {
            boolean useDarkColors = model.get(SuggestionCommonProperties.USE_DARK_COLORS);
            Drawable icon =
                    AppCompatResources.getDrawable(view.getContext(), R.drawable.ic_globe_24dp);
            int color = view.getContext().getResources().getColor(useDarkColors
                            ? R.color.default_icon_color_secondary_list
                            : R.color.white_mode_tint);
            DrawableCompat.setTint(icon, color);
            view.setImageDrawable(icon);
        }
    }
}
