// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.chromium.base.CallbackUtils.emptyRunnable;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.Handler;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.transition.ChangeBounds;
import android.transition.Fade;
import android.transition.Transition;
import android.transition.TransitionManager;
import android.transition.TransitionSet;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.DimenRes;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.ActionMenuView;
import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceGroup.PreferencePositionCallback;
import androidx.recyclerview.widget.RecyclerView;
import androidx.slidingpanelayout.widget.SlidingPaneLayout;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.ui.KeyboardUtils;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.accessibility.settings.ChromeAccessibilitySettingsDelegate;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.MultiColumnSettings;
import org.chromium.chrome.browser.site_settings.ChromeSiteSettingsDelegate;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettings;
import org.chromium.components.browser_ui.settings.search.SearchIndexProvider;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData.SearchResults;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.browser_ui.widget.containment.ContainmentItemController;
import org.chromium.components.browser_ui.widget.containment.ContainmentItemDecoration;
import org.chromium.components.browser_ui.widget.containment.ContainmentViewStyler;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizerUtil;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.BooleanSupplier;

/** The coordinator of search in Settings. TODO(jinsukkim): Build a proper MVC structure. */
@NullMarked
public class SettingsSearchCoordinator implements MultiColumnSettings.Observer {
    private static final String TAG = "SettingsSearch";

    public static final String FRAGMENT_TAG_RESULT = MainSettings.FRAGMENT_TAG_RESULT;

    private final AppCompatActivity mActivity;
    private final BooleanSupplier mUseMultiColumnSupplier;
    private @Nullable final MultiColumnSettings mMultiColumnSettings;
    private final Map<PreferenceFragmentCompat, ContainmentItemDecoration> mItemDecorations;
    private final Handler mHandler = new Handler();
    private final Profile mProfile;
    private final Callback<Integer> mUpdateFirstVisibleTitle;
    private final MonotonicObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

    private @Nullable Fragment mResultsFragment;
    private @Nullable Runnable mSearchRunnable;
    private @Nullable Runnable mRemoveResultChildViewListener;
    private @Nullable Runnable mTurnOffHighlight;
    private @Nullable ContainmentItemController mContainmentController;

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

    // Whether the detailed pane was slided open (therefore came into view over header pane) when
    // entering search mode. The pane needs opening in single-column mode viewing main settings
    // in header pane, so that the results fragment can always be shown in the detail one.
    private boolean mPaneOpenedBySearch;

    // True if multiple-column Fragment is enabled, and dual-column mode is on. Both the window
    // width and the feature flag condition should be met.
    private boolean mUseMultiColumn;

    private OnBackPressedCallback mBackActionCallback;

    // Set to {@code true} after queries are entered. Used to conditionally clear up the screen.
    private boolean mQueryEntered;
    private SettingsIndexData mIndexData;

    // True if empty fragment is showing.
    private boolean mShowingEmptyFragment;

    // Interface to communite with search backend and receive results asynchronously.
    public interface SearchCallback {
        /**
         * Invoked when the search results is available.
         *
         * @param results {@link SearchResults} containing the results.
         */
        void onSearchResults(SearchResults results);
    }

    // Information of the view to highlight.
    private static class HighlightInfo {
        public final View view;
        public final HighlightParams params;

        private HighlightInfo(View view, HighlightParams params) {
            this.view = view;
            this.params = params;
        }
    }

    /**
     * @param activity {@link SettingsActivity} object
     * @param useMultiColumnSupplier Supplier telling us whether the multi-column mode is on
     * @param multiColumnSettings {@link MultiColumnSettings} Fragment. Can be {@code null} unless
     *     the multi-column settings feature is enabled.
     * @param itemDecorations Containment style map used to apply the style to the highlighted item.
     * @param profile User profile object.
     * @param updateFirstVisibleTitle Callback used to set the first visible one of the titles. See
     *     {@link MultiColumnSettings#mFirstVisibleTitleIndex}.
     */
    public SettingsSearchCoordinator(
            AppCompatActivity activity,
            BooleanSupplier useMultiColumnSupplier,
            @Nullable MultiColumnSettings multiColumnSettings,
            Map<PreferenceFragmentCompat, ContainmentItemDecoration> itemDecorations,
            Profile profile,
            Callback<Integer> updateFirstVisibleTitle,
            MonotonicObservableSupplier<ModalDialogManager> modalDialogManagerSupplier) {
        mActivity = activity;
        mUseMultiColumnSupplier = useMultiColumnSupplier;
        mMultiColumnSettings = multiColumnSettings;
        mFragmentState = FS_SETTINGS;
        mItemDecorations = itemDecorations;
        mProfile = profile;
        mUpdateFirstVisibleTitle = updateFirstVisibleTitle;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
    }

