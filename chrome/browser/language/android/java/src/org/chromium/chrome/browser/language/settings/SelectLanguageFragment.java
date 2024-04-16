// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;

import androidx.appcompat.widget.SearchView;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.language.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ProfileDependentSetting;
import org.chromium.components.browser_ui.settings.SettingsUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Fragment with a {@link RecyclerView} containing a list of languages that users may add to their
 * accept languages. There is a {@link SearchView} on its Actionbar to make a quick lookup.
 */
public class SelectLanguageFragment extends Fragment implements ProfileDependentSetting {
    // Intent key to pass selected language code from SelectLanguageFragment.
    static final String INTENT_SELECTED_LANGUAGE = "SelectLanguageFragment.SelectedLanguage";
    // Intent key to receive type of languages to populate fragment with.
    static final String INTENT_POTENTIAL_LANGUAGES = "SelectLanguageFragment.PotentialLanguages";

    /** A host to launch SelectLanguageFragment and receive the result. */
    interface Launcher {
        /** Launches SelectLanguageFragment. */
        void launchAddLanguage();
    }

    private class LanguageSearchListAdapter extends LanguageListBaseAdapter {
        LanguageSearchListAdapter(Context context, Profile profile) {
            super(context, profile);
        }

        @Override
        public void onBindViewHolder(ViewHolder holder, int position) {
            super.onBindViewHolder(holder, position);
            ((LanguageRowViewHolder) holder)
                    .setItemClickListener(getItemByPosition(position), mItemClickListener);
        }

        /**
         * Called to perform a search.
         * @param query The text to search for.
         */
        private void search(String query) {
            if (TextUtils.isEmpty(query)) {
                setDisplayedLanguages(mFilteredLanguages);
                return;
            }

            Locale locale = Locale.getDefault();
            query = query.trim().toLowerCase(locale);
            List<LanguageItem> results = new ArrayList<>();
            for (LanguageItem item : mFilteredLanguages) {
                // TODO(crbug.com/40548938): Consider searching in item's native display name and
                // language code too.
                if (item.getDisplayName().toLowerCase(locale).contains(query)) {
                    results.add(item);
                }
            }
            setDisplayedLanguages(results);
        }
    }

    // The view for searching the list of items.
    private SearchView mSearchView;

    // If not blank, represents a substring to use to search for language names.
    private String mSearch;

    private RecyclerView mRecyclerView;
    private LanguageSearchListAdapter mAdapter;
    private List<LanguageItem> mFilteredLanguages;
    private LanguageListBaseAdapter.ItemClickListener mItemClickListener;
    private Profile mProfile;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DETAILED_LANGUAGE_SETTINGS)) {
            getActivity().setTitle(R.string.languages_select);
        } else {
            getActivity().setTitle(R.string.add_language);
        }
        setHasOptionsMenu(true);
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        // Inflate the layout for this fragment.
        View view = inflater.inflate(R.layout.add_languages_main, container, false);
        mSearch = "";
        final Activity activity = getActivity();

        mRecyclerView = (RecyclerView) view.findViewById(R.id.language_list);
        LinearLayoutManager layoutManager = new LinearLayoutManager(activity);
        mRecyclerView.setLayoutManager(layoutManager);
        mRecyclerView.addItemDecoration(
                new DividerItemDecoration(activity, layoutManager.getOrientation()));

        @LanguagesManager.LanguageListType
        int languageOption =
                getActivity()
                        .getIntent()
                        .getIntExtra(
                                INTENT_POTENTIAL_LANGUAGES,
                                LanguagesManager.LanguageListType.ACCEPT_LANGUAGES);
        mFilteredLanguages =
                LanguagesManager.getForProfile(mProfile).getPotentialLanguages(languageOption);
        mItemClickListener =
                item -> {
                    Intent intent = new Intent();
                    intent.putExtra(INTENT_SELECTED_LANGUAGE, item.getCode());
                    activity.setResult(Activity.RESULT_OK, intent);
                    activity.finish();
                };
        mAdapter = new LanguageSearchListAdapter(activity, mProfile);

        mRecyclerView.setAdapter(mAdapter);
        mAdapter.setDisplayedLanguages(mFilteredLanguages);
        mRecyclerView
                .getViewTreeObserver()
                .addOnScrollChangedListener(
                        SettingsUtils.getShowShadowOnScrollListener(
                                mRecyclerView, view.findViewById(R.id.shadow)));
        return view;
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        inflater.inflate(R.menu.languages_action_bar_menu, menu);

        mSearchView = (SearchView) menu.findItem(R.id.search).getActionView();
        mSearchView.setImeOptions(EditorInfo.IME_FLAG_NO_FULLSCREEN);

        mSearchView.setOnCloseListener(
                () -> {
                    mSearch = "";
                    mAdapter.setDisplayedLanguages(mFilteredLanguages);
                    return false;
                });

        mSearchView.setOnQueryTextListener(
                new SearchView.OnQueryTextListener() {
                    @Override
                    public boolean onQueryTextSubmit(String query) {
                        return true;
                    }

                    @Override
                    public boolean onQueryTextChange(String query) {
                        if (TextUtils.isEmpty(query) || TextUtils.equals(query, mSearch)) {
                            return true;
                        }

                        mSearch = query;
                        mAdapter.search(mSearch);
                        return true;
                    }
                });
    }

    @Override
    public void setProfile(Profile profile) {
        mProfile = profile;
    }
}
