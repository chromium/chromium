// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.button_group_component;

import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * These properties are used to describe a model for the tab layout component as used in the
 * keyboard accessory. The properties are describing all known tabs.
 */
class KeyboardAccessoryButtonGroupProperties {
    static final ReadableObjectPropertyKey<ListModel<KeyboardAccessoryData.Tab>> TABS =
            new ReadableObjectPropertyKey<>("tabs");
    static final WritableObjectPropertyKey<Integer> ACTIVE_TAB =
            new WritableObjectPropertyKey<>("active_tab");
    static final WritableObjectPropertyKey<
                    KeyboardAccessoryButtonGroupView.KeyboardAccessoryButtonGroupListener>
            BUTTON_SELECTION_CALLBACKS =
                    new WritableObjectPropertyKey<>("button_selection_callback");

    private KeyboardAccessoryButtonGroupProperties() {}
}
