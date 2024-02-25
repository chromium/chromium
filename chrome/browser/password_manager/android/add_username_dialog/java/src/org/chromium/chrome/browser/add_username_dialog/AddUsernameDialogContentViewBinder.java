// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.add_username_dialog;

import static org.chromium.chrome.browser.add_username_dialog.AddUsernameDialogContentProperties.PASSWORD;
import static org.chromium.chrome.browser.add_username_dialog.AddUsernameDialogContentProperties.USERNAME_CHANGED_CALLBACK;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class AddUsernameDialogContentViewBinder {
    static void bind(
            PropertyModel model, AddUsernameDialogContentView view, PropertyKey propertyKey) {
        if (propertyKey == PASSWORD) {
            view.setPassword(model.get(PASSWORD));
        } else if (propertyKey == USERNAME_CHANGED_CALLBACK) {
            view.setUsernameChangedCallback(model.get(USERNAME_CHANGED_CALLBACK));
        }
    }

    private AddUsernameDialogContentViewBinder() {}
}
