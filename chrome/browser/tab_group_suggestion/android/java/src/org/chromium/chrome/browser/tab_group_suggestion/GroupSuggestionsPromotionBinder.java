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
        }
    }
}
