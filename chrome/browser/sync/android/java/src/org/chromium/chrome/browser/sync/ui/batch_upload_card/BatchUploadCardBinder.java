// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui.batch_upload_card;

import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.sync.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
class BatchUploadCardBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the batch upload card to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (BatchUploadCardProperties.DESCRIPTION_TEXT == propertyKey) {
            TextView text = (TextView) view.findViewById(R.id.signin_settings_card_description);
            text.setText(model.get(BatchUploadCardProperties.DESCRIPTION_TEXT));
        } else if (BatchUploadCardProperties.BUTTON_TEXT == propertyKey) {
            Button button = (Button) view.findViewById(R.id.signin_settings_card_button);
            button.setText(model.get(BatchUploadCardProperties.BUTTON_TEXT));
            button.setOnClickListener(model.get(BatchUploadCardProperties.ON_CLICK_LISTENER));
        } else if (BatchUploadCardProperties.ICON == propertyKey) {
            ImageView image = (ImageView) view.findViewById(R.id.signin_settings_card_icon);
            image.setImageDrawable(model.get(BatchUploadCardProperties.ICON));
        }
    }
}
