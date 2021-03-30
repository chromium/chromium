// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.graphics.Bitmap;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Contains all the properties for app management menu header. */
class AppManagementMenuHeaderProperties {
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<CharSequence> URL =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Bitmap> ICON = new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {TITLE, URL, ICON};

    /** Create the {@link PropertyModel} for menu header. */
    public static PropertyModel buildHeader(LaunchpadItem item) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(TITLE, item.name)
                .with(URL, item.url)
                .with(ICON, item.icon)
                .build();
    }
}
