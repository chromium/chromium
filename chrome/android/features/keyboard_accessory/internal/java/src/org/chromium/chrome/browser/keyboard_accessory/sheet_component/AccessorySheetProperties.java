// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_component;

import android.support.v4.view.ViewPager;

import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Tab;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * This model holds all view state of the accessory sheet.
 * It is updated by the {@link AccessorySheetMediator} and emits notification on which observers
 * like the view binder react.
 */
class AccessorySheetProperties {
    static final ReadableObjectPropertyKey<ListModel<Tab>> TABS = new ReadableObjectPropertyKey<>();
    static final WritableIntPropertyKey ACTIVE_TAB_INDEX =
            new WritableIntPropertyKey("active_tab_index");
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey("visible");
    static final WritableIntPropertyKey HEIGHT = new WritableIntPropertyKey("height");
    static final WritableBooleanPropertyKey TOP_SHADOW_VISIBLE =
            new WritableBooleanPropertyKey("top_shadow_visible");
    static final WritableObjectPropertyKey<ViewPager.OnPageChangeListener> PAGE_CHANGE_LISTENER =
            new WritableObjectPropertyKey<>("page_change_listener");

    static final int NO_ACTIVE_TAB = -1;

    private AccessorySheetProperties() {}
}
