// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

/** Binder class for a GroupSuggestions promotion UI. */
@NullMarked
public class GroupSuggestionsPromotionBinder {
    public static void bind(
            @NonNull PropertyModel model,
            @NonNull LinearLayout view,
            @NonNull PropertyKey propertyKey) {
        if (propertyKey == GroupSuggestionsPromotionProperties.PROMO_HEADER) {
            TextView promoHeaderView = view.findViewById(R.id.promo_header);
            promoHeaderView.setText(model.get(GroupSuggestionsPromotionProperties.PROMO_HEADER));
        } else if (propertyKey == GroupSuggestionsPromotionProperties.PROMO_CONTENTS) {
            TextView promoContentView = view.findViewById(R.id.promo_contents);
            promoContentView.setText(model.get(GroupSuggestionsPromotionProperties.PROMO_CONTENTS));
        } else if (propertyKey == GroupSuggestionsPromotionProperties.SUGGESTED_NAME) {
            TextView suggestedNameView = view.findViewById(R.id.suggested_name);
            suggestedNameView.setText(
                    model.get(GroupSuggestionsPromotionProperties.SUGGESTED_NAME));
        } else if (propertyKey == GroupSuggestionsPromotionProperties.GROUP_CONTENT_STRING) {
            TextView groupContentView = view.findViewById(R.id.group_content);
            groupContentView.setText(
                    model.get(GroupSuggestionsPromotionProperties.GROUP_CONTENT_STRING));
        } else if (propertyKey == GroupSuggestionsPromotionProperties.ACCEPT_BUTTON_TEXT) {
            ButtonCompat acceptButtonView = view.findViewById(R.id.accept_button);
            acceptButtonView.setText(
                    model.get(GroupSuggestionsPromotionProperties.ACCEPT_BUTTON_TEXT));
        } else if (propertyKey == GroupSuggestionsPromotionProperties.REJECT_BUTTON_TEXT) {
            ButtonCompat rejectButtonView = view.findViewById(R.id.reject_button);
            rejectButtonView.setText(
                    model.get(GroupSuggestionsPromotionProperties.REJECT_BUTTON_TEXT));
        } else if (propertyKey == GroupSuggestionsPromotionProperties.ACCEPT_BUTTON_LISTENER) {
            ButtonCompat acceptButtonView = view.findViewById(R.id.accept_button);
            acceptButtonView.setOnClickListener(
                    model.get(GroupSuggestionsPromotionProperties.ACCEPT_BUTTON_LISTENER));
        } else if (propertyKey == GroupSuggestionsPromotionProperties.REJECT_BUTTON_LISTENER) {
            ButtonCompat rejectButtonView = view.findViewById(R.id.reject_button);
            rejectButtonView.setOnClickListener(
                    model.get(GroupSuggestionsPromotionProperties.REJECT_BUTTON_LISTENER));
        }
    }
}
