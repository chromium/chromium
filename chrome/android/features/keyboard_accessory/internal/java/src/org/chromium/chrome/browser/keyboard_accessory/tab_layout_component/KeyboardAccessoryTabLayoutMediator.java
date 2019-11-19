// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.tab_layout_component;

import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutProperties.ACTIVE_TAB;
import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutProperties.TABS;
import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutProperties.TAB_SELECTION_CALLBACKS;

import android.support.design.widget.TabLayout;
import android.support.v4.view.ViewPager;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutCoordinator.AccessoryTabObserver;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;

import java.util.HashSet;
import java.util.Set;

/**
 * This mediator observes and changes a {@link PropertyModel} that contains the visual appearance of
 * a {@link TabLayout}. It manages {@link ViewPager.OnPageChangeListener}s.
 */
class KeyboardAccessoryTabLayoutMediator
        implements TabLayout.OnTabSelectedListener,
                   PropertyObservable.PropertyObserver<PropertyKey>,
                   KeyboardAccessoryCoordinator.TabSwitchingDelegate {
    private final PropertyModel mModel;
    private @Nullable AccessoryTabObserver mAccessoryTabObserver;
    private Set<TabLayout.TabLayoutOnPageChangeListener> mPageChangeListeners = new HashSet<>();

    KeyboardAccessoryTabLayoutMediator(PropertyModel model) {
        mModel = model;
        mModel.addObserver(this);
        mModel.set(TAB_SELECTION_CALLBACKS, this);
    }

    ViewPager.OnPageChangeListener getStableOnPageChangeListener() {
        return new ViewPager.OnPageChangeListener() {
            @Override
            public void onPageScrolled(int i, float v, int j) {
                for (TabLayout.TabLayoutOnPageChangeListener listener : mPageChangeListeners) {
                    listener.onPageScrolled(i, v, j);
                }
            }

            @Override
            public void onPageSelected(int i) {
                for (TabLayout.TabLayoutOnPageChangeListener listener : mPageChangeListeners) {
                    listener.onPageSelected(i);
                }
            }

            @Override
            public void onPageScrollStateChanged(int i) {
                for (TabLayout.TabLayoutOnPageChangeListener listener : mPageChangeListeners) {
                    listener.onPageScrollStateChanged(i);
                }
            }
        };
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
        if (propertyKey == TAB_SELECTION_CALLBACKS) {
            return;
        }
        assert false : "Every property update needs to be handled explicitly!";
    }

    @Override
    public void addTab(KeyboardAccessoryData.Tab tab) {
        mModel.get(TABS).add(tab);
    }

    @Override
    public void removeTab(KeyboardAccessoryData.Tab tab) {
        mModel.get(TABS).remove(tab);
    }

    @Override
    public void setTabs(KeyboardAccessoryData.Tab[] tabs) {
        mModel.get(TABS).set(tabs);
    }

    @Override
    public void closeActiveTab() {
        mModel.set(ACTIVE_TAB, null);
    }

    @Override
    public @Nullable KeyboardAccessoryData.Tab getActiveTab() {
        if (mModel.get(ACTIVE_TAB) == null) return null;
        return mModel.get(TABS).get(mModel.get(ACTIVE_TAB));
    }

    @Override
    public boolean hasTabs() {
        return mModel.get(TABS).size() > 0;
    }

    @Override
    public void onTabSelected(TabLayout.Tab tab) {
        mModel.set(ACTIVE_TAB, validateActiveTab(tab.getPosition()));
    }

    @Override
    public void onTabUnselected(TabLayout.Tab tab) {}

    @Override
    public void onTabReselected(TabLayout.Tab tab) {
        if (mModel.get(ACTIVE_TAB) == null) {
            mModel.set(ACTIVE_TAB, validateActiveTab(tab.getPosition()));
        } else if (mAccessoryTabObserver != null) {
            mAccessoryTabObserver.onActiveTabReselected();
        }
    }

    void setTabObserver(AccessoryTabObserver accessoryTabObserver) {
        mAccessoryTabObserver = accessoryTabObserver;
    }

    void addPageChangeListener(TabLayout.TabLayoutOnPageChangeListener pageChangeListener) {
        mPageChangeListeners.add(pageChangeListener);
    }

    void removePageChangeListener(TabLayout.TabLayoutOnPageChangeListener pageChangeListener) {
        mPageChangeListeners.remove(pageChangeListener);
    }

    @VisibleForTesting
    Integer validateActiveTab(int tabLayoutPosition) {
        // The tab was detached but the object stayed in the pool and was reset:
        if (tabLayoutPosition == TabLayout.Tab.INVALID_POSITION) return null;

        // The tab was removed but the removeTabAt dispatched a onTabSelected event on it:
        if (tabLayoutPosition >= mModel.get(TABS).size()) return null;

        return tabLayoutPosition;
    }
}
