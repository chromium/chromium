// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import androidx.fragment.app.Fragment;
import androidx.viewpager2.adapter.FragmentStateAdapter;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Controls the behavior of the ViewPager to navigate between privacy guide steps.
 */
public class PrivacyGuidePagerAdapter extends FragmentStateAdapter {
    private final List<Integer> mFragmentTypeList;

    public PrivacyGuidePagerAdapter(Fragment parent, StepDisplayHandler displayHandler) {
        super(parent);
        Set<Integer> fragmentTypesToDisplay = getFragmentTypesToDisplay(displayHandler);
        mFragmentTypeList = getFragmentTypesToDisplayInOrder(fragmentTypesToDisplay);
    }

    private List<Integer> getFragmentTypesToDisplayInOrder(Set<Integer> fragmentTypesToDisplay) {
        List<Integer> fragmentTypesToDisplayInOrder = new ArrayList<>();

        // Add the fragment types to display to |fragmentTypesToDisplayInOrder|
        // in the order they are declared in FragmentType.
        for (@PrivacyGuideFragment.FragmentType int fragmentType = 0;
                fragmentType <= PrivacyGuideFragment.FragmentType.MAX_VALUE; fragmentType++) {
            if (fragmentTypesToDisplay.contains(fragmentType)) {
                fragmentTypesToDisplayInOrder.add(fragmentType);
            }
        }

        return fragmentTypesToDisplayInOrder;
    }

    private Set<Integer> getFragmentTypesToDisplay(StepDisplayHandler displayHandler) {
        Set<Integer> fragmentTypesToDisplay = new HashSet<>();

        fragmentTypesToDisplay.add(PrivacyGuideFragment.FragmentType.MSBB);
        if (displayHandler.shouldDisplayHistorySync()) {
            fragmentTypesToDisplay.add(PrivacyGuideFragment.FragmentType.HISTORY_SYNC);
        }
        if (displayHandler.shouldDisplaySafeBrowsing()) {
            fragmentTypesToDisplay.add(PrivacyGuideFragment.FragmentType.SAFE_BROWSING);
        }
        if (displayHandler.shouldDisplayCookies()) {
            fragmentTypesToDisplay.add(PrivacyGuideFragment.FragmentType.COOKIES);
        }

        return Collections.unmodifiableSet(fragmentTypesToDisplay);
    }

    @Override
    public Fragment createFragment(int position) {
        @PrivacyGuideFragment.FragmentType
        int fragmentType = getFragmentType(position);
        switch (fragmentType) {
            case PrivacyGuideFragment.FragmentType.MSBB:
                return new MSBBFragment();
            case PrivacyGuideFragment.FragmentType.HISTORY_SYNC:
                return new HistorySyncFragment();
            case PrivacyGuideFragment.FragmentType.SAFE_BROWSING:
                return new SafeBrowsingFragment();
            case PrivacyGuideFragment.FragmentType.COOKIES:
                return new CookiesFragment();
        }
        return null;
    }

    @Override
    public int getItemCount() {
        return mFragmentTypeList.size();
    }

    /**
     * Returns a {@link PrivacyGuideFragment.FragmentType} at a specified position of Privacy Guide.
     *
     * @param position within |mFragmentTypeList|
     * @return the {@link PrivacyGuideFragment.FragmentType} at the specified position.
     */
    public @PrivacyGuideFragment.FragmentType int getFragmentType(int position) {
        return mFragmentTypeList.get(position);
    }
}