    /** Initializes search UI, sets up listeners, backpress action handler, etc. */
    @Initializer
    public void initializeSearchUi() {
        mUseMultiColumn = mUseMultiColumnSupplier.getAsBoolean();
        // AppBarLayout(app_bar_layout)
        //         +-- FrameLayout
        //         |       +----- MaterialToolbar(action_bar)
        //         |                  +------ searchBox (dual-column)
        //         |                  +------ searchQuery
        //         +--- searchBox (single-column)
        Toolbar actionBar = mActivity.findViewById(R.id.action_bar);
        ViewGroup appBar = mActivity.findViewById(R.id.app_bar_layout);
        ViewGroup searchBoxParent = mUseMultiColumn ? actionBar : appBar;
        LayoutInflater.from(mActivity).inflate(R.layout.settings_search_box, searchBoxParent, true);
        LayoutInflater.from(mActivity).inflate(R.layout.settings_search_query, actionBar, true);
        View searchBox = mActivity.findViewById(R.id.search_box);
        setSearchBoxVerticalMargin(searchBox, mUseMultiColumn);
        searchBox.setOnClickListener(v -> enterSearchState());

        View query = mActivity.findViewById(R.id.search_query_container);
        if (mMultiColumnSettings != null) {
            mHandler.post(this::initializeMultiColumnSearchUi);
        } else {
            observeFragmentForVisibilityChange();
        }

        EditText queryEdit = mActivity.findViewById(R.id.search_query);
        setUpQueryEdit(queryEdit);
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
        query.findViewById(R.id.clear_text).setOnClickListener(v -> clearQueryText());
    }

    @Override
    public void onTitleUpdated() {
        boolean reset = (getSettingsFragmentManager().getBackStackEntryCount() == 0);
        if (reset && (mFragmentState == FS_SEARCH || mFragmentState == FS_RESULTS)) {
            exitSearchState(/* clearFragment= */ false);
        }
    }

    private void clearQueryText() {
        EditText queryEdit = mActivity.findViewById(R.id.search_query);
        if (queryEdit.getText().toString().isEmpty()) return;

        queryEdit.setText("");
        updateClearTextButton(queryEdit.getText());
        clearFragment(R.drawable.settings_zero_state, /* addToBackStack= */ false, emptyRunnable());
        KeyboardUtils.showKeyboard(queryEdit);
    }

    private void updateClearTextButton(CharSequence query) {
        int visibility = TextUtils.isEmpty(query.toString()) ? View.INVISIBLE : View.VISIBLE;
        mActivity.findViewById(R.id.clear_text).setVisibility(visibility);
    }

    private void initializeMultiColumnSearchUi() {
        assert mMultiColumnSettings != null;
        if (mMultiColumnSettings == null) return;

        updateSearchUiWidth();

        // Determine the search bar visibility.
        View searchBox = mActivity.findViewById(R.id.search_box);
        mHandler.post(
                () -> {
                    searchBox.setVisibility(isShowingMainSettings() ? View.VISIBLE : View.GONE);
                });

        // Controls search UI visibility in single-column mode.
        mMultiColumnSettings
                .getSlidingPaneLayout()
                .addPanelSlideListener(
                        new SlidingPaneLayout.SimplePanelSlideListener() {
                            @Override
                            public void onPanelOpened(View panel) {
                                showUiInSingleColumn(searchBox, /* show= */ false);
                            }

                            @Override
                            public void onPanelClosed(View panel) {
                                showUiInSingleColumn(searchBox, /* show= */ true);
                            }
                        });

        // Help menu/icon layout may change from Fragment to Fragment. Monitor the Fragment resume
        // event to update the search bar width in response.
        getSettingsFragmentManager()
                .registerFragmentLifecycleCallbacks(
                        new FragmentManager.FragmentLifecycleCallbacks() {
                            @Override
                            public void onFragmentResumed(FragmentManager fm, Fragment f) {
                                updateSearchUiWidth();
                            }
                        },
                        false);
    }

    private void observeFragmentForVisibilityChange() {
        getSettingsFragmentManager()
                .registerFragmentLifecycleCallbacks(
                        new FragmentManager.FragmentLifecycleCallbacks() {
                            @Override
                            public void onFragmentResumed(FragmentManager fm, Fragment f) {
                                View searchBox = mActivity.findViewById(R.id.search_box);
                                showUiInSingleColumn(searchBox, f.getClass() == MainSettings.class);
                            }
                        },
                        false);
        updateSearchUiWidth();
    }

    private void showUiInSingleColumn(View searchBox, boolean show) {
        Transition transition =
                new TransitionSet()
                        .addTransition(new Fade(show ? Fade.IN : Fade.OUT))
                        .addTransition(new ChangeBounds())
                        .setOrdering(TransitionSet.ORDERING_TOGETHER);
        var parentView = (ViewGroup) mActivity.findViewById(R.id.settings_activity);
        TransitionManager.beginDelayedTransition(parentView, transition);
        searchBox.setVisibility(show ? View.VISIBLE : View.GONE);
    }

    private boolean isShowingMainSettings() {
        assert mMultiColumnSettings != null : "Should be used with multi-column-settings#enabled";
        return mUseMultiColumn ? true : !mMultiColumnSettings.isLayoutOpen();
    }

