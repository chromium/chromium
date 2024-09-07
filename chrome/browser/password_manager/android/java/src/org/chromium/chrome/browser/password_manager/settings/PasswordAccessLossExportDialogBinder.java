// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static org.chromium.chrome.browser.password_manager.settings.PasswordAccessLossExportDialogProperties.CLOSE_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.password_manager.settings.PasswordAccessLossExportDialogProperties.EXPORT_AND_DELETE_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.password_manager.settings.PasswordAccessLossExportDialogProperties.TITLE;

import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.browser.password_manager.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Maps {@link PasswordAccessLossExportDialogProperties} changes in a {@link PropertyModel} to
 * suitable methods in {@link PasswordAccessLossExportDialogFragment}'s content view.
 */
class PasswordAccessLossExportDialogBinder {
    private PasswordAccessLossExportDialogBinder() {}

    static void bind(PropertyModel model, View dialogView, PropertyKey propertyKey) {
        if (propertyKey == TITLE) {
            ((TextView) dialogView.findViewById(R.id.title)).setText(model.get(TITLE));
        } else if (propertyKey == EXPORT_AND_DELETE_BUTTON_CALLBACK) {
            dialogView
                    .findViewById(R.id.positive_button)
                    .setOnClickListener(v -> model.get(EXPORT_AND_DELETE_BUTTON_CALLBACK).run());
        } else if (propertyKey == CLOSE_BUTTON_CALLBACK) {
            dialogView
                    .findViewById(R.id.negative_button)
                    .setOnClickListener(v -> model.get(CLOSE_BUTTON_CALLBACK).run());
        } else {
            assert false : "Property " + propertyKey.toString() + " not handler in the binder";
        }
    }
}
