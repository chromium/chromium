// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.widget.ImageView.ScaleType;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.EmptyTextWatcher;

/** View binder for the desktop android bookmark popup. */
@NullMarked
public class BookmarkPopupViewBinder {
    public static void bind(PropertyModel model, BookmarkPopupView view, PropertyKey key) {
        if (key == BookmarkPopupProperties.HEADER_TEXT) {
            view.setHeaderText(model.get(BookmarkPopupProperties.HEADER_TEXT));
        } else if (key == BookmarkPopupProperties.TITLE) {
            view.setTitle(model.get(BookmarkPopupProperties.TITLE));
        } else if (key == BookmarkPopupProperties.IMAGE_DRAWABLE) {
            view.setImageDrawable(model.get(BookmarkPopupProperties.IMAGE_DRAWABLE));
        } else if (key == BookmarkPopupProperties.REMOVE_BUTTON_CLICK_LISTENER) {
            Runnable runnable = model.get(BookmarkPopupProperties.REMOVE_BUTTON_CLICK_LISTENER);
            view.setRemoveClickListener(runnable == null ? null : v -> runnable.run());
        } else if (key == BookmarkPopupProperties.CLOSE_BUTTON_CLICK_LISTENER) {
            Runnable runnable = model.get(BookmarkPopupProperties.CLOSE_BUTTON_CLICK_LISTENER);
            view.setCloseClickListener(runnable == null ? null : v -> runnable.run());
        } else if (key == BookmarkPopupProperties.DONE_BUTTON_CLICK_LISTENER) {
            Runnable runnable = model.get(BookmarkPopupProperties.DONE_BUTTON_CLICK_LISTENER);
            view.setDoneClickListener(runnable == null ? null : v -> runnable.run());
        } else if (key == BookmarkPopupProperties.TITLE_CHANGED_LISTENER) {
            Callback<String> callback = model.get(BookmarkPopupProperties.TITLE_CHANGED_LISTENER);
            if (callback == null) {
                view.setTitleTextWatcher(null);
            } else {
                view.setTitleTextWatcher(
                        new EmptyTextWatcher() {
                            @Override
                            public void onTextChanged(
                                    CharSequence s, int start, int before, int count) {
                                callback.onResult(s.toString());
                            }
                        });
            }
        } else if (key == BookmarkPopupProperties.FOLDER_NAME) {
            view.setFolderName(model.get(BookmarkPopupProperties.FOLDER_NAME));
        } else if (key == BookmarkPopupProperties.FOLDER_ROW_CLICK_LISTENER) {
            Runnable runnable = model.get(BookmarkPopupProperties.FOLDER_ROW_CLICK_LISTENER);
            view.setFolderRowClickListener(runnable == null ? null : v -> runnable.run());
        } else if (key == BookmarkPopupProperties.IMAGE_SCALE_TYPE) {
            ScaleType scaleType = model.get(BookmarkPopupProperties.IMAGE_SCALE_TYPE);
            if (scaleType != null) {
                view.setImageScaleType(scaleType);
            }
        } else if (key == BookmarkPopupProperties.PRICE_TRACKING_VISIBLE) {
            view.setPriceTrackingVisible(model.get(BookmarkPopupProperties.PRICE_TRACKING_VISIBLE));
        } else if (key == BookmarkPopupProperties.PRICE_TRACKING_ENABLED) {
            view.setPriceTrackingEnabled(model.get(BookmarkPopupProperties.PRICE_TRACKING_ENABLED));
        } else if (key == BookmarkPopupProperties.PRICE_TRACKING_SWITCH_CHECKED) {
            view.setPriceTrackingSwitchChecked(
                    model.get(BookmarkPopupProperties.PRICE_TRACKING_SWITCH_CHECKED));
        } else if (key == BookmarkPopupProperties.PRICE_TRACKING_SWITCH_LISTENER) {
            view.setPriceTrackingSwitchListener(
                    model.get(BookmarkPopupProperties.PRICE_TRACKING_SWITCH_LISTENER));
        }
    }
}