    private @Nullable View getHelpMenuView() {
        Toolbar toolbar = mActivity.findViewById(R.id.action_bar);
        for (int i = 0; i < toolbar.getChildCount(); i++) {
            View child = toolbar.getChildAt(i);
            if (child instanceof ActionMenuView) return child;
        }
        return null;
    }

    private void handleBackAction() {
        if (mFragmentState == FS_SETTINGS) {
            // Do nothing. Let the default back action handler take care of it.
        } else if (mFragmentState == FS_SEARCH) {
            exitSearchState(/* clearFragment= */ true);
        } else if (mFragmentState == FS_RESULTS) {
            exitResultState();
        } else {
            assert false : "Unreachable state.";
        }
    }

    /** Returns the size in px for a given dimension resource ID. */
    private int getPixelSize(@DimenRes int resId) {
        return mActivity.getResources().getDimensionPixelSize(resId);
    }

    /**
     * Initializes the in-memory search index for all settings. It uses the providers found in
     * {@link SearchIndexProviderRegistry.ALL_PROVIDERS}.
     */
    @Initializer
    @EnsuresNonNull("mIndexData")
    private void initIndex() {
        if (mIndexData == null) {
            mIndexData = SettingsIndexData.createInstance();
        } else {
            if (!mIndexData.needsIndexing()) return;
        }

        // This is done to avoid duplicate entries when parsing XML.
        mIndexData.clear();

        List<SearchIndexProvider> providers = SearchIndexProviderRegistry.ALL_PROVIDERS;
        Map<String, SearchIndexProvider> providerMap = createProviderMap(providers);
        Set<String> processedFragments = new HashSet<>();

        String mainSettingsClassName = MainSettings.class.getName();
        SearchIndexProvider rootProvider = providerMap.get(mainSettingsClassName);

        // The root provider needs to be registered.
        assert rootProvider != null;

        rootProvider.registerFragmentHeaders(
                mActivity, mIndexData, providerMap, processedFragments);

        for (SearchIndexProvider provider : providers) {
            if (provider instanceof ChromeBaseSearchIndexProvider chromeProvider) {
                chromeProvider.initPreferenceXml(mActivity, mProfile, mIndexData, providerMap);
            } else {
                provider.initPreferenceXml(mActivity, mIndexData, providerMap);
            }
        }

        // Allow providers to make runtime modifications (e.g., hide preferences). Sometimes we also
        // need to update the title of a pref.
        for (SearchIndexProvider provider : providers) {
            if (provider instanceof ChromeSearchIndexProvider chromeProvider) {
                chromeProvider.updateDynamicPreferences(mActivity, mIndexData, mProfile);
            } else {
                provider.updateDynamicPreferences(mActivity, mIndexData);
            }
        }

        // Some exceptions whose dynamic preferences cannot be updated via SearchIndexProvider
        // #updateDynamicPreferences.
        SiteSettings.updateDynamicPreferences(
                mActivity, new ChromeSiteSettingsDelegate(mActivity, mProfile), mIndexData);
        AccessibilitySettings.updateDynamicPreferences(
                mActivity, new ChromeAccessibilitySettingsDelegate(mProfile), mIndexData);

        // Resolve headers and remove any orphaned entries.
        mIndexData.resolveIndex(mainSettingsClassName);

        mIndexData.resetNeedsIndexing();
    }

    /**
     * Creates a map from a fragment's class name to its corresponding SearchIndexProvider for
     * efficient lookups.
     *
     * @param providers A list of {@link SearchIndexProvider}s.
     * @return A map where keys are fragment class names and values are the providers.
     */
    private Map<String, SearchIndexProvider> createProviderMap(
            List<SearchIndexProvider> providers) {
        Map<String, SearchIndexProvider> providerMap = new HashMap<>();
        for (SearchIndexProvider provider : providers) {
            providerMap.put(provider.getPrefFragmentName(), provider);
        }
        return providerMap;
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
        showBackArrowInSingleColumnMode(false);
        EditText queryEdit = mActivity.findViewById(R.id.search_query);
        queryEdit.requestFocus();
        queryEdit.setText("");
        KeyboardUtils.showKeyboard(queryEdit);
        mQueryEntered = false;
        clearFragment(R.drawable.settings_zero_state, /* addToBackStack= */ true, emptyRunnable());
        mFragmentState = FS_SEARCH;
        mBackActionCallback.setEnabled(true);
        if (mMultiColumnSettings != null && isShowingMainSettings()) {
            mMultiColumnSettings.getSlidingPaneLayout().openPane();
            mPaneOpenedBySearch = true;
        }
        if (mUseMultiColumn) {
            int stackCount = getSettingsFragmentManager().getBackStackEntryCount();
            mUpdateFirstVisibleTitle.onResult(stackCount + 1);
        } else {
            updateSingleColumnSearchUiWidth();
        }
    }

