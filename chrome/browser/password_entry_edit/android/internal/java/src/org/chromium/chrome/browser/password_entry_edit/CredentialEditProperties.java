// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Properties defined here reflect the visible state of the credential edit UI.
 */
class CredentialEditProperties {
    static final PropertyModel.ReadableObjectPropertyKey<String> URL_OR_APP =
            new PropertyModel.ReadableObjectPropertyKey<>("url or app");
    static final PropertyModel.WritableObjectPropertyKey<String> USERNAME =
            new PropertyModel.WritableObjectPropertyKey<>("username");
    static final PropertyModel.WritableBooleanPropertyKey PASSWORD_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey("password visible");
    static final PropertyModel.WritableObjectPropertyKey<String> PASSWORD =
            new PropertyModel.WritableObjectPropertyKey<>("password");
    static final PropertyModel.ReadableObjectPropertyKey<String> FEDERATION_ORIGIN =
            new PropertyModel.ReadableObjectPropertyKey<>("federation origin");

    static final PropertyModel.WritableBooleanPropertyKey UI_DISMISSED_BY_NATIVE =
            new PropertyModel.WritableBooleanPropertyKey("ui dismissed by native");

    static final PropertyKey[] ALL_KEYS = {URL_OR_APP, USERNAME, PASSWORD_VISIBLE, PASSWORD,
            FEDERATION_ORIGIN, UI_DISMISSED_BY_NATIVE};

    private CredentialEditProperties() {}
}
