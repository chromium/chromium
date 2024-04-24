// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** List of properties to designate information about module in Safety Hub. */
public class SafetyHubModuleProperties {

    public static final PropertyModel.ReadableIntPropertyKey ICON =
            new PropertyModel.ReadableIntPropertyKey();

    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<Runnable> ON_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableIntPropertyKey COMPROMISED_PASSWORDS_COUNT =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyKey[] COMMON_SAFETY_HUB_MODULE_KEYS = {
        ICON, IS_VISIBLE, ON_CLICK_LISTENER
    };

    public static final PropertyKey[] PASSWORD_CHECK_SAFETY_HUB_MODULE_KEYS =
            PropertyModel.concatKeys(
                    COMMON_SAFETY_HUB_MODULE_KEYS, new PropertyKey[] {COMPROMISED_PASSWORDS_COUNT});
}
