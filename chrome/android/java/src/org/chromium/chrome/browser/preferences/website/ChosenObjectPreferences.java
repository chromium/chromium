// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.v4.view.MenuItemCompat;
import android.support.v7.widget.SearchView;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.inputmethod.EditorInfo;
import android.widget.ListView;

import org.chromium.base.annotations.RemovableInRelease;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.preferences.PreferenceUtils;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Locale;

/**
 * Shows a particular chosen object (e.g. a USB device) and the list of sites that have been
 * granted access to it by the user.
 */
public class ChosenObjectPreferences
        extends PreferenceFragment implements Preference.OnPreferenceClickListener {
    public static final String EXTRA_OBJECT_INFOS = "org.chromium.chrome.preferences.object_infos";
    public static final String EXTRA_SITES = "org.chromium.chrome.preferences.site_set";
    public static final String EXTRA_CATEGORY =
            "org.chromium.chrome.preferences.content_settings_type";

    public static final String PREF_OBJECT_NAME = "object_name";

    // The site settings category we are showing.
    private SiteSettingsCategory mCategory;
    // The set of object permissions being examined.
    private ArrayList<ChosenObjectInfo> mObjectInfos;
    // The set of sites to display.
    private ArrayList<Website> mSites;
    // The view for searching the list of items.
    private SearchView mSearchView;
    // If not blank, represents a substring to use to search for site names.
    private String mSearch = "";

    @Override
    @SuppressWarnings("unchecked")
    public void onActivityCreated(Bundle savedInstanceState) {
        PreferenceUtils.addPreferencesFromResource(this, R.xml.chosen_object_preferences);
        ListView listView = (ListView) getView().findViewById(android.R.id.list);
        listView.setDivider(null);

        int contentSettingsType = getArguments().getInt(EXTRA_CATEGORY);
        mCategory = SiteSettingsCategory.createFromContentSettingsType(contentSettingsType);
        mObjectInfos =
                (ArrayList<ChosenObjectInfo>) getArguments().getSerializable(EXTRA_OBJECT_INFOS);
        checkObjectConsistency();
        mSites = (ArrayList<Website>) getArguments().getSerializable(EXTRA_SITES);
        String title = getArguments().getString(SingleCategoryPreferences.EXTRA_TITLE);
        if (title != null) getActivity().setTitle(title);

        setHasOptionsMenu(true);

        super.onActivityCreated(savedInstanceState);
    }

    /**
     * Checks the consistency of |mObjectInfos|.
     *
     * This method asserts that for all the entries in |mObjectInfos| the getObject() method
     * returns the same value. This must be true because this activity is displaying permissions
     * for a single object. Each instance varies only in which site it represents.
     */
    @RemovableInRelease
    private void checkObjectConsistency() {
        String exampleObject = mObjectInfos.get(0).getObject();
        for (ChosenObjectInfo info : mObjectInfos) {
            assert info.getObject().equals(exampleObject);
        }
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        inflater.inflate(R.menu.website_preferences_menu, menu);

        MenuItem searchItem = menu.findItem(R.id.search);
        mSearchView = (SearchView) MenuItemCompat.getActionView(searchItem);
        mSearchView.setImeOptions(EditorInfo.IME_FLAG_NO_FULLSCREEN);
        SearchView.OnQueryTextListener queryTextListener = new SearchView.OnQueryTextListener() {
            @Override
            public boolean onQueryTextSubmit(String query) {
                return true;
            }

            @Override
            public boolean onQueryTextChange(String query) {
                // Make search case-insensitive.
                query = query.toLowerCase(Locale.getDefault());

                if (query.equals(mSearch)) return true;

                mSearch = query;
                getInfo();
                return true;
            }
        };
        mSearchView.setOnQueryTextListener(queryTextListener);

        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(VectorDrawableCompat.create(
                getResources(), R.drawable.ic_help_and_feedback, getActivity().getTheme()));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            HelpAndFeedback.getInstance(getActivity())
                    .show(getActivity(), getString(R.string.help_context_settings),
                            Profile.getLastUsedProfile(), null);
            return true;
        }
        return false;
    }

    @Override
    public void onResume() {
        super.onResume();

        if (mSites == null) {
            getInfo();
        } else {
            resetList();
        }
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (PREF_OBJECT_NAME.equals(preference.getKey())) {
            for (int i = 0; i < mObjectInfos.size(); ++i) mObjectInfos.get(i).revoke();
            getActivity().finish();
            return true;
        }

        return false;
    }

    private class ResultsPopulator implements WebsitePermissionsFetcher.WebsitePermissionsCallback {
        @Override
        public void onWebsitePermissionsAvailable(Collection<Website> sites) {
            // This method may be called after the activity has been destroyed.
            // In that case, bail out.
            if (getActivity() == null) return;

            // Remember the object being examined in this view so that we can filter the results
            // to only include sites with permission for this particular object.
            String exampleObject = mObjectInfos.get(0).getObject();

            mObjectInfos.clear();
            mSites = new ArrayList<Website>();
            for (Website site : sites) {
                for (ChosenObjectInfo info : site.getChosenObjectInfo()) {
                    if (info.getObject().equals(exampleObject)) {
                        mObjectInfos.add(info);
                        if (mSearch.isEmpty()
                                || site.getTitle()
                                           .toLowerCase(Locale.getDefault())
                                           .contains(mSearch)) {
                            mSites.add(site);
                        }
                    }
                }
            }

            // After revoking a site's permission to access an object the user may end up back at
            // this activity. It is awkward to display this empty list because there's no action
            // that can be taken from it. In this case we dismiss this activity as well, taking
            // them back to SingleCategoryPreferences which will now no longer offer the option to
            // examine the permissions for this object.
            if (mObjectInfos.isEmpty()) {
                getActivity().finish();
            } else {
                resetList();
            }
        }
    }

    /**
     * Refreshes the list of sites with access to the object being examined.
     *
     * resetList() is called to refresh the view when the data is ready.
     */
    private void getInfo() {
        WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher();
        fetcher.fetchPreferencesForCategory(mCategory, new ResultsPopulator());
    }

    private void resetList() {
        getPreferenceScreen().removeAll();
        PreferenceUtils.addPreferencesFromResource(this, R.xml.chosen_object_preferences);

        PreferenceScreen preferenceScreen = getPreferenceScreen();
        Preference header = preferenceScreen.findPreference(PREF_OBJECT_NAME);
        // All the ChosenObjectInfo instances represent the same object so we may arbitrarily
        // use the name of the first one.
        header.setTitle(mObjectInfos.get(0).getName());
        header.setOnPreferenceClickListener(this);

        for (int i = 0; i < mSites.size(); ++i) {
            Website site = mSites.get(i);
            Preference preference = new WebsitePreference(getActivity(), site, mCategory);
            preference.getExtras().putSerializable(SingleWebsitePreferences.EXTRA_SITE, site);
            preference.setFragment(SingleWebsitePreferences.class.getCanonicalName());
            preferenceScreen.addPreference(preference);
        }

        // Force this list to be reloaded if the activity is resumed.
        mSites = null;
    }
}
