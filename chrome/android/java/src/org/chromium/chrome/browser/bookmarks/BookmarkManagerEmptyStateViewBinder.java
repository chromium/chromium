// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.chromium.chrome.browser.bookmarks.BookmarkManagerEmptyStateProperties.EMPTY_STATE_DESCRIPTION_RES;
import static org.chromium.chrome.browser.bookmarks.BookmarkManagerEmptyStateProperties.EMPTY_STATE_IMAGE_RES;
import static org.chromium.chrome.browser.bookmarks.BookmarkManagerEmptyStateProperties.EMPTY_STATE_TITLE_RES;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Responsible for binding views to their properties. */
class BookmarkManagerEmptyStateViewBinder {
    public static void bindEmptyStateView(PropertyModel model, View view, PropertyKey key) {
        if (key == EMPTY_STATE_TITLE_RES) {
            ((TextView) view.findViewById(R.id.empty_state_text_title))
                    .setText(model.get(EMPTY_STATE_TITLE_RES));
        } else if (key == EMPTY_STATE_DESCRIPTION_RES) {
            ((TextView) view.findViewById(R.id.empty_state_text_description))
                    .setText(model.get(EMPTY_STATE_DESCRIPTION_RES));
        } else if (key == EMPTY_STATE_IMAGE_RES) {
            ((ImageView) view.findViewById(R.id.empty_state_icon))
                    .setImageResource(model.get(EMPTY_STATE_IMAGE_RES));
        }
    }
}
