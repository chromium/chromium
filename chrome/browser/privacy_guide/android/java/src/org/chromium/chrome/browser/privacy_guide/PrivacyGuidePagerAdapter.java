// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import androidx.annotation.IntDef;
import androidx.fragment.app.Fragment;
import androidx.viewpager2.adapter.FragmentStateAdapter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Controls the behavior of the ViewPager to navigate between privacy guide steps.
 */
public class PrivacyGuidePagerAdapter extends FragmentStateAdapter {
    /**
     * The types of fragments supported. Each fragment corresponds to a step in the privacy guide.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({FragmentType.COOKIES, FragmentType.MSBB, FragmentType.SAFE_BROWSING,
            FragmentType.SYNC})
    private @interface FragmentType {
        int MSBB = 0;
        int SYNC = 1;
        int SAFE_BROWSING = 2;
        int COOKIES = 3;
    }

    private final List<Integer> mFragmentTypeList = new ArrayList<>();

    public PrivacyGuidePagerAdapter(Fragment parent, StepDisplayHandler displayHandler) {
        super(parent);

        mFragmentTypeList.add(FragmentType.MSBB);
        if (displayHandler.shouldDisplaySync()) {
            mFragmentTypeList.add(FragmentType.SYNC);
        }
        if (displayHandler.shouldDisplaySafeBrowsing()) {
            mFragmentTypeList.add(FragmentType.SAFE_BROWSING);
        }
        if (displayHandler.shouldDisplayCookies()) {
            mFragmentTypeList.add(FragmentType.COOKIES);
        }
    }

    @Override
    public Fragment createFragment(int position) {
        @FragmentType
        int fragmentType = mFragmentTypeList.get(position);
        switch (fragmentType) {
            case FragmentType.MSBB:
                return new MSBBFragment();
            case FragmentType.SYNC:
                return new SyncFragment();
            case FragmentType.SAFE_BROWSING:
                return new SafeBrowsingFragment();
            case FragmentType.COOKIES:
                return new CookiesFragment();
        }
        return null;
    }

    @Override
    public int getItemCount() {
        return mFragmentTypeList.size();
    }
}
