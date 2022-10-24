// Copyright 2021 The Chromium Authors
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
    static final PropertyModel.ReadableObjectPropertyKey<List<String>> USERNAMES =
            new PropertyModel.ReadableObjectPropertyKey<>("usernames");

    // Used only when PasswordEditDialogWithDetails feature is on
    static final PropertyModel.WritableObjectPropertyKey<String> USERNAME =
            new PropertyModel.WritableObjectPropertyKey<>("username");

    // Used only when PasswordEditDialogWithDetails feature is off
    static final PropertyModel.WritableIntPropertyKey USERNAME_INDEX =
            new PropertyModel.WritableIntPropertyKey("username index");
    /**
     * The callback, invoked when the user edits the username
     * Used only when PasswordEditDialogWithDetails feature is on
     */
    static final PropertyModel
            .ReadableObjectPropertyKey<Callback<String>> USERNAME_CHANGED_CALLBACK =
            new PropertyModel.ReadableObjectPropertyKey<>("username changed callback");

    /**
     * The callback, invoked when the user selects a username. The value is 0 based index of
     * selected username.
     * Used only when PasswordEditDialogWithDetails feature is off
     */
    static final PropertyModel
            .ReadableObjectPropertyKey<Callback<Integer>> USERNAME_SELECTED_CALLBACK =
            new PropertyModel.ReadableObjectPropertyKey<>("username selected callback");

    static final PropertyModel.WritableObjectPropertyKey<String> PASSWORD =
            new PropertyModel.WritableObjectPropertyKey<>("password");

    static final PropertyModel.ReadableObjectPropertyKey<String> FOOTER =
            new PropertyModel.ReadableObjectPropertyKey<>("footer");

    static final PropertyModel
            .ReadableObjectPropertyKey<Callback<String>> PASSWORD_CHANGED_CALLBACK =
            new PropertyModel.ReadableObjectPropertyKey<>("password changed callback");

    static final PropertyModel.WritableObjectPropertyKey<String> PASSWORD_ERROR =
            new PropertyModel.WritableObjectPropertyKey<>("empty password error");

    static final PropertyKey[] ALL_KEYS = {USERNAMES, USERNAME, USERNAME_INDEX,
            USERNAME_CHANGED_CALLBACK, USERNAME_SELECTED_CALLBACK, PASSWORD,
            PASSWORD_CHANGED_CALLBACK, PASSWORD_ERROR, FOOTER};
}