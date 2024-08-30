// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the educational tip module. */
interface EducationalTipModuleProperties {
    WritableObjectPropertyKey<String> MODULE_CONTENT_TITLE_STRING =
            new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<String> MODULE_CONTENT_DESCRIPTION_STRING =
            new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<OnClickListener> MODULE_BUTTON_ON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    WritableIntPropertyKey MODULE_CONTENT_IMAGE = new WritableIntPropertyKey();

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                MODULE_CONTENT_TITLE_STRING,
                MODULE_CONTENT_DESCRIPTION_STRING,
                MODULE_BUTTON_ON_CLICK_LISTENER,
                MODULE_CONTENT_IMAGE
            };
}
