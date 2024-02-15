// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Defines properties for password edit dialog custom view. */
class PasswordEditDialogProperties {
    static final PropertyModel.ReadableObjectPropertyKey<List<String>> USERNAMES =
            new PropertyModel.ReadableObjectPropertyKey<>("usernames");

    static final PropertyModel.WritableObjectPropertyKey<String> USERNAME =
            new PropertyModel.WritableObjectPropertyKey<>("username");

    /** The callback, invoked when the user edits the username. */
    static final PropertyModel.ReadableObjectPropertyKey<Callback<String>>
            USERNAME_CHANGED_CALLBACK =
                    new PropertyModel.ReadableObjectPropertyKey<>("username changed callback");

    static final PropertyModel.WritableObjectPropertyKey<String> PASSWORD =
            new PropertyModel.WritableObjectPropertyKey<>("password");

    static final PropertyModel.WritableObjectPropertyKey<String> FOOTER =
            new PropertyModel.WritableObjectPropertyKey<>("footer");

    static final PropertyModel.ReadableObjectPropertyKey<Callback<String>>
            PASSWORD_CHANGED_CALLBACK =
                    new PropertyModel.ReadableObjectPropertyKey<>("password changed callback");

    static final PropertyModel.WritableObjectPropertyKey<String> PASSWORD_ERROR =
            new PropertyModel.WritableObjectPropertyKey<>("empty password error");

    static final PropertyKey[] ALL_KEYS = {
        USERNAMES,
        USERNAME,
        USERNAME_CHANGED_CALLBACK,
        PASSWORD,
        PASSWORD_CHANGED_CALLBACK,
        PASSWORD_ERROR,
        FOOTER
    };
}
