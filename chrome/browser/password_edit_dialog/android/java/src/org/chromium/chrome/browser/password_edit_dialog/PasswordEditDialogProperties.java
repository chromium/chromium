// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Defines properties for password edit dialog custom view.
 */
class PasswordEditDialogProperties {
    /**
     * The callback, invoked when the user selects a username. The value is 0 based index of
     * selected username.
     */
    static final PropertyModel
            .ReadableObjectPropertyKey<Callback<Integer>> USERNAME_SELECTED_CALLBACK =
            new PropertyModel.ReadableObjectPropertyKey<>("username selected callback");

    static final PropertyModel.ReadableObjectPropertyKey<List<String>> USERNAMES =
            new PropertyModel.ReadableObjectPropertyKey<>("usernames");

    static final PropertyModel.WritableIntPropertyKey SELECTED_USERNAME_INDEX =
            new PropertyModel.WritableIntPropertyKey("selected username index");

    static final PropertyModel.WritableObjectPropertyKey<String> PASSWORD =
            new PropertyModel.WritableObjectPropertyKey<>("password");

    static final PropertyModel.ReadableObjectPropertyKey<String> FOOTER =
            new PropertyModel.ReadableObjectPropertyKey<>("footer");

    static final PropertyModel
            .ReadableObjectPropertyKey<Callback<String>> PASSWORD_CHANGED_CALLBACK =
            new PropertyModel.ReadableObjectPropertyKey<>("password changed callback");

    static final PropertyModel.WritableBooleanPropertyKey EMPTY_PASSWORD_ERROR =
            new PropertyModel.WritableBooleanPropertyKey("empty password error");

    static final PropertyKey[] ALL_KEYS = {USERNAME_SELECTED_CALLBACK, USERNAMES,
            SELECTED_USERNAME_INDEX, PASSWORD, PASSWORD_CHANGED_CALLBACK, EMPTY_PASSWORD_ERROR,
            FOOTER};
}