// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_component;

import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.ACTIVE_TAB_INDEX;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.HEIGHT;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.NO_ACTIVE_TAB;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.PAGE_CHANGE_LISTENER;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.SHOW_KEYBOARD_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.TABS;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.TOP_SHADOW_VISIBLE;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.VISIBLE;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Observes {@link AccessorySheetProperties} changes (like a newly available tab) and triggers the
 * {@link AccessorySheetViewBinder} which will modify the view accordingly.
 */
class AccessorySheetViewBinder {
    static void bind(PropertyModel model, View sheetView, PropertyKey propertyKey) {
        AccessorySheetView view = (AccessorySheetView) sheetView;
        if (propertyKey == TABS) {
            view.setAdapter(
                    AccessorySheetCoordinator.createTabViewAdapter(
                            model.get(TABS), view.getViewPager()));
        } else if (propertyKey == VISIBLE) {
            view.bringToFront(); // Ensure toolbars and other containers are overlaid.
            view.setVisibility(model.get(VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (propertyKey == HEIGHT) {
            ViewGroup.LayoutParams p = view.getLayoutParams();
            p.height = model.get(HEIGHT);
            view.setLayoutParams(p);
            view.setFrameHeight(model.get(HEIGHT));
        } else if (propertyKey == TOP_SHADOW_VISIBLE) {
            view.setTopShadowVisible(model.get(TOP_SHADOW_VISIBLE));
        } else if (propertyKey == ACTIVE_TAB_INDEX) {
            if (model.get(ACTIVE_TAB_INDEX) != NO_ACTIVE_TAB) {
                view.setCurrentItem(model.get(ACTIVE_TAB_INDEX));
                view.setTitle(model.get(TABS).get(model.get(ACTIVE_TAB_INDEX)).getTitle());
            }
        } else if (propertyKey == PAGE_CHANGE_LISTENER) {
            if (model.get(PAGE_CHANGE_LISTENER) != null) {
                view.addOnPageChangeListener(model.get(PAGE_CHANGE_LISTENER));
            }
        } else if (propertyKey == SHOW_KEYBOARD_CALLBACK) {
            view.setShowKeyboardCallback(model.get(SHOW_KEYBOARD_CALLBACK));
        } else {
            assert false : "Every possible property update needs to be handled!";
        }
    }
}
