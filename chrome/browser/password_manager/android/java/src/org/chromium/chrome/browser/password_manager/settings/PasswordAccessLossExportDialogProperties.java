// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

/** Defines properties for password access loss export dialog view. */
class PasswordAccessLossExportDialogProperties {
    private PasswordAccessLossExportDialogProperties() {}

    static final PropertyModel.ReadableObjectPropertyKey<String> TITLE =
            new ReadableObjectPropertyKey<>("dialog title");

    static final PropertyModel.ReadableObjectPropertyKey<Runnable>
            EXPORT_AND_DELETE_BUTTON_CALLBACK =
                    new ReadableObjectPropertyKey<>("export and delete button callback");
    static final PropertyModel.ReadableObjectPropertyKey<Runnable> CLOSE_BUTTON_CALLBACK =
            new ReadableObjectPropertyKey<>("close button callback");

    static final PropertyKey[] ALL_KEYS = {
        TITLE, EXPORT_AND_DELETE_BUTTON_CALLBACK, CLOSE_BUTTON_CALLBACK
    };
}
