// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Configuration;
import android.os.Handler;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.EditText;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.preference.PreferenceFragmentCompat;
import androidx.window.layout.WindowMetricsCalculator;

import org.chromium.base.Log;
import org.chromium.base.ui.KeyboardUtils;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.MultiColumnSettings;
import org.chromium.chrome.browser.settings.search.SettingsIndexData.SearchResults;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.util.function.BooleanSupplier;

/** The coordinator of search in Settings. TODO(jinsukkim): Build a proper MVC structure. */
@NullMarked
public class SettingsSearchCoordinator {
    private static final String TAG = "SettingsSearch";

    // Tag for Fragment backstack entry loading the search results into the display fragment.
    // Popping the entry means we are transitioning from result -> search state.
    private static final String FRAGMENT_TAG_RESULT = "enter_result_settings";

    private final AppCompatActivity mActivity;
    private final BooleanSupplier mUseMultiColumnSupplier;
    private @Nullable final MultiColumnSettings mMultiColumnSettings;
    private final Handler mHandler = new Handler();
    private @Nullable Fragment mResultsFragment;
    private @Nullable Runnable mSearchRunnable;

    // Whether the back action handler for MultiColumnSettings was set. This is set lazily when
    // search UI gets focus for the first time.
    private boolean mMultiColumnSettingsBackActionHandlerSet;

    // States for search operation. These states are managed to go back and forth between viewing
    // the search results and browsing result fragments. We perform some UI tasks such as
    // initializing search widget UI, creating/restoring fragments as the state changes.
    //
    // FS_SETTINGS: Basic state browsing through setting fragments/preferences
    // FS_SEARCH: In search UI, entering queries and performing search
    // FS_RESULTS: After tapping one of the search results, navigating through them
    private static final int FS_SETTINGS = 0;
    private static final int FS_SEARCH = 1;
    private static final int FS_RESULTS = 2;

    private int mFragmentState;

    // True if multiple-column Fragment is activated. Both the window width and the feature flag
    // condition should be met.
    private boolean mUseMultiColumn;

    private OnBackPressedCallback mBackActionCallback;

    // Set to {@code true} after queries are entered. Used to conditionally clear up the screen.
    private boolean mQueryEntered;
    private SettingsIndexData mIndexData;

    // Interface to communite with search backend and receive results asynchronously.
    public interface SearchCallback {
        /**
         * Invoked when the search results is available.
         *
         * @param results {@link SearchResults} containing the results.
         */
        void onSearchResults(SearchResults results);
    }

    /**
     * @param activity {@link SettingsActivity} object
     * @param useMultiColumnSupplier Supplier telling us whether the multi-column mode is on
     * @param multiColumnSettings {@link MultiColumnSettings} Fragment. Can be {@code null} unless
     *     the multi-column settings feature is enabled.
     */
    public SettingsSearchCoordinator(
            AppCompatActivity activity,
            BooleanSupplier useMultiColumnSupplier,
            @Nullable MultiColumnSettings multiColumnSettings) {
        mActivity = activity;
        mUseMultiColumnSupplier = useMultiColumnSupplier;
        mMultiColumnSettings = multiColumnSettings;
        mFragmentState = FS_SETTINGS;
    }

