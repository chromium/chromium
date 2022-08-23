// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import androidx.annotation.IntDef;
import androidx.fragment.app.Fragment;
import androidx.viewpager2.adapter.FragmentStateAdapter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Controls the behavior of the ViewPager to navigate between privacy guide steps.
 */
public class PrivacyGuidePagerAdapter extends FragmentStateAdapter {
    /**
     * The types of views supported. Each view corresponds to a step in the privacy guide.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({FragmentPosition.COOKIES, FragmentPosition.MSBB, FragmentPosition.SAFE_BROWSING,
            FragmentPosition.SYNC, FragmentPosition.COUNT})
    private @interface FragmentPosition {
        int MSBB = 0;
        int SYNC = 1;
        int SAFE_BROWSING = 2;
        int COOKIES = 3;
        int COUNT = 4;
    }

    public PrivacyGuidePagerAdapter(Fragment parent) {
        super(parent);
    }

    @Override
    public Fragment createFragment(int position) {
        switch (position) {
            case FragmentPosition.MSBB:
                return new MSBBFragment();
            case FragmentPosition.SYNC:
                return new SyncFragment();
            case FragmentPosition.SAFE_BROWSING:
                return new SafeBrowsingFragment();
            case FragmentPosition.COOKIES:
                return new CookiesFragment();
        }
        return null;
    }

    @Override
    public int getItemCount() {
        return FragmentPosition.COUNT;
    }
}
