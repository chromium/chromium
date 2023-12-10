// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** These properties make up the model of the AccessorySheetTab component. */
class AccessorySheetTabProperties {
    static final ReadableObjectPropertyKey<AccessorySheetTabItemsModel> ITEMS =
            new ReadableObjectPropertyKey<>("items");
    static final ReadableObjectPropertyKey<RecyclerView.OnScrollListener> SCROLL_LISTENER =
            new ReadableObjectPropertyKey<>("scroll_listener");
    static final WritableObjectPropertyKey<Boolean> IS_DEFAULT_A11Y_FOCUS_REQUESTED =
            new WritableObjectPropertyKey<>("is_default_a11y_focus_requested");

    static final PropertyKey[] ALL_KEYS = {ITEMS, SCROLL_LISTENER, IS_DEFAULT_A11Y_FOCUS_REQUESTED};

    private AccessorySheetTabProperties() {}
}
