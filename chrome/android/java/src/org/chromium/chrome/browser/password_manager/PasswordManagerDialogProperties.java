// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/**
 * Data properties for the password manager illustration modal dialog.
 */
class PasswordManagerDialogProperties {
    // Callback handling clicks on the help button. If present, the button will be shown.
    static final ReadableObjectPropertyKey<Runnable> HELP_BUTTON_CALLBACK =
            new ReadableObjectPropertyKey<>();

    // Illustration drawable resource id for the password manager.
    static final ReadableIntPropertyKey ILLUSTRATION = new ReadableIntPropertyKey();

    // Boolean indicating whether there is enough space for the illustration to be shown.
    static final WritableBooleanPropertyKey ILLUSTRATION_VISIBLE = new WritableBooleanPropertyKey();

    // Title that appears below the illustration.
    static final ReadableObjectPropertyKey<String> TITLE = new ReadableObjectPropertyKey<>();

    // Multiline explanation text displayed under the illustration.
    static final ReadableObjectPropertyKey<CharSequence> DETAILS =
            new ReadableObjectPropertyKey<>();

    private PasswordManagerDialogProperties() {}

    static PropertyModel.Builder defaultModelBuilder() {
        return new PropertyModel.Builder(
                HELP_BUTTON_CALLBACK, ILLUSTRATION, ILLUSTRATION_VISIBLE, TITLE, DETAILS);
    }
}
