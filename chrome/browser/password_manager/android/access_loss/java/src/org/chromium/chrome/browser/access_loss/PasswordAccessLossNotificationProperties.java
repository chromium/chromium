// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties defined here reflect the visible state of the password access loss notification. */
class PasswordAccessLossNotificationProperties {
    public static final PropertyModel.WritableObjectPropertyKey<String> TITLE =
            new PropertyModel.WritableObjectPropertyKey<>("sheet_title");
    public static final PropertyModel.WritableObjectPropertyKey<String> TEXT =
            new PropertyModel.WritableObjectPropertyKey<>("sheet_text");

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {TITLE, TEXT};

    private PasswordAccessLossNotificationProperties() {}
}
