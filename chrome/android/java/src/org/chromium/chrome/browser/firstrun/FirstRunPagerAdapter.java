// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.viewpager2.adapter.FragmentStateAdapter;

import java.util.ArrayList;
import java.util.List;

/** Adapter used to provide First Run pages to the FirstRunActivity ViewPager. */
class FirstRunPagerAdapter extends FragmentStateAdapter {
    private final List<FirstRunPage> mPages;

    private final List<FirstRunFragment> mFragments = new ArrayList<>();

    public FirstRunPagerAdapter(FragmentActivity activity, List<FirstRunPage> pages) {
        super(activity);
        assert pages != null;
        assert pages.size() > 0;
        mPages = pages;
    }

    /**
     * Returns the FirstRunFragment at the passed-in position. Returns null if the fragment has not
     * yet been instantiated by RecyclerView.
     */
    public FirstRunFragment getFirstRunFragment(int position) {
        return (position < mFragments.size()) ? mFragments.get(position) : null;
    }

    @Override
    public Fragment createFragment(int position) {
        assert position >= 0 && position < mPages.size();
        Fragment fragment = mPages.get(position).instantiateFragment();

        for (int i = mFragments.size(); i <= position; i++) {
            mFragments.add(null);
        }

        // Caching fragment is OK because FirstRunActivity retains all of the fragments via
        // ViewPager2#setOffscreenPageLimit(). See crbug.com/740897 for details.
        mFragments.set(position, (FirstRunFragment) fragment);

        return fragment;
    }

    @Override
    public int getItemCount() {
        return mPages.size();
    }
}
