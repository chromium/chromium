// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_component;

import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.ACTIVE_TAB_INDEX;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.NO_ACTIVE_TAB;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.PAGE_CHANGE_LISTENER;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.TABS;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.TOP_SHADOW_VISIBLE;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.VISIBLE;

import android.support.v4.view.ViewPager;
import android.support.v7.widget.RecyclerView;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;

/**
 * Contains the controller logic of the AccessorySheet component.
 * It communicates with data providers and native backends to update a model based on {@link
 * AccessorySheetProperties}.
 */
class AccessorySheetMediator implements PropertyObservable.PropertyObserver<PropertyKey> {
    private final PropertyModel mModel;
    private final RecyclerView.OnScrollListener mScrollListener =
            new RecyclerView.OnScrollListener() {
                @Override
                public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                    if (recyclerView == null) return;
                    mModel.set(TOP_SHADOW_VISIBLE, recyclerView.canScrollVertically(-1));
                }
            };

    AccessorySheetMediator(PropertyModel model) {
        mModel = model;
        mModel.addObserver(this);
    }

    @Nullable
    KeyboardAccessoryData.Tab getTab() {
        if (mModel.get(ACTIVE_TAB_INDEX) == NO_ACTIVE_TAB) return null;
        return mModel.get(TABS).get(mModel.get(ACTIVE_TAB_INDEX));
    }

    @VisibleForTesting
    PropertyModel getModelForTesting() {
        return mModel;
    }

    void show() {
        mModel.set(VISIBLE, true);
    }

    void setHeight(int height) {
        mModel.set(AccessorySheetProperties.HEIGHT, height);
    }

    @Px
    int getHeight() {
        return mModel.get(AccessorySheetProperties.HEIGHT);
    }

    void hide() {
        mModel.set(VISIBLE, false);
    }

    boolean isShown() {
        return mModel.get(VISIBLE);
    }

    RecyclerView.OnScrollListener getScrollListener() {
        return mScrollListener;
    }

    void setTabs(KeyboardAccessoryData.Tab[] tabs) {
        mModel.get(TABS).set(tabs);
        mModel.set(ACTIVE_TAB_INDEX, mModel.get(TABS).size() - 1);
    }

    void setActiveTab(int position) {
        assert position < mModel.get(TABS).size()
                || position >= 0 : position + " is not a valid tab index!";
        mModel.set(ACTIVE_TAB_INDEX, position);
    }

    /**
     * Returns the position of a tab which needs to become the active tab. If the tab to be deleted
     * is the active tab, return the item on its left. If it was the first item in the list, return
     * the new first item. If no items remain, return {@link
     * AccessorySheetProperties#NO_ACTIVE_TAB}.
     * @param tabToBeDeleted The tab to be removed from the list.
     * @return The position of the tab which should become active.
     */
    private int getNextActiveTab(KeyboardAccessoryData.Tab tabToBeDeleted) {
        int activeTab = mModel.get(ACTIVE_TAB_INDEX);
        for (int i = 0; i <= activeTab; i++) {
            KeyboardAccessoryData.Tab tabLeftToActiveTab = mModel.get(TABS).get(i);
            // If we delete the active tab or a tab left to it, the new active tab moves left.
            if (tabLeftToActiveTab == tabToBeDeleted) {
                --activeTab;
                break;
            }
        }
        if (activeTab >= 0) return activeTab; // The new active tab is valid.
        // If there are items left, take the first one.
        int itemCountAfterDeletion = mModel.get(TABS).size() - 1;
        return itemCountAfterDeletion > 0 ? 0 : NO_ACTIVE_TAB;
    }

    @Override
    public void onPropertyChanged(PropertyObservable<PropertyKey> source, PropertyKey propertyKey) {
        if (propertyKey == VISIBLE) {
            if (mModel.get(VISIBLE) && getTab() != null && getTab().getListener() != null) {
                getTab().getListener().onTabShown();
            }
            return;
        }
        if (propertyKey == ACTIVE_TAB_INDEX || propertyKey == AccessorySheetProperties.HEIGHT
                || propertyKey == TOP_SHADOW_VISIBLE || propertyKey == PAGE_CHANGE_LISTENER) {
            return;
        }
        assert false : "Every property update needs to be handled explicitly!";
    }

    void setOnPageChangeListener(ViewPager.OnPageChangeListener onPageChangeListener) {
        mModel.set(PAGE_CHANGE_LISTENER, onPageChangeListener);
    }
}
