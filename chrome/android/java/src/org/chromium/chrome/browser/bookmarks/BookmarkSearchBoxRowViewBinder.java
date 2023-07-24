// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds model properties to view methods for {@link BookmarkSearchBoxRow}. */
class BookmarkSearchBoxRowViewBinder {
    static void bind(PropertyModel model, View view, PropertyKey key) {
        BookmarkSearchBoxRow row = (BookmarkSearchBoxRow) view;
        ChipView shoppingChip = view.findViewById(R.id.shopping_filter_chip);
        if (key == BookmarkSearchBoxRowProperties.QUERY_CALLBACK) {
            row.setQueryCallback(model.get(BookmarkSearchBoxRowProperties.QUERY_CALLBACK));
        } else if (key == BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY) {
            boolean isVisible = model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY);
            ((View) shoppingChip.getParent()).setVisibility(isVisible ? View.VISIBLE : View.GONE);
        } else if (key == BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TOGGLE_CALLBACK) {
            Callback<Boolean> onToggle =
                    model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TOGGLE_CALLBACK);
            shoppingChip.setOnClickListener((View v) -> {
                boolean newState = !shoppingChip.isSelected();
                shoppingChip.setSelected(newState);
                onToggle.onResult(newState);
            });
        } else if (key == BookmarkSearchBoxRowProperties.SHOPPING_CHIP_START_ICON_RES) {
            final @DrawableRes int res =
                    model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_START_ICON_RES);
            // TODO(https://crbug.com/1466583): Use tintWithTextColor because the drawable tint is
            // broken.
            shoppingChip.setIcon(res, /*tintWithTextColor*/ true);
        } else if (key == BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TEXT_RES) {
            final @StringRes int res =
                    model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TEXT_RES);
            shoppingChip.getPrimaryTextView().setText(res);
        }
    }
}