    private void showBackArrowInSingleColumnMode(boolean show) {
        if (mUseMultiColumn) return;

        assumeNonNull(mActivity.getSupportActionBar()).setDisplayHomeAsUpEnabled(show);
    }

    private void exitSearchState(boolean clearFragment) {
        // Back action in search state. Restore the settings fragment and search UI.
        View searchBox = mActivity.findViewById(R.id.search_box);
        View queryContainer = mActivity.findViewById(R.id.search_query_container);
        queryContainer.setVisibility(View.GONE);
        searchBox.setVisibility(View.VISIBLE);
        mQueryEntered = false;
        showBackArrowInSingleColumnMode(true);
        EditText queryEdit = mActivity.findViewById(R.id.search_query);
        KeyboardUtils.hideAndroidSoftKeyboard(queryEdit);

        // Clearing the fragment before popping the back stack. Otherwise the existing
        // fragment is visible behind the popped one through the transparent background.
        if (clearFragment) {
            clearFragment(/* imageId= */ 0, /* addToBackStack= */ false, emptyRunnable());
        }
        getSettingsFragmentManager().popBackStack();
        if (mMultiColumnSettings != null
                && mMultiColumnSettings.isLayoutOpen()
                && mPaneOpenedBySearch) {
            mMultiColumnSettings.getSlidingPaneLayout().closePane();
            mPaneOpenedBySearch = false;
        }

        mFragmentState = FS_SETTINGS;
        mBackActionCallback.setEnabled(false);
        if (mUseMultiColumn) mUpdateFirstVisibleTitle.onResult(0);
        mShowingEmptyFragment = false;
    }

    private void exitResultState() {
        FragmentManager fragmentManager = getSettingsFragmentManager();
        int stackCount = fragmentManager.getBackStackEntryCount();
        if (stackCount > 0) {
            // Switch back to 'search' state if we go all the way back to the fragment
            // where we display the search results.
            String topStackEntry = fragmentManager.getBackStackEntryAt(stackCount - 1).getName();
            if (TextUtils.equals(FRAGMENT_TAG_RESULT, topStackEntry)) {
                mFragmentState = FS_SEARCH;
                mActivity.findViewById(R.id.search_query_container).setVisibility(View.VISIBLE);
                EditText queryEdit = mActivity.findViewById(R.id.search_query);
                queryEdit.requestFocus();
                // Search again to reflect the changes to the index that might have been made
                // while browsing results.
                if (mIndexData.shouldRefreshResult()) {
                    onQueryUpdated(queryEdit.getText().toString());
                    mIndexData.setRefreshResult(false);
                }
                showBackArrowInSingleColumnMode(false);
            }
        }
        // Clearing the fragment before popping the back stack. Otherwise the existing
        // fragment is visible behind the popped one through the transparent background.
        clearFragment(/* imageId= */ 0, /* addToBackStack= */ false, emptyRunnable());
        fragmentManager.popBackStack();
    }

    private FragmentManager getSettingsFragmentManager() {
        if (mMultiColumnSettings != null) {
            return mMultiColumnSettings.getChildFragmentManager();
        } else {
            return mActivity.getSupportFragmentManager();
        }
    }

    @SuppressWarnings("ReferenceEquality")
    private void clearFragment(int imageId, boolean addToBackStack, Runnable openHelpCenter) {
        var fragmentManager = getSettingsFragmentManager();
        int viewId = getViewIdForSearchDisplay();
        var transaction = fragmentManager.beginTransaction();
        var emptyFragment = new EmptyFragment();
        emptyFragment.init(imageId, openHelpCenter);
        transaction.setReorderingAllowed(true);
        transaction.replace(viewId, emptyFragment);
        transaction.setTransition(FragmentTransaction.TRANSIT_FRAGMENT_FADE);
        if (addToBackStack) transaction.addToBackStack(null);
        transaction.commit();

        if (imageId != 0) {
            fragmentManager.registerFragmentLifecycleCallbacks(
                    new FragmentManager.FragmentLifecycleCallbacks() {
                        @Override
                        public void onFragmentDetached(FragmentManager fm, Fragment f) {
                            if (f == emptyFragment) {
                                emptyFragment.clear();
                                fm.unregisterFragmentLifecycleCallbacks(this);
                            }
                        }
                    },
                    false);
        }
        mShowingEmptyFragment = true;
    }

    private void openHelpCenter() {
        HelpAndFeedbackLauncherImpl.getForProfile(mProfile)
                .show(mActivity, mActivity.getString(R.string.help_context_settings), null);
    }

    /** Returns the view ID where search results will be displayed. */
    private int getViewIdForSearchDisplay() {
        // We always show the search results in the detail pane when using MultiColumnSettings.
        // The detail pane could be in closed state in a single column layout (on phone in
        // portrait mode displaying the main settings). Then the detail pane needs to be slided
        // open first by the caller to make the pane visible.
        return mMultiColumnSettings != null ? R.id.preferences_detail : R.id.content;
    }

