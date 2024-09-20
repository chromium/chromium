// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import static org.chromium.chrome.browser.access_loss.PasswordAccessLossDialogSettingsProperties.DETAILS;
import static org.chromium.chrome.browser.access_loss.PasswordAccessLossDialogSettingsProperties.HELP_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.access_loss.PasswordAccessLossDialogSettingsProperties.HELP_BUTTON_VISIBILITY;
import static org.chromium.chrome.browser.access_loss.PasswordAccessLossDialogSettingsProperties.TITLE;

import android.view.View;
import android.widget.TextView;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Maps {@link PasswordAccessLossDialogSettingsProperties} changes in a {@link PropertyModel} to
 * {@link PasswordAccessLossDialogSettingsCoordinator}'s modal dialog custom view.
 */
class PasswordAccessLossDialogSettingsViewBinder {
    private PasswordAccessLossDialogSettingsViewBinder() {}

    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == TITLE) {
            ((TextView) view.findViewById(R.id.title)).setText(model.get(TITLE));
        } else if (propertyKey == DETAILS) {
            ((TextView) view.findViewById(R.id.details)).setText(model.get(DETAILS));
        } else if (propertyKey == HELP_BUTTON_VISIBILITY) {
            boolean isVisible = model.get(HELP_BUTTON_VISIBILITY);
            view.findViewById(R.id.help_button).setVisibility(isVisible ? View.VISIBLE : View.GONE);
        } else if (propertyKey == HELP_BUTTON_CALLBACK) {
            view.findViewById(R.id.help_button)
                    .setOnClickListener(button -> model.get(HELP_BUTTON_CALLBACK).run());
        } else {
            assert false : "Property " + propertyKey.toString() + " not handled in the binder";
        }
    }
}