    /** Initializes search UI, sets up listeners, backpress action handler, etc. */
    @Initializer
    public void initializeSearchUi() {
        mUseMultiColumn = mUseMultiColumnSupplier.getAsBoolean();
        Toolbar actionBar = mActivity.findViewById(R.id.action_bar);
        ViewGroup appBar = mActivity.findViewById(R.id.app_bar_layout);
        ViewGroup searchBoxParent = mUseMultiColumn ? actionBar : appBar;
        LayoutInflater.from(mActivity).inflate(R.layout.settings_search_box, searchBoxParent, true);
        LayoutInflater.from(mActivity).inflate(R.layout.settings_search_query, actionBar, true);
        View searchBox = mActivity.findViewById(R.id.search_box);
        if (mUseMultiColumn) {
            // Adjust the view width after the Fragment layout is completed.
            mHandler.post(this::updateDetailPanelWidth);
        }

        EditText queryEdit = mActivity.findViewById(R.id.search_query);
        setUpQueryEdit(queryEdit);
        searchBox.setOnClickListener(v -> enterSearchState());
        View backToSettings = mActivity.findViewById(R.id.back_arrow_icon);
        backToSettings.setOnClickListener(v -> handleBackAction());
        mBackActionCallback =
                new OnBackPressedCallback(false) {
                    @Override
                    public void handleOnBackPressed() {
                        handleBackAction();
                    }
                };
        mActivity.getOnBackPressedDispatcher().addCallback(mActivity, mBackActionCallback);
        actionBar.setOverflowIcon(null);
    }

    private void handleBackAction() {
        if (mFragmentState == FS_SETTINGS) {
            // Do nothing. Let the default back action handler take care of it.
        } else if (mFragmentState == FS_SEARCH) {
            exitSearchState();
        } else if (mFragmentState == FS_RESULTS) {
            FragmentManager fragmentManager = getSettingsFragmentManager();
            int stackCount = fragmentManager.getBackStackEntryCount();
            if (stackCount > 0) {
                // Switch back to 'search' state if we go all the way back to the fragment
                // where we display the search results.
                String topStackEntry =
                        fragmentManager.getBackStackEntryAt(stackCount - 1).getName();
                if (TextUtils.equals(FRAGMENT_TAG_RESULT, topStackEntry)) {
                    mFragmentState = FS_SEARCH;
                }
            }
            clearFragment(/* addToBackStack= */ false);
            fragmentManager.popBackStack();
        } else {
            assert false : "Unreachable state.";
        }
    }

    @Initializer
    @EnsuresNonNull("mIndexData")
    private void initIndex() {
        if (mIndexData == null) {
            mIndexData = new SettingsIndexData();
        } else {
            if (!mIndexData.needsIndexing()) return;
        }
        mIndexData.initIndex(mActivity);
    }

    private void enterSearchState() {
        initIndex();

        if (mMultiColumnSettings != null && !mMultiColumnSettingsBackActionHandlerSet) {
            mActivity
                    .getOnBackPressedDispatcher()
                    .addCallback(mMultiColumnSettings, mBackActionCallback);
            mMultiColumnSettingsBackActionHandlerSet = true;
        }
        View searchBox = mActivity.findViewById(R.id.search_box);
        View queryContainer = mActivity.findViewById(R.id.search_query_container);
        searchBox.setVisibility(View.GONE);
        queryContainer.setVisibility(View.VISIBLE);
        if (!mUseMultiColumn) {
            assumeNonNull(mActivity.getSupportActionBar()).setDisplayHomeAsUpEnabled(false);
        }
        EditText queryEdit = mActivity.findViewById(R.id.search_query);
        queryEdit.requestFocus();
        queryEdit.setText("");
        KeyboardUtils.showKeyboard(queryEdit);
        mQueryEntered = false;
        clearFragment(/* addToBackStack= */ true);
        mFragmentState = FS_SEARCH;
        mBackActionCallback.setEnabled(true);
    }

    private void exitSearchState() {
        // Back action in search state. Restore the settings fragment and search UI.
        View searchBox = mActivity.findViewById(R.id.search_box);
        View queryContainer = mActivity.findViewById(R.id.search_query_container);
        queryContainer.setVisibility(View.GONE);
        searchBox.setVisibility(View.VISIBLE);
        mQueryEntered = false;
        if (!mUseMultiColumn) {
            assumeNonNull(mActivity.getSupportActionBar()).setDisplayHomeAsUpEnabled(true);
        }
        EditText queryEdit = mActivity.findViewById(R.id.search_query);
        KeyboardUtils.hideAndroidSoftKeyboard(queryEdit);

        // Clearing the fragment first before popping the back stack. Otherwise the existing
        // fragment is visible behind the popped one through the transparent background.
        clearFragment(/* addToBackStack= */ false);
        getSettingsFragmentManager().popBackStack();
        mFragmentState = FS_SETTINGS;
        mBackActionCallback.setEnabled(false);
    }

