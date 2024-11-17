// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui.bookmark_batch_upload_card;

import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class BookmarkBatchUploadCardBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the batch upload card to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (BookmarkBatchUploadCardProperties.DESCRIPTION_TEXT == propertyKey) {
            TextView text = (TextView) view.findViewById(R.id.signin_settings_card_description);
            text.setText(model.get(BookmarkBatchUploadCardProperties.DESCRIPTION_TEXT));
        } else if (BookmarkBatchUploadCardProperties.BUTTON_TEXT == propertyKey) {
            Button button = (Button) view.findViewById(R.id.signin_settings_card_button);
            button.setText(model.get(BookmarkBatchUploadCardProperties.BUTTON_TEXT));
            button.setOnClickListener(
                    model.get(BookmarkBatchUploadCardProperties.On_CLICK_LISTENER));
        } else if (BookmarkBatchUploadCardProperties.ICON == propertyKey) {
            ImageView image = (ImageView) view.findViewById(R.id.signin_settings_card_icon);
            image.setImageDrawable(model.get(BookmarkBatchUploadCardProperties.ICON));
        }
    }
}
