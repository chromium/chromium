// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;

import androidx.activity.OnBackPressedCallback;
import androidx.appcompat.widget.SearchView;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.ui.KeyboardUtils;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.language.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ProfileDependentSetting;
import org.chromium.chrome.browser.settings.SettingsHostUtil;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SearchViewProvider;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Fragment with a {@link RecyclerView} containing a list of languages that users may add to their
 * accept languages. There is a {@link SearchView} on its Actionbar to make a quick lookup.
 *
 * <p>There is one subclass per kind of selection, nested below, and each is its own settings page
 * with its own Url. To add one: subclass this, return the list to offer from {@link
 * #getLanguageListType} and the page that launches it from {@link #getRequestingFragment}, add it
 * to {@link #ALL_PICKERS}, and register it in SettingsFragmentRegistry. The requesting page
 * launches the subclass and, under Url navigation, listens for {@link #resultKey} of it.
 */
@NullMarked
public abstract class SelectLanguageFragment extends Fragment
        implements ProfileDependentSetting,
                SettingsFragment,
                EmbeddableSettingsPage,
                SearchViewProvider {
    // Intent key to pass selected language code from SelectLanguageFragment.
    static final String KEY_SELECTED_LANGUAGE = "SelectLanguageFragment.SelectedLanguage";

    static final String FRAGMENT_RESULT_TAG = "SelectLanguageFragment";

    /** Picks the app language, for {@link LanguageSettings}. */
    public static class AppLanguagePickerFragment extends SelectLanguageFragment {
        @Override
        protected @LanguagesManager.LanguageListType int getLanguageListType() {
            return LanguagesManager.LanguageListType.UI_LANGUAGES;
        }

        @Override
        protected Class<? extends Fragment> getRequestingFragment() {
            return LanguageSettings.class;
        }
    }

    /** Picks a language to add to the accept languages, for {@link LanguageSettings}. */
    public static class ContentLanguagePickerFragment extends SelectLanguageFragment {
        @Override
        protected @LanguagesManager.LanguageListType int getLanguageListType() {
            return LanguagesManager.LanguageListType.ACCEPT_LANGUAGES;
        }

        @Override
        protected Class<? extends Fragment> getRequestingFragment() {
            return LanguageSettings.class;
        }
    }

    /** Picks the language to translate into, for {@link LanguageSettings}. */
    public static class TranslateTargetLanguagePickerFragment extends SelectLanguageFragment {
        @Override
        protected @LanguagesManager.LanguageListType int getLanguageListType() {
            return LanguagesManager.LanguageListType.TARGET_LANGUAGES;
        }

        @Override
        protected Class<? extends Fragment> getRequestingFragment() {
            return LanguageSettings.class;
        }
    }

    /** Picks a language to always translate, for {@link AlwaysTranslateListFragment}. */
    public static class AlwaysTranslateLanguagePickerFragment extends SelectLanguageFragment {
        @Override
        protected @LanguagesManager.LanguageListType int getLanguageListType() {
            return LanguagesManager.LanguageListType.ALWAYS_LANGUAGES;
        }

        @Override
        protected Class<? extends Fragment> getRequestingFragment() {
            return AlwaysTranslateListFragment.class;
        }
    }

    /** Picks a language to never translate, for {@link NeverTranslateListFragment}. */
    public static class NeverTranslateLanguagePickerFragment extends SelectLanguageFragment {
        @Override
        protected @LanguagesManager.LanguageListType int getLanguageListType() {
            return LanguagesManager.LanguageListType.NEVER_LANGUAGES;
        }

        @Override
        protected Class<? extends Fragment> getRequestingFragment() {
            return NeverTranslateListFragment.class;
        }
    }

    /** Every picker. Kept next to the subclasses so that a new one is hard to miss. */
    public static final List<Class<? extends SelectLanguageFragment>> ALL_PICKERS =
            List.of(
                    AppLanguagePickerFragment.class,
                    ContentLanguagePickerFragment.class,
                    TranslateTargetLanguagePickerFragment.class,
                    AlwaysTranslateLanguagePickerFragment.class,
                    NeverTranslateLanguagePickerFragment.class);

    /**
     * The result key {@code picker} delivers its selection under, under Url navigation.
     *
     * <p>A key per picker, rather than one key for them all. Under Url navigation the picker
     * replaces the page that opened it, and the result is delivered to that page's replacement once
     * it is created. A single shared key would let a selection made for one page be applied by
     * another. Deriving the key from the class means a picker and the page listening for it cannot
     * disagree on a string, and two pickers cannot share one.
     */
    public static String resultKey(Class<? extends SelectLanguageFragment> picker) {
        return picker.getName();
    }

    /**
     * Whether {@code fragment} is navigated by Url rather than by the fragment back stack.
     *
     * <p>The picker and the pages that ask it for a language hand the selection over differently
     * under Url navigation, so they all have to agree on this. Settings hosted in SettingsActivity,
     * e.g. on phones, keep using the fragment back stack even when the flag is enabled.
     */
    static boolean usesUrlNavigation(Fragment fragment) {
        return ChromeFeatureList.sSettingsInTabUrlNav.isEnabled()
                && SettingsHostUtil.isShownInTab(fragment);
    }

    /** A host to launch SelectLanguageFragment and receive the result. */
    interface Launcher {
        /** Launches the picker for a language to add to the accept languages. */
        void launchAddLanguage();
    }

    /** Which languages to offer. */
    protected abstract @LanguagesManager.LanguageListType int getLanguageListType();

    /**
     * The page that launches this picker and receives its selection. Under Url navigation the
     * picker finishes by navigating there, as there is no back stack to return along.
     */
    protected abstract Class<? extends Fragment> getRequestingFragment();

    public final Class<? extends Fragment> getRequestingFragmentForTesting() {
        return getRequestingFragment();
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
            assumeNonNull(mFilteredLanguages);
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
    private @Nullable SearchView mSearchView;

    // If not blank, represents a substring to use to search for language names.
    private String mSearch;

    private RecyclerView mRecyclerView;
    private LanguageSearchListAdapter mAdapter;
    private @MonotonicNonNull List<LanguageItem> mFilteredLanguages;
    private LanguageListBaseAdapter.ItemClickListener mItemClickListener;
    private @MonotonicNonNull Profile mProfile;
    private @MonotonicNonNull SearchViewProvider.Observer mSearchViewObserver;

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    private @Nullable OnBackPressedCallback mBackPressCallback;

    // Whether a selection has been handed to the requesting page under Url navigation.
    private boolean mSelectionPending;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        int titleResource =
                ChromeFeatureList.isEnabled(ChromeFeatureList.DETAILED_LANGUAGE_SETTINGS)
                        ? R.string.languages_select
                        : R.string.add_language;
        mPageTitle.set(getString(titleResource));
        setHasOptionsMenu(true);
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        // Inflate the layout for this fragment.
        View view = inflater.inflate(R.layout.add_languages_main, container, false);
        mSearch = "";
        final Activity activity = getActivity();

        mRecyclerView = (RecyclerView) view.findViewById(R.id.language_list);
        LinearLayoutManager layoutManager = new LinearLayoutManager(activity);
        mRecyclerView.setLayoutManager(layoutManager);
        mRecyclerView.addItemDecoration(
                new DividerItemDecoration(activity, layoutManager.getOrientation()));

        assumeNonNull(mProfile);
        mFilteredLanguages =
                LanguagesManager.getForProfile(mProfile)
                        .getPotentialLanguages(getLanguageListType());
        mItemClickListener =
                item -> {
                    if (usesUrlNavigation(this)) {
                        // The first selection is the one on its way to the requesting page. A
                        // second tap before this page is replaced would otherwise overwrite it.
                        if (mSelectionPending) return;
                        mSelectionPending = true;

                        Bundle result = new Bundle();
                        result.putString(KEY_SELECTED_LANGUAGE, item.getCode());
                        getParentFragmentManager().setFragmentResult(resultKey(getClass()), result);

                        // Leave for the page that asked, which reads the result as it is created.
                        // Its own entry is replaced rather than added to, so that going back from
                        // there does not return to a picker whose selection has already been made.
                        SettingsNavigationFactory.createSettingsNavigation(getContext())
                                .finishCurrentSettings(
                                        this, getRequestingFragment(), /* parentArgs= */ null);
                        return;
                    }

                    Bundle result = new Bundle();
                    result.putString(KEY_SELECTED_LANGUAGE, item.getCode());
                    var fragmentManager = getFragmentManager();
                    assumeNonNull(fragmentManager);
                    fragmentManager.setFragmentResult(FRAGMENT_RESULT_TAG, result);
                    fragmentManager.popBackStack();
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

        SearchView searchView = (SearchView) menu.findItem(R.id.search).getActionView();
        assumeNonNull(searchView);
        initSearchView(searchView);
    }

    /** Initialize a {@link SearchView} for filtering languages. */
    @Override
    public void initSearchView(SearchView searchView) {
        mSearchView = searchView;
        mSearchView.setImeOptions(EditorInfo.IME_FLAG_NO_FULLSCREEN);

        mBackPressCallback =
                new OnBackPressedCallback(false) {
                    @Override
                    public void handleOnBackPressed() {
                        assumeNonNull(mSearchView).setIconified(true);
                    }
                };
        requireActivity()
                .getOnBackPressedDispatcher()
                .addCallback(getViewLifecycleOwner(), mBackPressCallback);

        mSearchView.setOnSearchClickListener(
                view -> {
                    if (mSearchViewObserver != null) mSearchViewObserver.onUpdated(true);
                    assumeNonNull(mBackPressCallback).setEnabled(true);
                });
        mSearchView.setOnCloseListener(
                () -> {
                    mSearch = "";
                    mAdapter.setDisplayedLanguages(assumeNonNull(mFilteredLanguages));
                    if (mSearchViewObserver != null) mSearchViewObserver.onUpdated(false);
                    assumeNonNull(mBackPressCallback).setEnabled(false);
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
    public void setSearchViewObserver(SearchViewProvider.Observer observer) {
        mSearchViewObserver = observer;
    }

    @Override
    public void onDestroy() {
        if (getView() != null) KeyboardUtils.hideAndroidSoftKeyboard(getView());
        if (mSelectionPending && !requireActivity().isChangingConfigurations()) {
            // A fragment result waits in the FragmentManager until a listener for its key starts,
            // however much later that is. Should the requesting page not be shown in this picker's
            // place, e.g. because the host sent the user to the main settings page instead, the
            // selection would otherwise be applied the next time the user happens to open that
            // page. The navigation that destroys this picker is a single transaction that also
            // creates and starts the requesting page, which collects the result before a task
            // posted from here runs. So whatever is left by then was never collected: drop it.
            FragmentManager fragmentManager = getParentFragmentManager();
            String key = resultKey(getClass());
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT, () -> fragmentManager.clearFragmentResult(key));
        }
        super.onDestroy();
        if (mSearchViewObserver != null) mSearchViewObserver.onUpdated(false);
        if (mBackPressCallback != null) mBackPressCallback.remove();
    }

    @Override
    public void setProfile(Profile profile) {
        mProfile = profile;
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }
}
