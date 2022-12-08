// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import androidx.fragment.app.Fragment;
import androidx.viewpager2.adapter.FragmentStateAdapter;

import java.util.ArrayList;
import java.util.List;

/**
 * Controls the behavior of the ViewPager to navigate between privacy guide steps.
 */
public class PrivacyGuidePagerAdapter extends FragmentStateAdapter {
    private final List<Integer> mFragmentTypeList = new ArrayList<>();

    public PrivacyGuidePagerAdapter(Fragment parent, StepDisplayHandler displayHandler) {
        super(parent);

        mFragmentTypeList.add(PrivacyGuideFragment.FragmentType.MSBB);
        if (displayHandler.shouldDisplaySync()) {
            mFragmentTypeList.add(PrivacyGuideFragment.FragmentType.SYNC);
        }
        if (displayHandler.shouldDisplaySafeBrowsing()) {
            mFragmentTypeList.add(PrivacyGuideFragment.FragmentType.SAFE_BROWSING);
        }
        if (displayHandler.shouldDisplayCookies()) {
            mFragmentTypeList.add(PrivacyGuideFragment.FragmentType.COOKIES);
        }
    }

    @Override
    public Fragment createFragment(int position) {
        @PrivacyGuideFragment.FragmentType
        int fragmentType = getFragmentType(position);
        switch (fragmentType) {
            case PrivacyGuideFragment.FragmentType.MSBB:
                return new MSBBFragment();
            case PrivacyGuideFragment.FragmentType.SYNC:
                return new SyncFragment();
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
     * TODO(crbug.com/1396267): Remove this method and substitute with getCurrentFragmentType
     *
     * @param position within |mFragmentTypeList|
     * @return the {@link PrivacyGuideFragment.FragmentType} at the specified position.
     */
    public @PrivacyGuideFragment.FragmentType int getFragmentType(int position) {
        return mFragmentTypeList.get(position);
    }
}
