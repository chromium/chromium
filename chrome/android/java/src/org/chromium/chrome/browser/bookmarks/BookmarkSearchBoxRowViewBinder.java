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
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/** Binds model properties to view methods for {@link BookmarkSearchBoxRow}. */
class BookmarkSearchBoxRowViewBinder {
    public static ViewBinder<PropertyModel, View, PropertyKey> createViewBinder() {
        return new BookmarkSearchBoxRowViewBinder()::bind;
    }

    private boolean mInBind;

    private void bind(PropertyModel model, View view, PropertyKey key) {
        mInBind = true;
        bindInternal(model, view, key);
        mInBind = false;
    }

    private void bindInternal(PropertyModel model, View view, PropertyKey key) {
        BookmarkSearchBoxRow row = (BookmarkSearchBoxRow) view;
        ChipView shoppingChip = view.findViewById(R.id.shopping_filter_chip);
        if (key == BookmarkSearchBoxRowProperties.SEARCH_TEXT_CHANGE_CALLBACK) {
            Callback<String> callback =
                    wrapCallback(model, BookmarkSearchBoxRowProperties.SEARCH_TEXT_CHANGE_CALLBACK);
            row.setSearchTextCallback(callback);
        } else if (key == BookmarkSearchBoxRowProperties.SEARCH_TEXT) {
            row.setSearchText(model.get(BookmarkSearchBoxRowProperties.SEARCH_TEXT));
        } else if (key == BookmarkSearchBoxRowProperties.FOCUS_CHANGE_CALLBACK) {
            Callback<Boolean> callback =
                    wrapCallback(model, BookmarkSearchBoxRowProperties.FOCUS_CHANGE_CALLBACK);
            row.setFocusChangeCallback(callback);
        } else if (key == BookmarkSearchBoxRowProperties.HAS_FOCUS) {
            row.setHasFocus(model.get(BookmarkSearchBoxRowProperties.HAS_FOCUS));
        } else if (key == BookmarkSearchBoxRowProperties.CLEAR_SEARCH_TEXT_RUNNABLE) {
            Runnable runnable =
                    wrapRunnable(model, BookmarkSearchBoxRowProperties.CLEAR_SEARCH_TEXT_RUNNABLE);
            row.setClearSearchTextButtonRunnable(runnable);
        } else if (key == BookmarkSearchBoxRowProperties.CLEAR_SEARCH_TEXT_BUTTON_VISIBILITY) {
            row.setClearSearchTextButtonVisibility(
                    model.get(BookmarkSearchBoxRowProperties.CLEAR_SEARCH_TEXT_BUTTON_VISIBILITY));
        } else if (key == BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY) {
            boolean isVisible = model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY);
            ((View) shoppingChip.getParent()).setVisibility(isVisible ? View.VISIBLE : View.GONE);
        } else if (key == BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TOGGLE_CALLBACK) {
            Callback<Boolean> onToggle =
                    wrapCallback(
                            model, BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TOGGLE_CALLBACK);
            shoppingChip.setOnClickListener(
                    (View v) -> {
                        onToggle.onResult(
                                !model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_SELECTED));
                    });
        } else if (key == BookmarkSearchBoxRowProperties.SHOPPING_CHIP_SELECTED) {
            shoppingChip.setSelected(
                    model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_SELECTED));
        } else if (key == BookmarkSearchBoxRowProperties.SHOPPING_CHIP_START_ICON_RES) {
            final @DrawableRes int res =
                    model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_START_ICON_RES);
            // TODO(crbug.com/40924045): Use tintWithTextColor because the drawable tint
            // is broken.
            shoppingChip.setIcon(res, /* tintWithTextColor= */ true);
        } else if (key == BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TEXT_RES) {
            final @StringRes int res =
                    model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TEXT_RES);
            shoppingChip.getPrimaryTextView().setText(res);
        }
    }

    private Runnable wrapRunnable(
            PropertyModel model, ReadableObjectPropertyKey<Runnable> propertyKey) {
        return () -> {
            if (!mInBind) {
                model.get(propertyKey).run();
            }
        };
    }

    private <T> Callback<T> wrapCallback(
            PropertyModel model, ReadableObjectPropertyKey<Callback<T>> propertyKey) {
        return result -> {
            if (!mInBind) {
                model.get(propertyKey).onResult(result);
            }
        };
    }
}
