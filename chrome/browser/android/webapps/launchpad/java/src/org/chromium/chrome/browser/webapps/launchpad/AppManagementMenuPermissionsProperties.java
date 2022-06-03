// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Properties for Launchpad app management menu's app permissions.
 */
class AppManagementMenuPermissionsProperties {
    private AppManagementMenuPermissionsProperties() {}

    public static final WritableIntPropertyKey NOTIFICATIONS = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey MIC = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey CAMERA = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey LOCATION = new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<
            AppManagementMenuPermissionsView.OnButtonClickListener> ON_CLICK =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {NOTIFICATIONS, MIC, CAMERA, LOCATION, ON_CLICK};
}
