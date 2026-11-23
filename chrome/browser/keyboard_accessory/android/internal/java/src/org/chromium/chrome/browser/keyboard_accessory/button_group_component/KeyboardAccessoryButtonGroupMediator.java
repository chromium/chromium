// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.button_group_component;

import static org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupProperties.ACTIVE_TAB;
import static org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupProperties.BUTTON_SELECTION_CALLBACKS;
import static org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupProperties.SELECTED_BUTTON;
import static org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupProperties.TABS;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupCoordinator.AccessoryTabObserver;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;

/**
 * This mediator observes and changes a {@link PropertyModel} that contains the visual appearance of
 * a {@link KeyboardAccessoryButtonGroupView}. It manages the visible sheet openers.
 */
@NullMarked
class KeyboardAccessoryButtonGroupMediator
        implements KeyboardAccessoryButtonGroupView.KeyboardAccessoryButtonGroupListener,
                PropertyObservable.PropertyObserver<PropertyKey>,
                KeyboardAccessoryCoordinator.TabSwitchingDelegate {
    private final PropertyModel mModel;
    private @Nullable AccessoryTabObserver mAccessoryTabObserver;

    KeyboardAccessoryButtonGroupMediator(PropertyModel model) {
        mModel = model;
        mModel.addObserver(this);
        mModel.set(BUTTON_SELECTION_CALLBACKS, this);
    }

    @Override
    public void onPropertyChanged(
            PropertyObservable<PropertyKey> source, @Nullable PropertyKey propertyKey) {
        if (propertyKey == ACTIVE_TAB) {
            if (mAccessoryTabObserver != null) {
                mAccessoryTabObserver.onActiveTabChanged(mModel.get(ACTIVE_TAB));
            }
            return;
        }
        if (propertyKey == TABS) {
            closeActiveTab(); // Make sure the active tab is reset for a modified tab list.
            return;
        }
        if (propertyKey == BUTTON_SELECTION_CALLBACKS || propertyKey == SELECTED_BUTTON) {
            return;
        }
        assert false : "Every property update needs to be handled explicitly!";
    }

    @Override
    public void setTabs(KeyboardAccessoryData.Tab[] tabs) {
        mModel.set(SELECTED_BUTTON, null);
        mModel.get(TABS).set(tabs);
    }

    @Override
    public void closeActiveTab() {
        mModel.set(ACTIVE_TAB, null);
    }

    @Override
    public void setActiveTab(@AccessoryTabType int tabType) {
        ListModel<KeyboardAccessoryData.Tab> tabs = mModel.get(TABS);
        int tabPosition = 0;
        while (tabPosition < tabs.size()) {
            if (tabs.get(tabPosition).getRecordingType() == tabType) {
                break;
            }
            tabPosition++;
        }
        // TODO(crbug.com/430575808): Add logging here to check how many times we would reach this
        // condition in production, since asserts are removed in production. If this is still a
        // recurring issue, we can potentially look into making a synchronous version of
        // `ManualFillingViewAndroid::OnItemsAvailable` (http://shortn/_6mwstKP5Xs).
        assert tabPosition < tabs.size() : "No tab found for the given tabType: " + tabType;
        mModel.set(ACTIVE_TAB, tabPosition);
    }

    @Override
    public KeyboardAccessoryData.@Nullable Tab getActiveTab() {
        if (mModel.get(ACTIVE_TAB) == null) return null;
        return mModel.get(TABS).get(mModel.get(ACTIVE_TAB));
    }

    @Override
    public boolean hasTabs() {
        return mModel.get(TABS).size() > 0;
    }

    @Override
    public int getButtonCount() {
        return mModel.get(TABS).size();
    }

    @Override
    public @Nullable Integer getSelectedButton() {
        return mModel.get(SELECTED_BUTTON);
    }

    @Override
    public void setSelectedButton(@Nullable Integer buttonIndex) {
        mModel.set(SELECTED_BUTTON, buttonIndex);
    }

    @Override
    public boolean clickSelectedButton() {
        Integer selectedButtonIndex = mModel.get(SELECTED_BUTTON);
        if (selectedButtonIndex == null) return false;
        onButtonClicked(selectedButtonIndex);
        return true;
    }

    @Override
    public void onButtonClicked(int position) {
        mModel.set(SELECTED_BUTTON, null);
        mModel.set(ACTIVE_TAB, position >= mModel.get(TABS).size() ? null : position);
    }

    void setTabObserver(AccessoryTabObserver accessoryTabObserver) {
        mAccessoryTabObserver = accessoryTabObserver;
    }
}