    // Update search UI width/location when multi-column settings fragment is enabled.
    private void updateSearchUiWidth() {
        View searchBox = mActivity.findViewById(R.id.search_box);
        View query = mActivity.findViewById(R.id.search_query_container);
        int settingsMargin = getPixelSize(R.dimen.settings_item_margin);
        boolean showBackIcon = mFragmentState != FS_SEARCH;
        View menuView = getHelpMenuView();
        if (mUseMultiColumn) {
            int detailPaneWidth = mActivity.findViewById(R.id.preferences_detail).getWidth();
            if (detailPaneWidth == 0 || menuView == null) {
                mHandler.post(this::updateSearchUiWidth);
                return;
            }
            int width = detailPaneWidth - settingsMargin * 2 - menuView.getWidth();
            updateView(searchBox, 0, settingsMargin, width);
            updateView(query, 0, settingsMargin, width);

            // |searchBox| loses android:layout_gravity attr after move. Sets it back.
            var lp = (Toolbar.LayoutParams) searchBox.getLayoutParams();
            lp.gravity = Gravity.END;
            searchBox.setLayoutParams(lp);

            showBackIcon = true;
        } else {
            if (menuView == null) {
                mHandler.post(this::updateSearchUiWidth);
                return;
            }
            updateSingleColumnSearchUiWidth();
        }
        assumeNonNull(mActivity.getSupportActionBar()).setDisplayHomeAsUpEnabled(showBackIcon);
    }

