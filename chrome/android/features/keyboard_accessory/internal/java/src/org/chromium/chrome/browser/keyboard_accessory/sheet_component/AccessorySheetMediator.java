// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_component;

import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.ACTIVE_TAB_INDEX;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.NO_ACTIVE_TAB;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.PAGE_CHANGE_LISTENER;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.TABS;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.TOP_SHADOW_VISIBLE;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.VISIBLE;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.viewpager.widget.ViewPager;

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
