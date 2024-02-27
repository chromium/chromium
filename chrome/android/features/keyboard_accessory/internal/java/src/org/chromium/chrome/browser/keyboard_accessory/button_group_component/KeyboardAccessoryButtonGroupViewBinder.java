// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.button_group_component;

import static org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupProperties.ACTIVE_TAB;
import static org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupProperties.BUTTON_SELECTION_CALLBACKS;
import static org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupProperties.TABS;

import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Stateless {@link ListModelChangeProcessor.ViewBinder} that binds a {@link ListModel}'s data to
 * a {@link KeyboardAccessoryButtonGroupView}.
 */
public class KeyboardAccessoryButtonGroupViewBinder
        implements ListModelChangeProcessor.ViewBinder<
                ListModel<KeyboardAccessoryData.Tab>, KeyboardAccessoryButtonGroupView, Void> {
    @Override
    public void onItemsInserted(
            ListModel<KeyboardAccessoryData.Tab> model,
            KeyboardAccessoryButtonGroupView view,
            int index,
            int count) {
        // More fine-grained implementations showed artifacts when adding in quick succession.
        updateAllButtons(view, model);
    }

    @Override
    public void onItemsRemoved(
            ListModel<KeyboardAccessoryData.Tab> model,
            KeyboardAccessoryButtonGroupView view,
            int index,
            int count) {
        // More fine-grained implementations showed artifacts when removing in quick succession.
        updateAllButtons(view, model);
    }

    @Override
    public void onItemsChanged(
            ListModel<KeyboardAccessoryData.Tab> model,
            KeyboardAccessoryButtonGroupView view,
            int index,
            int count,
            Void payload) {
        updateAllButtons(view, model);
    }

    private void updateAllButtons(
            KeyboardAccessoryButtonGroupView view, ListModel<KeyboardAccessoryData.Tab> model) {
        view.removeAllButtons();
        if (model.size() <= 0) return;
        for (int i = 0; i < model.size(); i++) {
            KeyboardAccessoryData.Tab tab = model.get(i);
            view.addButton(tab.getIcon(), tab.getContentDescription());
        }
    }

    private void registerTabIconObservers(
            KeyboardAccessoryButtonGroupView view, ListModel<KeyboardAccessoryData.Tab> model) {
        for (int i = 0; i < model.size(); i++) {
            final int observedIconIndex = i;
            model.get(i)
                    .addIconObserver(
                            (unusedTypeId, unusedDrawable) -> {
                                onItemsChanged(model, view, observedIconIndex, 1, null);
                            });
        }
    }

    protected static void bind(
            PropertyModel model, KeyboardAccessoryButtonGroupView view, PropertyKey propertyKey) {
        if (propertyKey == TABS) {
            KeyboardAccessoryButtonGroupViewBinder viewBinder =
                    KeyboardAccessoryButtonGroupCoordinator.createButtonGroupViewBinder(model, view);
            viewBinder.updateAllButtons(view, model.get(TABS));
            viewBinder.registerTabIconObservers(view, model.get(TABS));
        } else if (propertyKey == BUTTON_SELECTION_CALLBACKS) {
            KeyboardAccessoryButtonGroupView.KeyboardAccessoryButtonGroupListener listener =
                    model.get(BUTTON_SELECTION_CALLBACKS);
            if (listener != null) view.setButtonSelectionListener(listener);
        } else if (propertyKey == ACTIVE_TAB) {
            // not used for this view.
        } else {
            assert false : "Every possible property update needs to be handled!";
        }
    }
}
