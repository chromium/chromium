// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search.module;

import android.view.View.OnClickListener;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the auxiliary search module. */
@NullMarked
interface AuxiliarySearchModuleProperties {
    WritableObjectPropertyKey<OnClickListener> MODULE_FIRST_BUTTON_ON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<OnClickListener> MODULE_SECOND_BUTTON_ON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    WritableIntPropertyKey MODULE_CONTENT_TEXT_RES_ID = new WritableIntPropertyKey();
    WritableIntPropertyKey MODULE_FIRST_BUTTON_TEXT_RES_ID = new WritableIntPropertyKey();
    WritableIntPropertyKey MODULE_SECOND_BUTTON_TEXT_RES_ID = new WritableIntPropertyKey();

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                MODULE_FIRST_BUTTON_ON_CLICK_LISTENER,
                MODULE_SECOND_BUTTON_ON_CLICK_LISTENER,
                MODULE_CONTENT_TEXT_RES_ID,
                MODULE_FIRST_BUTTON_TEXT_RES_ID,
                MODULE_SECOND_BUTTON_TEXT_RES_ID
            };
}
