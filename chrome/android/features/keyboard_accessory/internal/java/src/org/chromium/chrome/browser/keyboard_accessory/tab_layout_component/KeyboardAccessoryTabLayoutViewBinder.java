// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.tab_layout_component;

import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutProperties.ACTIVE_TAB;
import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutProperties.TABS;
import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutProperties.TAB_SELECTION_CALLBACKS;

import android.support.design.widget.TabLayout;

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Stateless {@link ListModelChangeProcessor.ViewBinder} that binds a {@link ListModel}'s data to
 * a {@link KeyboardAccessoryTabLayoutView}.
 */
class KeyboardAccessoryTabLayoutViewBinder
        implements ListModelChangeProcessor.ViewBinder<ListModel<KeyboardAccessoryData.Tab>,
                KeyboardAccessoryTabLayoutView> {
    @Override
    public void onItemsInserted(ListModel<KeyboardAccessoryData.Tab> model,
            KeyboardAccessoryTabLayoutView view, int index, int count) {
        // More fine-grained implementations showed artifacts when adding in quick succession.
        updateAllTabs(view, model);
    }

    @Override
    public void onItemsRemoved(ListModel<KeyboardAccessoryData.Tab> model,
            KeyboardAccessoryTabLayoutView view, int index, int count) {
        // More fine-grained implementations showed artifacts when removing in quick succession.
        updateAllTabs(view, model);
    }

    @Override
    public void onItemsChanged(ListModel<KeyboardAccessoryData.Tab> model,
            KeyboardAccessoryTabLayoutView view, int index, int count) {
        updateAllTabs(view, model);
    }

    private void updateAllTabs(
            KeyboardAccessoryTabLayoutView view, ListModel<KeyboardAccessoryData.Tab> model) {
        view.removeAllTabs();
        if (model.size() <= 0) return;
        for (int i = 0; i < model.size(); i++) {
            KeyboardAccessoryData.Tab tab = model.get(i);
            view.addTabAt(i, tab.getIcon(), tab.getContentDescription());
        }
    }

    protected static void bind(
            PropertyModel model, KeyboardAccessoryTabLayoutView view, PropertyKey propertyKey) {
        if (propertyKey == TABS) {
            KeyboardAccessoryTabLayoutCoordinator.createTabViewBinder(model, view)
                    .updateAllTabs(view, model.get(TABS));
        } else if (propertyKey == ACTIVE_TAB) {
            view.setActiveTabColor(model.get(ACTIVE_TAB));
            setActiveTabHint(model, view);
        } else if (propertyKey == TAB_SELECTION_CALLBACKS) {
            // Don't add null as listener. It's a valid state but an invalid argument.
            TabLayout.OnTabSelectedListener listener = model.get(TAB_SELECTION_CALLBACKS);
            if (listener != null) view.setTabSelectionAdapter(listener);
        } else {
            assert false : "Every possible property update needs to be handled!";
        }
    }

    private static void setActiveTabHint(PropertyModel model, KeyboardAccessoryTabLayoutView view) {
        int activeTab = -1;
        if (model.get(ACTIVE_TAB) != null) {
            activeTab = model.get(ACTIVE_TAB);
        }
        for (int i = 0; i < model.get(TABS).size(); ++i) {
            KeyboardAccessoryData.Tab tab = model.get(TABS).get(i);
            if (activeTab == i) {
                view.setTabDescription(i, R.string.keyboard_accessory_sheet_hide);
            } else {
                view.setTabDescription(i, tab.getContentDescription());
            }
        }
    }
}
