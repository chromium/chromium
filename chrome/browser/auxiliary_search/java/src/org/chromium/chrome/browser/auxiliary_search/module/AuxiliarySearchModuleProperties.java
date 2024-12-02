// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search.module;

import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the auxiliary search module. */
interface AuxiliarySearchModuleProperties {
    WritableObjectPropertyKey<OnClickListener> MODULE_FIRST_BUTTON_ON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<OnClickListener> MODULE_SECOND_BUTTON_ON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                MODULE_FIRST_BUTTON_ON_CLICK_LISTENER, MODULE_SECOND_BUTTON_ON_CLICK_LISTENER
            };
}
