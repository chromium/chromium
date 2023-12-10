// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import org.chromium.chrome.browser.password_entry_edit.CredentialEntryFragmentViewBase.UiActionHandler;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties defined here reflect the visible state of the credential edit UI. */
class CredentialEditProperties {
    static final PropertyModel.WritableObjectPropertyKey<UiActionHandler> UI_ACTION_HANDLER =
            new PropertyModel.WritableObjectPropertyKey<>("ui action handler");
    static final PropertyModel.ReadableObjectPropertyKey<String> URL_OR_APP =
            new PropertyModel.ReadableObjectPropertyKey<>("url or app");
    static final PropertyModel.WritableObjectPropertyKey<String> USERNAME =
            new PropertyModel.WritableObjectPropertyKey<>("username");
    static final PropertyModel.WritableBooleanPropertyKey DUPLICATE_USERNAME_ERROR =
            new PropertyModel.WritableBooleanPropertyKey("duplicate username error");
    static final PropertyModel.WritableBooleanPropertyKey PASSWORD_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey("password visible");
    static final PropertyModel.WritableObjectPropertyKey<String> PASSWORD =
            new PropertyModel.WritableObjectPropertyKey<>("password");
    static final PropertyModel.WritableBooleanPropertyKey EMPTY_PASSWORD_ERROR =
            new PropertyModel.WritableBooleanPropertyKey("empty password error");
    static final PropertyModel.ReadableObjectPropertyKey<String> FEDERATION_ORIGIN =
            new PropertyModel.ReadableObjectPropertyKey<>("federation origin");

    static final PropertyModel.WritableBooleanPropertyKey UI_DISMISSED_BY_NATIVE =
            new PropertyModel.WritableBooleanPropertyKey("ui dismissed by native");

    static final PropertyKey[] ALL_KEYS = {
        UI_ACTION_HANDLER,
        URL_OR_APP,
        USERNAME,
        DUPLICATE_USERNAME_ERROR,
        PASSWORD_VISIBLE,
        PASSWORD,
        EMPTY_PASSWORD_ERROR,
        FEDERATION_ORIGIN,
        UI_DISMISSED_BY_NATIVE
    };

    private CredentialEditProperties() {}
}
