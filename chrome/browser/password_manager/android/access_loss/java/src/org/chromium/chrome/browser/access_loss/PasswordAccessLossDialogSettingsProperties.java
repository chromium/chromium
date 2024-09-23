// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

class PasswordAccessLossDialogSettingsProperties {
    private PasswordAccessLossDialogSettingsProperties() {}

    static final PropertyModel.ReadableObjectPropertyKey<String> TITLE =
            new ReadableObjectPropertyKey<>("dialog title");
    static final PropertyModel.ReadableObjectPropertyKey<String> DETAILS =
            new ReadableObjectPropertyKey<>("dialog details");
    static final PropertyModel.ReadableBooleanPropertyKey HELP_BUTTON_VISIBILITY =
            new ReadableBooleanPropertyKey("is help button visible");
    static final PropertyModel.ReadableObjectPropertyKey<Runnable> HELP_BUTTON_CALLBACK =
            new ReadableObjectPropertyKey<>("help button callback");

    static final PropertyKey[] ALL_KEYS = {
        TITLE, DETAILS, HELP_BUTTON_VISIBILITY, HELP_BUTTON_CALLBACK
    };
}
