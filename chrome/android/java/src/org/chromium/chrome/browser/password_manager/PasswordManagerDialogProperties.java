// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Data properties for the password manager illustration modal dialog.
 */
class PasswordManagerDialogProperties {
    // Illustration drawable resource id for the password manager.
    static final WritableIntPropertyKey ILLUSTRATION = new WritableIntPropertyKey();

    // Boolean indicating whether there is enough space for the illustration to be shown.
    static final WritableBooleanPropertyKey ILLUSTRATION_VISIBLE = new WritableBooleanPropertyKey();

    // Title that appears below the illustration.
    static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    // Multiline explanation text displayed under the illustration.
    static final WritableObjectPropertyKey<CharSequence> DETAILS =
            new WritableObjectPropertyKey<>();

    private PasswordManagerDialogProperties() {}

    static PropertyModel.Builder defaultModelBuilder() {
        return new PropertyModel.Builder(ILLUSTRATION, ILLUSTRATION_VISIBLE, TITLE, DETAILS);
    }
}
