// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import com.google.android.material.tabs.TabLayout;
import com.google.android.material.tabs.TabLayout.Tab;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.download.home.filter.FilterCoordinator.TabType;
import org.chromium.chrome.browser.download.internal.R;

/**
 * A View class responsible for setting specific properties from a {@link FilterModel} to a
 * Android {@code View} hierarchy.
 */
class FilterView {
    private final ViewGroup mView;

    private final TabLayout mTabsView;
    private final ViewGroup mContentContainerView;

    private Callback</* @TabType */ Integer> mTabSelectedCallback;

    /** Builds a new FilterView. */
    public FilterView(Context context) {
        mView = (ViewGroup) LayoutInflater.from(context).inflate(R.layout.download_home_tabs, null);

        mTabsView = (TabLayout) mView.findViewById(R.id.tabs);
        mContentContainerView = (ViewGroup) mView.findViewById(R.id.content_container);

        mTabsView.setOnTabSelectedListener(
                new TabLayout.OnTabSelectedListener() {
                    @Override
                    public void onTabUnselected(Tab tab) {}

                    @Override
                    public void onTabReselected(Tab tab) {}

                    @Override
                    public void onTabSelected(Tab tab) {
                        if (mTabSelectedCallback == null) return;

                        @TabType
                        int tabType =
                                tab.getPosition() == 0
                                        ? FilterCoordinator.TabType.FILES
                                        : FilterCoordinator.TabType.PREFETCH;
                        mTabSelectedCallback.onResult(tabType);
                    }
                });
    }

    /** The underlying {@link View} that represents this widget. */
    public View getView() {
        return mView;
    }

    /** Sets the content view of the widget to {@code view}. */
    public void setContentView(View view) {
        mContentContainerView.removeAllViews();
        if (view != null) {
            mContentContainerView.addView(
                    view, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        }
    }

    /** Sets which of the two tabs are selected based on {code selectedType}. */
    public void setTabSelected(@TabType int selectedType) {
        int selectedIndex = selectedType == FilterCoordinator.TabType.FILES ? 0 : 1;
        if (mTabsView.getSelectedTabPosition() == selectedIndex) return;
        mTabsView.getTabAt(selectedIndex).select();
    }

    /** Sets the callback for when one of the tabs is selected. */
    public void setTabSelectedCallback(Callback</* @TabType */ Integer> callback) {
        mTabSelectedCallback = callback;
    }

    /** Sets whether or not we show the tabs. */
    public void setShowTabs(boolean show) {
        mTabsView.setVisibility(show ? View.VISIBLE : View.GONE);
    }
}
