// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.add_username_dialog;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class AddUsernameDialogContentProperties {
    static final PropertyModel.WritableObjectPropertyKey<String> USERNAME =
            new PropertyModel.WritableObjectPropertyKey<>("username");
    static final PropertyModel.ReadableObjectPropertyKey<Callback<String>>
            USERNAME_CHANGED_CALLBACK =
                    new PropertyModel.ReadableObjectPropertyKey<>("username_changed_callback");
    static final PropertyModel.ReadableObjectPropertyKey<String> PASSWORD =
            new PropertyModel.ReadableObjectPropertyKey<>("password");

    static final PropertyKey[] ALL_KEYS = {USERNAME, USERNAME_CHANGED_CALLBACK, PASSWORD};

    private AddUsernameDialogContentProperties() {}
}
