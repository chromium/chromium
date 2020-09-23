// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import androidx.core.text.TextUtilsCompat;
import androidx.core.view.ViewCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentPagerAdapter;
import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;
// TODO(bjoyce): Need to convert viewpager after fragmentpageradatper.
import androidx.viewpager.widget.ViewPager;

// TODO(bjoyce): Need to convert tablayout to androidx after viewpager
// and fragmentpageradatper are converted.
import com.google.android.material.tabs.TabLayout;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivity;

import java.util.Locale;

/**
 * Fragment with a {@link TabLayout} containing a basic and an advanced version of the CBD dialog.
 */
public class ClearBrowsingDataTabsFragment extends Fragment {
    public static final int CBD_TAB_COUNT = 2;

    private ClearBrowsingDataFetcher mFetcher;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setHasOptionsMenu(true);

        if (savedInstanceState == null) {
            mFetcher = new ClearBrowsingDataFetcher();
            mFetcher.fetchImportantSites();
            mFetcher.requestInfoAboutOtherFormsOfBrowsingHistory();
        } else {
            mFetcher = savedInstanceState.getParcelable(
                    ClearBrowsingDataFragment.CLEAR_BROWSING_DATA_FETCHER);
        }

        RecordUserAction.record("ClearBrowsingData_DialogCreated");
    }

    /*
     * RTL is broken for ViewPager: https://code.google.com/p/android/issues/detail?id=56831
     * This class works around this issue by inserting the tabs in inverse order if RTL is active.
     * The TabLayout needs to be set to LTR for this to work.
     * TODO(dullweber): Extract the RTL code into a wrapper class if other places in Chromium need
     * it as well.
     */
    private static int adjustIndexForDirectionality(int index) {
        if (TextUtilsCompat.getLayoutDirectionFromLocale(Locale.getDefault())
                == ViewCompat.LAYOUT_DIRECTION_RTL) {
            return CBD_TAB_COUNT - 1 - index;
        }
        return index;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        // Inflate the layout for this fragment.
        View view = inflater.inflate(R.layout.clear_browsing_data_tabs, container, false);

        // Get the ViewPager and set its PagerAdapter so that it can display items.
        ViewPager viewPager = view.findViewById(R.id.clear_browsing_data_viewpager);
        viewPager.setAdapter(
                new ClearBrowsingDataPagerAdapter(mFetcher, getFragmentManager(), getActivity()));

        // Give the TabLayout the ViewPager.
        TabLayout tabLayout = view.findViewById(R.id.clear_browsing_data_tabs);
        tabLayout.setupWithViewPager(viewPager);
        int tabIndex = adjustIndexForDirectionality(
                BrowsingDataBridge.getInstance().getLastSelectedClearBrowsingDataTab());
        TabLayout.Tab tab = tabLayout.getTabAt(tabIndex);
        if (tab != null) {
            tab.select();
        }
        tabLayout.addOnTabSelectedListener(new TabSelectListener());

        // Remove elevation to avoid shadow between title and tabs.
        SettingsActivity activity = (SettingsActivity) getActivity();
        activity.getSupportActionBar().setElevation(0.0f);

        return view;
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        // mFetcher acts as a cache for important sites and history data. If the activity gets
        // suspended, we can save the cached data and reuse it when we are activated again.
        outState.putParcelable(ClearBrowsingDataFragment.CLEAR_BROWSING_DATA_FETCHER, mFetcher);
    }

    private static class ClearBrowsingDataPagerAdapter extends FragmentPagerAdapter {
        private final ClearBrowsingDataFetcher mFetcher;
        private final Context mContext;

        ClearBrowsingDataPagerAdapter(
                ClearBrowsingDataFetcher fetcher, FragmentManager fm, Context context) {
            super(fm);
            mFetcher = fetcher;
            mContext = context;
        }

        @Override
        public int getCount() {
            return CBD_TAB_COUNT;
        }

        @Override
        public Fragment getItem(int position) {
            position = adjustIndexForDirectionality(position);
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
            fragment.setClearBrowsingDataFetcher(mFetcher);
            return fragment;
        }

        @Override
        public CharSequence getPageTitle(int position) {
            position = adjustIndexForDirectionality(position);
            switch (position) {
                case 0:
                    return mContext.getString(R.string.clear_browsing_data_basic_tab_title);
                case 1:
                    return mContext.getString(R.string.prefs_section_advanced);
                default:
                    throw new RuntimeException("invalid position: " + position);
            }
        }
    }

    private static class TabSelectListener implements TabLayout.OnTabSelectedListener {
        @Override
        public void onTabSelected(TabLayout.Tab tab) {
            int tabIndex = adjustIndexForDirectionality(tab.getPosition());
            BrowsingDataBridge.getInstance().setLastSelectedClearBrowsingDataTab(tabIndex);
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

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(VectorDrawableCompat.create(
                getResources(), R.drawable.ic_help_and_feedback, getActivity().getTheme()));
        help.setShowAsAction(MenuItem.SHOW_AS_ACTION_IF_ROOM);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            HelpAndFeedbackLauncherImpl.getInstance().show(getActivity(),
                    getString(R.string.help_context_clear_browsing_data),
                    Profile.getLastUsedRegularProfile(), null);
            return true;
        }
        return false;
    }
}
