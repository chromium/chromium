// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** List of properties to designate information about module in Safety Hub. */
public class SafetyHubModuleProperties {
    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<Runnable> ON_CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableIntPropertyKey COMPROMISED_PASSWORDS_COUNT =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<UpdateStatusProvider.UpdateStatus>
            UPDATE_STATUS = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableIntPropertyKey SITES_WITH_UNUSED_PERMISSIONS_COUNT =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel.WritableIntPropertyKey
            NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT = new PropertyModel.WritableIntPropertyKey();

    public static final PropertyKey[] COMMON_SAFETY_HUB_MODULE_KEYS = {
        IS_VISIBLE, ON_CLICK_LISTENER
    };

    public static final PropertyKey[] PASSWORD_CHECK_SAFETY_HUB_MODULE_KEYS =
            PropertyModel.concatKeys(
                    COMMON_SAFETY_HUB_MODULE_KEYS, new PropertyKey[] {COMPROMISED_PASSWORDS_COUNT});

    public static final PropertyKey[] UPDATE_CHECK_SAFETY_HUB_MODULE_KEYS =
            PropertyModel.concatKeys(
                    COMMON_SAFETY_HUB_MODULE_KEYS, new PropertyKey[] {UPDATE_STATUS});

    public static final PropertyKey[] PERMISSIONS_MODULE_KEYS =
            PropertyModel.concatKeys(
                    COMMON_SAFETY_HUB_MODULE_KEYS,
                    new PropertyKey[] {SITES_WITH_UNUSED_PERMISSIONS_COUNT});

    public static final PropertyKey[] NOTIFICATIONS_REVIEW_MODULE_KEYS =
            PropertyModel.concatKeys(
                    COMMON_SAFETY_HUB_MODULE_KEYS,
                    new PropertyKey[] {NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT});
}
