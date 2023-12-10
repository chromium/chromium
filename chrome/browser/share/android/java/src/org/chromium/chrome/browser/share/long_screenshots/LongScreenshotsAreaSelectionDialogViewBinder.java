// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import android.view.View;
import android.widget.ImageButton;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Class responsible for binding the model and the view. */
class LongScreenshotsAreaSelectionDialogViewBinder {
    static void bind(PropertyModel model, View parent, PropertyKey propertyKey) {
        if (LongScreenshotsAreaSelectionDialogProperties.CLOSE_BUTTON_CALLBACK.equals(
                propertyKey)) {
            ImageButton view = (ImageButton) parent.findViewById(R.id.close_button);
            view.setOnClickListener(
                    model.get(LongScreenshotsAreaSelectionDialogProperties.CLOSE_BUTTON_CALLBACK));
        } else if (LongScreenshotsAreaSelectionDialogProperties.DONE_BUTTON_CALLBACK.equals(
                propertyKey)) {
            ImageButton view = (ImageButton) parent.findViewById(R.id.done_button);
            view.setOnClickListener(
                    model.get(LongScreenshotsAreaSelectionDialogProperties.DONE_BUTTON_CALLBACK));
        } else if (LongScreenshotsAreaSelectionDialogProperties.DOWN_BUTTON_CALLBACK.equals(
                propertyKey)) {
            ImageButton view = (ImageButton) parent.findViewById(R.id.down_button);
            view.setOnClickListener(
                    model.get(LongScreenshotsAreaSelectionDialogProperties.DOWN_BUTTON_CALLBACK));
        } else if (LongScreenshotsAreaSelectionDialogProperties.UP_BUTTON_CALLBACK.equals(
                propertyKey)) {
            ImageButton view = (ImageButton) parent.findViewById(R.id.up_button);
            view.setOnClickListener(
                    model.get(LongScreenshotsAreaSelectionDialogProperties.UP_BUTTON_CALLBACK));
        }
    }
}
