// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.DETAILS;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.HELP_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.ILLUSTRATION;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.ILLUSTRATION_VISIBLE;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.TITLE;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Class responsible for binding the model and the view. */
class PasswordManagerDialogViewBinder {
    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        PasswordManagerDialogView dialogView = (PasswordManagerDialogView) view;
        if (HELP_BUTTON_CALLBACK == propertyKey) {
            dialogView.addHelpButton(model.get(HELP_BUTTON_CALLBACK));
        } else if (ILLUSTRATION == propertyKey) {
            dialogView.setIllustration(model.get(ILLUSTRATION));
        } else if (ILLUSTRATION_VISIBLE == propertyKey) {
            dialogView.updateIllustrationVisibility(model.get(ILLUSTRATION_VISIBLE));
            dialogView.updateHelpIcon(!model.get(ILLUSTRATION_VISIBLE));
        } else if (TITLE == propertyKey) {
            dialogView.setTitle(model.get(TITLE));
        } else if (DETAILS == propertyKey) {
            dialogView.setDetails(model.get(DETAILS));
        }
    }
}
