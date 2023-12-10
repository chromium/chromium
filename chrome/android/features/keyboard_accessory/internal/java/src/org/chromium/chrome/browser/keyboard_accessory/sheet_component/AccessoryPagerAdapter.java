// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_component;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.viewpager.widget.PagerAdapter;
import androidx.viewpager.widget.ViewPager;

import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Tab;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.ListModelChangeProcessor;

import java.util.HashMap;
import java.util.Map;

/**
 * This {@link PagerAdapter} renders an observable list of {@link Tab}s into a
 * {@link ViewPager}. It instantiates the tab views based on the layout they provide.
 */
class AccessoryPagerAdapter extends PagerAdapter
        implements ListModelChangeProcessor.ViewBinder<ListModel<Tab>, ViewPager, Void> {
    private final ListModel<Tab> mTabList;
    private final Map<Tab, ViewGroup> mViews;

    /**
     * Creates the PagerAdapter that populates a ViewPager based on a held list of tabs.
     * @param tabList The list that contains the tabs to instantiate.
     */
    public AccessoryPagerAdapter(ListModel<Tab> tabList) {
        mTabList = tabList;
        mViews = new HashMap<>(mTabList.size());
    }

    @NonNull
    @Override
    public Object instantiateItem(@NonNull ViewGroup container, int position) {
        Tab tab = mTabList.get(position);
        ViewGroup layout = mViews.get(tab);
        if (layout == null) {
            layout =
                    (ViewGroup)
                            LayoutInflater.from(container.getContext())
                                    .inflate(tab.getTabLayout(), container, false);
            mViews.put(tab, layout);
            if (container.indexOfChild(layout) == -1) container.addView(layout);
            if (tab.getListener() != null) {
                tab.getListener().onTabCreated(layout);
            }
        }
        return layout;
    }

    @Override
    public void destroyItem(@NonNull ViewGroup container, int position, @Nullable Object object) {
        if (object == null) return; // Nothing to do here.
        ViewGroup viewToBeDeleted = (ViewGroup) object;
        if (container.indexOfChild(viewToBeDeleted) != -1) container.removeView(viewToBeDeleted);
        for (Map.Entry<Tab, ViewGroup> entry : mViews.entrySet()) {
            if (entry.getValue().equals(viewToBeDeleted)) {
                mViews.remove(entry.getKey());
                return; // Every ViewGroup can only be associated to one tab.
            }
        }
    }

    @Override
    public int getCount() {
        return mTabList.size();
    }

    @Override
    public boolean isViewFromObject(@NonNull View view, @NonNull Object o) {
        return view == o;
    }

    @Override
    public int getItemPosition(@NonNull Object object) {
        ViewGroup viewToBeFound = (ViewGroup) object;
        for (int i = 0; i < mTabList.size(); i++) {
            if (viewToBeFound.equals(mViews.get(mTabList.get(i)))) {
                return i; // The tab the view is connected to still exists and its position is i.
            }
        }
        return POSITION_NONE; // Returning this, invokes |destroyItem| on |object|.
    }

    @Override
    public void onItemsInserted(ListModel<Tab> model, ViewPager view, int index, int count) {
        notifyDataSetChanged();
    }

    @Override
    public void onItemsRemoved(ListModel<Tab> model, ViewPager view, int index, int count) {
        notifyDataSetChanged();
    }

    @Override
    public void onItemsChanged(
            ListModel<Tab> model, ViewPager view, int index, int count, Void payload) {
        notifyDataSetChanged();
    }
}
