// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.viewpager2.adapter.FragmentStateAdapter;
import androidx.viewpager2.widget.ViewPager2;

import com.google.android.material.appbar.AppBarLayout;
import com.google.android.material.tabs.TabLayout;
import com.google.android.material.tabs.TabLayoutMediator;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ProfileDependentSetting;
import org.chromium.chrome.browser.settings.SettingsActivity;

/**
 * Fragment with a {@link TabLayout} containing a basic and an advanced version of the CBD dialog.
 */
public class ClearBrowsingDataTabsFragment extends Fragment implements ProfileDependentSetting {
    public static final int CBD_TAB_COUNT = 2;

    private Profile mProfile;
    private ClearBrowsingDataFetcher mFetcher;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        RecordUserAction.record("ClearBrowsingData_DialogCreated");
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        if (savedInstanceState == null) {
            mFetcher = new ClearBrowsingDataFetcher();
            mFetcher.fetchImportantSites(mProfile);
            mFetcher.requestInfoAboutOtherFormsOfBrowsingHistory(mProfile);
        } else {
            mFetcher =
                    savedInstanceState.getParcelable(
                            ClearBrowsingDataFragment.CLEAR_BROWSING_DATA_FETCHER);
        }

        Bundle fragmentArgs = getArguments();
        assert fragmentArgs != null : "A valid fragment argument is required.";
        String referrer =
                fragmentArgs.getString(
                        ClearBrowsingDataFragment.CLEAR_BROWSING_DATA_REFERRER, null);

        // Inflate the layout for this fragment.
        View view = inflater.inflate(R.layout.clear_browsing_data_tabs, container, false);

        // Get the ViewPager and set its PagerAdapter so that it can display items.
        ViewPager2 viewPager = view.findViewById(R.id.clear_browsing_data_viewpager);
        viewPager.setAdapter(
                new ClearBrowsingDataPagerAdapter(
                        mFetcher,
                        getFragmentManager(),
                        (FragmentActivity) getActivity(),
                        referrer));

        // Give the TabLayout the ViewPager.
        TabLayout tabLayout = view.findViewById(R.id.clear_browsing_data_tabs);
        new TabLayoutMediator(
                        tabLayout,
                        viewPager,
                        (tab, position) -> {
                            tab.setText(getTabTitle(position));
                        })
                .attach();
        int tabIndex =
                BrowsingDataBridge.getForProfile(mProfile).getLastSelectedClearBrowsingDataTab();
        TabLayout.Tab tab = tabLayout.getTabAt(tabIndex);
        if (tab != null) {
            tab.select();
        }
        tabLayout.addOnTabSelectedListener(new TabSelectListener(mProfile));

        // Set outline provider to null to prevent shadow from being drawn between title and tabs.
        SettingsActivity activity = (SettingsActivity) getActivity();
        AppBarLayout appBarLayout = activity.findViewById(R.id.app_bar_layout);
        appBarLayout.setOutlineProvider(null);

        return view;
    }

    /**
     * A method to create the {@link ClearBrowsingDataTabsFragment} arguments.
     *
     * @param referrer The name of the referrer activity.
     */
    public static Bundle createFragmentArgs(String referrer) {
        Bundle bundle = new Bundle();
        bundle.putString(ClearBrowsingDataFragment.CLEAR_BROWSING_DATA_REFERRER, referrer);
        return bundle;
    }

    private String getTabTitle(int position) {
        switch (position) {
            case 0:
                return getActivity().getString(R.string.clear_browsing_data_basic_tab_title);
            case 1:
                return getActivity().getString(R.string.prefs_section_advanced);
            default:
                throw new RuntimeException("invalid position: " + position);
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        // mFetcher acts as a cache for important sites and history data. If the activity gets
        // suspended, we can save the cached data and reuse it when we are activated again.
        outState.putParcelable(ClearBrowsingDataFragment.CLEAR_BROWSING_DATA_FETCHER, mFetcher);
    }

    @Override
    public void setProfile(Profile profile) {
        assert profile != null;
        mProfile = profile;
    }

    private static class ClearBrowsingDataPagerAdapter extends FragmentStateAdapter {
        private final ClearBrowsingDataFetcher mFetcher;
        private final String mReferrer;

        ClearBrowsingDataPagerAdapter(
                ClearBrowsingDataFetcher fetcher,
                FragmentManager fm,
                FragmentActivity activity,
                String referrer) {
            super(activity);
            mFetcher = fetcher;
            mReferrer = referrer;
        }

        @Override
        public int getItemCount() {
            return CBD_TAB_COUNT;
        }

        @Override
        public Fragment createFragment(int position) {
            ClearBrowsingDataFragment fragment;
            switch (position) {
                case 0:
                    fragment = new ClearBrowsingDataFragmentBasic();
                    break;
                case 1:
                    fragment = new ClearBrowsingDataFragmentAdvanced();
                    break;
                default:
                    throw new RuntimeException("invalid position: " + position);
            }
            // We supply the fetcher in the next line.
            fragment.setArguments(
                    ClearBrowsingDataFragment.createFragmentArgs(
                            mReferrer, /* isFetcherSuppliedFromOutside= */ true));
            fragment.setClearBrowsingDataFetcher(mFetcher);
            return fragment;
        }
    }

    private static class TabSelectListener implements TabLayout.OnTabSelectedListener {
        private final Profile mProfile;

        TabSelectListener(Profile profile) {
            assert profile != null;
            mProfile = profile;
        }

        @Override
        public void onTabSelected(TabLayout.Tab tab) {
            int tabIndex = tab.getPosition();
            BrowsingDataBridge.getForProfile(mProfile)
                    .setLastSelectedClearBrowsingDataTab(tabIndex);
            if (tabIndex == ClearBrowsingDataTab.BASIC) {
                RecordUserAction.record("ClearBrowsingData_SwitchTo_BasicTab");
            } else {
                RecordUserAction.record("ClearBrowsingData_SwitchTo_AdvancedTab");
            }
        }

        @Override
        public void onTabUnselected(TabLayout.Tab tab) {}

        @Override
        public void onTabReselected(TabLayout.Tab tab) {}
    }
}