    private static void updateView(View view, int startMargin, int endMargin, int width) {
        var lp = (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        lp.setMarginStart(startMargin);
        lp.setMarginEnd(endMargin);
        lp.width = width;
        view.setLayoutParams(lp);
    }

    private void updateSingleColumnSearchUiWidth() {
        int appBarWidth = mActivity.findViewById(R.id.app_bar_layout).getWidth();
        View searchBox = mActivity.findViewById(R.id.search_box);
        View query = mActivity.findViewById(R.id.search_query_container);

        int minWidePadding = getPixelSize(R.dimen.settings_wide_display_min_padding);
        int padding =
                ViewResizerUtil.computePaddingForWideDisplay(mActivity, searchBox, minWidePadding);
        int settingsMargin = padding;
        if (padding > minWidePadding) settingsMargin += getPixelSize(R.dimen.settings_item_margin);

        int searchBoxWidth = appBarWidth - settingsMargin * 2;
        int queryWidth = searchBoxWidth - assumeNonNull(getHelpMenuView()).getWidth();
        updateView(searchBox, settingsMargin, settingsMargin, searchBoxWidth);
        updateView(query, settingsMargin, settingsMargin, queryWidth);
    }

    /** Show/hide search bar UI. */
    public void showSearchBar(boolean show) {
        if (!mUseMultiColumn) return;

        View searchBox = mActivity.findViewById(R.id.search_box);
        searchBox.setVisibility(show ? View.VISIBLE : View.GONE);
    }

    /**
     * Updates the UI layout for the changes in column mode/window width.
     *
     * @see {@link Activity#onConfigurationChanged(Configuration)}.
     */
    public void onConfigurationChanged(Configuration newConfig) {
        // mUseMultiColumnSupplier doesn't return the right, updated value immediately.
        // Posting the job to the next slot fixes it.
        mHandler.post(this::onConfigurationChangedInternal);
    }

    private void onConfigurationChangedInternal() {
        boolean useMultiColumn = mUseMultiColumnSupplier.getAsBoolean();

        if (useMultiColumn == mUseMultiColumn) {
            // Resizing/rotation could only change the window width. Adjust search bar UI in
            // response to the header/detail pane width.
            updateSearchUiWidth();
            return;
        }

        // Single vs. multi-column mode got switched.
        mUseMultiColumn = useMultiColumn;

        if (mMultiColumnSettings != null) {
            // Needs posting for the layout pass to be complete so that
            // |mMultiColumnSettings.isLayoutOpen()| returns the right result.
            mHandler.post(
                    () -> {
                        switchSearchUiLayout();
                        updateSearchUiWidth();
                    });
        } else {
            updateSearchUiWidth();
        }
    }

    /**
     * Adjust the UI when the layout (single vs. dual) switches. Used for multi-column settings
     * fragment only.
     * <li>Moves the search bar around to fit the layout
     * <li>Hide/show search bar - in single column mode, show it only in main settings
     * <li>Open the detail pane if needed.
     */
    private void switchSearchUiLayout() {
        assert mMultiColumnSettings != null : "Should be used for multi-column fragment only";
        if (mMultiColumnSettings == null) return;

        View searchBox = mActivity.findViewById(R.id.search_box);
        UiUtils.removeViewFromParent(searchBox);
        View query = mActivity.findViewById(R.id.search_query_container);
        if (mUseMultiColumn) {
            ViewGroup actionBar = mActivity.findViewById(R.id.action_bar);
            setSearchBoxVerticalMargin(searchBox, true);
            assumeNonNull(actionBar).addView(searchBox);
            if (mFragmentState == FS_SETTINGS) {
                searchBox.setVisibility(View.VISIBLE);
                var lp = (Toolbar.LayoutParams) searchBox.getLayoutParams();
                lp.gravity = Gravity.END;
                searchBox.setLayoutParams(lp);
            }
            if (mFragmentState == FS_RESULTS || mFragmentState == FS_SEARCH) {
                // Make the query edit UI visible which was hidden in single-column mode.
                query.setVisibility(View.VISIBLE);
                var lp = (Toolbar.LayoutParams) query.getLayoutParams();
                lp.gravity = Gravity.END;
                query.setLayoutParams(lp);
            }
        } else {
            // Search bar goes beneath the toolbar (app_bar_layout) in single-column layout.
            ViewGroup appBarLayout = mActivity.findViewById(R.id.app_bar_layout);
            setSearchBoxVerticalMargin(searchBox, false);
            appBarLayout.addView(searchBox);
            if (!isShowingMainSettings()) {
                searchBox.setVisibility(View.GONE);
                query.setVisibility(View.GONE);
            }
            // Query edit UI should be hidden while we're browsing results.
            if (mFragmentState == FS_RESULTS) query.setVisibility(View.GONE);

            // In single mode we end up at non-main settings where search cannot be initiated.
            // Keeping the empty fragment in that state is confusing and misleading. To sort
            // out the inconsistency, we revert to default state (FS_SETTINGS);
            if (mFragmentState == FS_SEARCH && mShowingEmptyFragment) {
                exitSearchState(/* clearFragment= */ false);
                mUpdateFirstVisibleTitle.onResult(0);
                return;
            }

            if (mFragmentState == FS_SEARCH || mFragmentState == FS_RESULTS) {
                if (isShowingMainSettings()) {
                    // Results in the detail pane should be slided in to be visible if the pane
                    // is showing main settings.
                    mMultiColumnSettings.getSlidingPaneLayout().openPane();
                    mPaneOpenedBySearch = true;
                }
            }
        }
    }

    private void setSearchBoxVerticalMargin(View searchBox, boolean multiColumn) {
        var lp = (ViewGroup.MarginLayoutParams) searchBox.getLayoutParams();
        lp.topMargin = multiColumn ? 0 : getPixelSize(R.dimen.settings_search_ui_top_margin);
        lp.bottomMargin = multiColumn ? 0 : getPixelSize(R.dimen.settings_search_ui_bottom_margin);
        searchBox.setLayoutParams(lp);
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
                        updateClearTextButton(s);
                        onQueryUpdated(s.toString().trim());
                    }
                });
        queryEdit.setOnFocusChangeListener(
                new View.OnFocusChangeListener() {
                    @Override
                    public void onFocusChange(View v, boolean hasFocus) {
                        if (hasFocus && mFragmentState == FS_RESULTS) {
                            // When tapping on search UI while browsing search results, pop the
                            // backstacks all the way back to showing the search result fragment.
                            FragmentManager fragmentManager = getSettingsFragmentManager();
                            fragmentManager.popBackStack(
                                    FRAGMENT_TAG_RESULT, FragmentManager.POP_BACK_STACK_INCLUSIVE);
                            mFragmentState = FS_SEARCH;
                        }
                    }
                });
    }

    private void onQueryUpdated(String query) {
        performSearch(query, SettingsSearchCoordinator.this::displayResultsFragment);
    }

    public void onTitleTapped(@Nullable String entryName) {
        // Tap on the title 'Search results' should set the state to 'SEARCH'.
        if (FRAGMENT_TAG_RESULT.equals(entryName)) mFragmentState = FS_SEARCH;
    }

    /**
     * Performs search by sending the query to search backend.
     *
     * @param query The search query the user entered.
     * @param callback The callback function to be executed when results are available.
     */
    private void performSearch(String query, SearchCallback callback) {
        if (mSearchRunnable != null) {
            // Debouncing to avoid initiating search for each keystroke entered fast.
            // We sets some delay before initiating search (see postDelayed() below) so that
            // we can perform search only when user is likely to have typed the entire query.
            mHandler.removeCallbacks(mSearchRunnable);
        }
        if (!query.isEmpty()) {
            mQueryEntered = true;
            mSearchRunnable = () -> callback.onSearchResults(mIndexData.search(query));
            mHandler.postDelayed(mSearchRunnable, 200);
        } else if (mQueryEntered) {
            // Do this only after a query has been entered at least once.
            clearFragment(
                    R.drawable.settings_zero_state, /* addToBackStack= */ false, emptyRunnable());
        }
    }

    /**
     * Displays the search results in {@link SearchResultsPreferenceFragment} for users to browse
     * and open them.
     *
     * @param results search results to display.
     */
    private void displayResultsFragment(SearchResults results) {
        mSearchRunnable = null;

        if (results.getItems().isEmpty()) {
            clearFragment(
                    R.drawable.settings_no_match,
                    /* addToBackStack= */ false,
                    this::openHelpCenter);
            return;
        }
        // Create a new instance of the fragment and pass the results
        mResultsFragment =
                new SearchResultsPreferenceFragment(
                        results.groupByHeader(), this::onResultSelected);

        // Get the FragmentManager and replace the current fragment in the container
        FragmentManager fragmentManager = getSettingsFragmentManager();
        fragmentManager
                .beginTransaction()
                .replace(getViewIdForSearchDisplay(), mResultsFragment)
                .setReorderingAllowed(true)
                .commit();
        mShowingEmptyFragment = false;
    }

    /**
     * Called when a preference is chosen from search results. Open the associated fragment or
     * activity, and if possible, scrolls to the chosen item and highlights it.
     *
     * @param preferenceFragment Settings fragment to show.
     * @param key The key of the chosen preference in the fragment.
     * @param extras The additional args required to launch the pref.
     * @param highlight Whether or not to scroll and highlight the item.
     * @param highlightKey The key to highlight if it is different from {@code key}.
     * @param subViewPos Position of the view to highlight among the child views.
     */
    private void onResultSelected(
            @Nullable String preferenceFragment,
            String key,
            Bundle extras,
            boolean highlight,
            @Nullable String highlightKey,
            int subViewPos) {
        EditText queryEdit = mActivity.findViewById(R.id.search_query);
        KeyboardUtils.hideAndroidSoftKeyboard(queryEdit);
        if (preferenceFragment == null) {
            if (MainSettings.openSearchResult(
                    mActivity, mProfile, key, extras, mModalDialogManagerSupplier.asNonNull())) {
                enterSearchResultState();
            }
            return;
        }

        try {
            Class fragment = Class.forName(preferenceFragment);
            Constructor constructor = fragment.getConstructor();
            var f = (Fragment) constructor.newInstance();
            f.setArguments(extras);
            FragmentManager fragmentManager = getSettingsFragmentManager();
            fragmentManager
                    .beginTransaction()
                    .replace(getViewIdForSearchDisplay(), f)
                    .addToBackStack(FRAGMENT_TAG_RESULT)
                    .setReorderingAllowed(true)
                    .commit();

            // Scroll to the chosen preference after the new fragment is ready.
            if (highlight && (f instanceof PreferenceFragmentCompat pf)) {
                fragmentManager.registerFragmentLifecycleCallbacks(
                        new FragmentManager.FragmentLifecycleCallbacks() {
                            @Override
                            public void onFragmentAttached(
                                    FragmentManager fm, Fragment f, Context context) {
                                mHandler.post(
                                        () ->
                                                scrollAndHighlightItem(
                                                        pf, key, highlightKey, subViewPos));
                                fm.unregisterFragmentLifecycleCallbacks(this);
                            }
                        },
                        false);
            }
        } catch (ClassNotFoundException
                | InstantiationException
                | NoSuchMethodException
                | IllegalAccessException
                | InvocationTargetException e) {
            Log.e(TAG, "Search result fragment cannot be opened: " + preferenceFragment);
            return;
        }

        enterSearchResultState();
    }

    private void enterSearchResultState() {
        mFragmentState = FS_RESULTS;
        if (mUseMultiColumn) {
            mActivity.findViewById(R.id.search_query).clearFocus();
        } else {
            // In single-column mode, search UI is hidden and title is shown instead in the toolbar.
            mActivity.findViewById(R.id.search_query_container).setVisibility(View.GONE);
        }
        showBackArrowInSingleColumnMode(true);
        if (mTurnOffHighlight != null) {
            mTurnOffHighlight.run();
            mTurnOffHighlight = null;
        }
    }

    private void scrollAndHighlightItem(
            PreferenceFragmentCompat fragment,
            String entryKey,
            @Nullable String highlightKey,
            int subViewPos) {
        RecyclerView listView = fragment.getListView();
        assert listView.getAdapter() instanceof PreferencePositionCallback
                : "Recycler adapter must implement PreferencePositionCallback";
        var listAdapter = (PreferencePositionCallback) listView.getAdapter();
        boolean highlightSubView = highlightKey != null;
        String key = assumeNonNull(highlightSubView ? highlightKey : entryKey);

        // Zero-based position of the preference view in listView.
        int pos = listAdapter.getPreferenceAdapterPosition(key);
        if (pos < 0) {
            // Fragment that builds preferences dynamically (not with an xml resource but using
            // APIs) is not ready to return the right position of the item to highlight and scroll
            // to, even though the associated view would already have been attached. Take a
            // different approach to do the scrolling and highlighting i.e. wait a few more
            // layout passes for the view holder to be available.
            mHandler.post(
                    () ->
                            scrollAndHighlightDynamicPref(
                                    fragment, key, highlightSubView, subViewPos));
            return;
        }
        mRemoveResultChildViewListener = null;
        listView.addOnChildAttachStateChangeListener(
                new RecyclerView.OnChildAttachStateChangeListener() {
                    @Override
                    public void onChildViewAttachedToWindow(View view) {
                        // |attach| events for a preference view may be invoked multiple times,
                        // intertwined with |detach| in close succession. We should use the last
                        // event to highlight the corresponding preference view. The listener
                        // is removed after that.
                        var viewHolder = fragment.getListView().getChildViewHolder(view);
                        if (pos == viewHolder.getBindingAdapterPosition()) {
                            if (mRemoveResultChildViewListener != null) {
                                mHandler.removeCallbacks(mRemoveResultChildViewListener);
                            }
                            mRemoveResultChildViewListener =
                                    () -> {
                                        highlightItem(
                                                fragment, view, pos, highlightSubView, subViewPos);
                                        listView.removeOnChildAttachStateChangeListener(this);
                                        mRemoveResultChildViewListener = null;
                                    };
                            mHandler.postDelayed(mRemoveResultChildViewListener, 200);
                        }
                    }

                    @Override
                    public void onChildViewDetachedFromWindow(View view) {}
                });
        scrollToPref(fragment, key);
    }

    private void scrollAndHighlightDynamicPref(
            PreferenceFragmentCompat fragment,
            String key,
            boolean highlightSubView,
            int subViewPos) {
        RecyclerView listView = fragment.getListView();
        if (listView == null) return;

        var listAdapter = (PreferencePositionCallback) listView.getAdapter();
        int pos = assumeNonNull(listAdapter).getPreferenceAdapterPosition(key);
        var viewHolder = listView.findViewHolderForAdapterPosition(pos);
        if (viewHolder == null) {
            mHandler.post(
                    () ->
                            scrollAndHighlightDynamicPref(
                                    fragment, key, highlightSubView, subViewPos));
        } else {
            highlightItem(fragment, viewHolder.itemView, pos, highlightSubView, subViewPos);
            scrollToPref(fragment, key);
        }
    }

    private void highlightItem(
            PreferenceFragmentCompat fragment,
            View view,
            int pos,
            boolean highlightSubView,
            int viewPos) {
        var info = getHighlightInfo(fragment, view, pos, highlightSubView, viewPos);
        ViewHighlighter.turnOnHighlight(info.view, info.params);
        mHandler.post(
                () -> {
                    mTurnOffHighlight = () -> ViewHighlighter.turnOffHighlight(info.view);
                });
    }

    private void scrollToPref(PreferenceFragmentCompat fragment, String key) {
        RecyclerView listView = fragment.getListView();
        // OnScrollListener#onScrolled is always invoked after the recycler view layout pass
        // is completed. Use this timing to scroll the preference. The listener is only meant
        // to run once to scroll to the preference, and then be removed.
        listView.addOnScrollListener(
                new RecyclerView.OnScrollListener() {
                    @Override
                    public void onScrollStateChanged(RecyclerView recyclerView, int newState) {}

                    @Override
                    public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                        fragment.scrollToPreference(key);
                        listView.removeOnScrollListener(this);
                    }
                });
        listView.addOnItemTouchListener(
                new RecyclerView.SimpleOnItemTouchListener() {
                    @Override
                    public boolean onInterceptTouchEvent(RecyclerView recyclerView, MotionEvent e) {
                        if (mTurnOffHighlight != null) {
                            mTurnOffHighlight.run();
                            mTurnOffHighlight = null;
                            listView.removeOnItemTouchListener(this);
                        }
                        return false;
                    }
                });
    }

    private HighlightInfo getHighlightInfo(
            PreferenceFragmentCompat fragment,
            View view,
            int pos,
            boolean highlightSubView,
            int subViewPos) {
        var params = new HighlightParams(HighlightShape.RECTANGLE);
        var defaultRes = new HighlightInfo(view, params);
        if (highlightSubView) {
            List<View> views = new ArrayList<>();
            ContainmentViewStyler.recursivelyFindStyledViews(view, views);
            if (views.isEmpty() || subViewPos >= views.size()) return defaultRes;

            if (mContainmentController == null) {
                mContainmentController = new ContainmentItemController(mActivity);
            }
            var style = mContainmentController.generateViewStyles(views).get(subViewPos);
            params.setTopCornerRadius((int) style.getTopRadius());
            params.setBottomCornerRadius((int) style.getBottomRadius());
            return new HighlightInfo(views.get(subViewPos), params);
        } else {
            var itemDecoration = mItemDecorations.get(fragment);
            if (itemDecoration == null) return defaultRes;

            var style = itemDecoration.getContainerStyle(pos);
            if (style == null) return defaultRes;

            params.setTopCornerRadius((int) style.getTopRadius());
            params.setBottomCornerRadius((int) style.getBottomRadius());
            return defaultRes;
        }
    }

    public void destroy() {
        // Title supplier should be nulled out as we step out of Settings for cleanup.
        SearchResultsPreferenceFragment.reset();
        if (mIndexData != null) {
            SettingsIndexData.reset();
        }
        mHandler.removeCallbacksAndMessages(null);
        mContainmentController = null;
    }
}