    private FragmentManager getSettingsFragmentManager() {
        if (mMultiColumnSettings != null) {
            return mMultiColumnSettings.getChildFragmentManager();
        } else {
            return mActivity.getSupportFragmentManager();
        }
    }

    private void clearFragment(boolean addToBackStack) {
        var fragmentManager = getSettingsFragmentManager();
        int viewId = getViewIdForSearchDisplay();
        var transaction = fragmentManager.beginTransaction();
        transaction.setReorderingAllowed(true);
        transaction.replace(viewId, new EmptyFragment());
        transaction.setTransition(FragmentTransaction.TRANSIT_FRAGMENT_FADE);
        if (addToBackStack) transaction.addToBackStack(null);
        transaction.commit();
    }

    /** Returns the view ID where search results will be displayed. */
    private int getViewIdForSearchDisplay() {
        if (mMultiColumnSettings == null) {
            return R.id.content;
        }
        return mMultiColumnSettings.isLayoutOpen()
                ? R.id.preferences_detail
                : R.id.preferences_header;
    }

    private void updateDetailPanelWidth() {
        assert mUseMultiColumn : "Should be called in multi-column mode only.";

        var windowMetrics =
                WindowMetricsCalculator.getOrCreate().computeCurrentWindowMetrics(mActivity);
        int endPaddingPx =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.settings_detail_panel_end_padding);
        int headerWidthPx = assumeNonNull(mMultiColumnSettings).getHeaderPanelWidthPx();
        int detailViewWidthPx = windowMetrics.getBounds().width() - headerWidthPx - endPaddingPx;
        View searchBox = mActivity.findViewById(R.id.search_box);
        var lp = (Toolbar.LayoutParams) searchBox.getLayoutParams();
        lp.width = detailViewWidthPx;
        lp.gravity = Gravity.END;
        searchBox.setLayoutParams(lp);
        View queryContainer = mActivity.findViewById(R.id.search_query_container);
        LayoutParams qlp = queryContainer.getLayoutParams();
        qlp.width = detailViewWidthPx;
        queryContainer.setLayoutParams(qlp);
    }

    /**
     * Updates the UI layout for the changes in column mode/window width.
     *
     * @see {@link Activity#onConfigurationChanged(Configuration)}.
     */
    public void onConfigurationChanged(@NonNull Configuration newConfig) {
        boolean useMultiColumn = mUseMultiColumnSupplier.getAsBoolean();

        if (useMultiColumn == mUseMultiColumn) {
            if (mUseMultiColumn) mHandler.post(this::updateDetailPanelWidth);
            return;
        }

        mUseMultiColumn = useMultiColumn;
        View searchBox = mActivity.findViewById(R.id.search_box);
        ViewGroup searchBoxParent =
                mActivity.findViewById(useMultiColumn ? R.id.app_bar_layout : R.id.action_bar);
        searchBoxParent.removeView(searchBox);
        mHandler.post(() -> switchSearchUiLayout(searchBox));
    }

    private void switchSearchUiLayout(View searchBox) {
        if (mUseMultiColumn) {
            ViewGroup actionBar = mActivity.findViewById(R.id.action_bar);
            actionBar.addView(searchBox);
            updateDetailPanelWidth();
        } else {
            ViewGroup appBarLayout = mActivity.findViewById(R.id.app_bar_layout);
            appBarLayout.addView(searchBox);
            View queryContainer = mActivity.findViewById(R.id.search_query_container);
            LayoutParams lp = searchBox.getLayoutParams();
            lp.width = LayoutParams.MATCH_PARENT;
            searchBox.setLayoutParams(lp);
            lp = queryContainer.getLayoutParams();
            lp.width = LayoutParams.MATCH_PARENT;
            queryContainer.setLayoutParams(lp);
        }
    }

    private void setUpQueryEdit(EditText queryEdit) {
        queryEdit.addTextChangedListener(
                new TextWatcher() {
                    @Override
                    public void beforeTextChanged(
                            CharSequence s, int start, int count, int after) {}

                    @Override
                    public void onTextChanged(CharSequence s, int start, int before, int count) {}

                    @Override
                    public void afterTextChanged(Editable s) {
                        String query = s.toString().trim();
                        if (query.length() > 0) {
                            mQueryEntered = true;
                            performSearch(
                                    query, SettingsSearchCoordinator.this::displayResultsFragment);
                        } else {
                            if (mQueryEntered) clearFragment(/* addToBackStack= */ false);
                        }
                    }
                });
    }

    /**
     * Performs search by sending the query to search backend.
     *
     * @param query The search query the user entered.
     * @param callback The callback function to be executed when results are available.
     */
    @EnsuresNonNull("mSearchRunnable")
    private void performSearch(String query, SearchCallback callback) {
        if (mSearchRunnable != null) {
            // Debouncing to avoid initiating search for each keystroke entered fast.
            // We sets some delay before initiating search (see postDelayed() below) so that
            // we can perform search only when user is likely to have typed the entire query.
            mHandler.removeCallbacks(mSearchRunnable);
        }

        mSearchRunnable = () -> callback.onSearchResults(mIndexData.search(query));
        mHandler.postDelayed(mSearchRunnable, 200);
    }

    /**
     * Displays the search results in {@link SearchResultsPreferenceFragment} for users to browse
     * and open them.
     *
     * @param results search results to display.
     */
    private void displayResultsFragment(SearchResults results) {
        mSearchRunnable = null;

        // TODO(jinsukkim): Group the results by their header.

        // Create a new instance of the fragment and pass the results
        mResultsFragment = new SearchResultsPreferenceFragment(results, this::onResultSelected);

        // Get the FragmentManager and replace the current fragment in the container
        FragmentManager fragmentManager = getSettingsFragmentManager();
        fragmentManager
                .beginTransaction()
                .replace(getViewIdForSearchDisplay(), mResultsFragment)
                .setReorderingAllowed(true)
                .commit();
    }

    /**
     * Open the PreferenceFragment chosen from the search results.
     *
     * @param preferenceFragment Settings fragment to show.
     * @param key The key of the chosen preference in the fragment.
     */
    private void onResultSelected(String preferenceFragment, String key) {
        try {
            EditText queryEdit = mActivity.findViewById(R.id.search_query);
            KeyboardUtils.hideAndroidSoftKeyboard(queryEdit);
            Class fragment = Class.forName(preferenceFragment);
            Constructor constructor = fragment.getConstructor();
            var pf = (PreferenceFragmentCompat) constructor.newInstance();
            FragmentManager fragmentManager = getSettingsFragmentManager();
            fragmentManager
                    .beginTransaction()
                    .replace(getViewIdForSearchDisplay(), pf)
                    .addToBackStack(FRAGMENT_TAG_RESULT)
                    .setReorderingAllowed(true)
                    .commit();

            // Scroll to the chosen preference after the new fragment is ready.
            fragmentManager.registerFragmentLifecycleCallbacks(
                    new FragmentManager.FragmentLifecycleCallbacks() {
                        @Override
                        public void onFragmentAttached(
                                @NonNull FragmentManager fm,
                                @NonNull Fragment f,
                                @NonNull Context context) {
                            mHandler.post(() -> pf.scrollToPreference(pf.findPreference(key)));
                            fm.unregisterFragmentLifecycleCallbacks(this);
                        }
                    },
                    false);
        } catch (ClassNotFoundException
                | InstantiationException
                | NoSuchMethodException
                | IllegalAccessException
                | InvocationTargetException e) {
            Log.e(TAG, "Search result fragment cannot be opened: " + preferenceFragment);
            return;
        }
        if (mFragmentState != FS_RESULTS) mFragmentState = FS_RESULTS;
    }
}
