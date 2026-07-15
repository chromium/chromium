// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
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
            view.setRemoveClickListener(
                    wrapRunnable(model, BookmarkPopupProperties.REMOVE_BUTTON_CLICK_LISTENER));
        } else if (key == BookmarkPopupProperties.CLOSE_BUTTON_CLICK_LISTENER) {
            view.setCloseClickListener(
                    wrapRunnable(model, BookmarkPopupProperties.CLOSE_BUTTON_CLICK_LISTENER));
        } else if (key == BookmarkPopupProperties.DONE_BUTTON_CLICK_LISTENER) {
            view.setDoneClickListener(
                    wrapRunnable(model, BookmarkPopupProperties.DONE_BUTTON_CLICK_LISTENER));
        } else if (key == BookmarkPopupProperties.TITLE_CHANGED_LISTENER) {
            view.setTitleTextWatcher(
                    new EmptyTextWatcher() {
                        @Override
                        public void onTextChanged(
                                CharSequence s, int start, int before, int count) {
                            Callback<String> callback =
                                    model.get(BookmarkPopupProperties.TITLE_CHANGED_LISTENER);
                            if (callback != null) {
                                callback.onResult(s.toString());
                            }
                        }
                    });
        } else if (key == BookmarkPopupProperties.FOLDER_NAME) {
            view.setFolderName(model.get(BookmarkPopupProperties.FOLDER_NAME));
        } else if (key == BookmarkPopupProperties.FOLDER_ROW_CLICK_LISTENER) {
            view.setFolderRowClickListener(
                    wrapRunnable(model, BookmarkPopupProperties.FOLDER_ROW_CLICK_LISTENER));
        } else if (key == BookmarkPopupProperties.IMAGE_SCALE_TYPE) {
            view.setImageScaleType(model.get(BookmarkPopupProperties.IMAGE_SCALE_TYPE));
        }
    }

    // Shares the process of checking for null and running the action for OnClickListeners.
    private static View.OnClickListener wrapRunnable(
            PropertyModel model, ReadableObjectPropertyKey<Runnable> key) {
        return (v) -> {
            Runnable listener = model.get(key);
            if (listener != null) {
                listener.run();
            }
        };
    }
}
