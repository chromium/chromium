// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static androidx.annotation.VisibleForTesting.PRIVATE;

import static org.chromium.base.CallbackUtils.emptyRunnable;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Handler;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.transition.ChangeBounds;
import android.transition.Fade;
import android.transition.Transition;
import android.transition.TransitionListenerAdapter;
import android.transition.TransitionManager;
import android.transition.TransitionSet;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.DimenRes;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.ActionMenuView;
import androidx.appcompat.widget.SearchView;
import androidx.appcompat.widget.Toolbar;
import androidx.appcompat.widget.TooltipCompat;
import androidx.core.content.ContextCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceGroup.PreferencePositionCallback;
import androidx.recyclerview.widget.RecyclerView;
import androidx.slidingpanelayout.widget.SlidingPaneLayout;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.ui.KeyboardUtils;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.accessibility.settings.ChromeAccessibilitySettingsDelegate;
import org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.MultiColumnSettings;
import org.chromium.chrome.browser.site_settings.ChromeSiteSettingsDelegate;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettings;
import org.chromium.components.browser_ui.settings.search.SearchIndexProvider;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData.SearchResults;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.util.ToolbarUtils;
import org.chromium.components.browser_ui.widget.containment.ContainmentItemController;
import org.chromium.components.browser_ui.widget.containment.ContainmentItemDecoration;
import org.chromium.components.browser_ui.widget.containment.ContainmentViewStyler;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizerUtil;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.ui.UiUtils;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.DeviceFormFactor;
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
public class SettingsSearchCoordinator
        implements MultiColumnSettings.Observer, AccessibilityState.Listener {
    private static final String TAG = "SettingsSearch";

    public static final String RESULT_BACKSTACK = MainSettings.RESULT_BACKSTACK;
    public static final String RESULT_FRAGMENT = "search_result_fragment";
    public static final String EMPTY_FRAGMENT = "empty_fragment";

    private static final String KEY_FRAGMENT_STATE = "FragmentState";
    private static final String KEY_PANE_OPENED_BY_SEARCH = "PaneOpenedBySearch";
    private static final String KEY_QUERY = "Query";
    private static final String KEY_SELECTION_START = "SelectionStart";
    private static final String KEY_SELECTION_END = "SelectionEnd";
    private static final String KEY_FIRST_UI_ENTERED = "FirstUiEntered";
    private static final String KEY_RESULT_RETURNED = "ResultReturned";
    private static final String KEY_EXIT_REASON_LOGGED = "ExitReasonLogged";

    private final AppCompatActivity mActivity;
    private final BooleanSupplier mUseMultiColumnSupplier;
    private @Nullable final MultiColumnSettings mMultiColumnSettings;
    private final Map<PreferenceFragmentCompat, ContainmentItemDecoration> mItemDecorations;
    private final Handler mHandler = new Handler();
    private final Profile mProfile;
    private final Callback<Integer> mUpdateFirstVisibleTitle;
    private final MonotonicObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;

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

    // True while local search (language, site settings) UI is enabled, so that settings search
    // should remain hidden across configuration changes.
    private boolean mSuppressUi;

    // Used for histogram that logs the user behavior for search.
    // LINT.IfChange(ExitReason)
    @IntDef({
        ExitReason.CLICKED_RESULT,
        ExitReason.ABANDONED_RESULTS,
        ExitReason.ABANDONED_NORESULTS,
        ExitReason.COUNT
    })
    public @interface ExitReason {
        int CLICKED_RESULT = 0;
        int ABANDONED_RESULTS = 1;
        int ABANDONED_NORESULTS = 2;
        int COUNT = 3;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/settings/enums.xml:SettingsSearchExitReason)

    // Flags for metrics:
    // Whether the search UI is entered for the first time for the current settings activity
    // session. Used to record the event a user ever entered search since the settings is opened.
    private boolean mFirstUiEntered = true;

    // Whether the last search returns non-zero results.
    private boolean mResultReturned;

    // Whether the ExitReason histogram was already logged for a given query session, to avoid
    // double logging when exiting search.
    private boolean mExitReasonLogged;

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
        AccessibilityState.addListener(this);

        mActivity = activity;
        mUseMultiColumnSupplier = useMultiColumnSupplier;
        mMultiColumnSettings = multiColumnSettings;
        setFragmentState(FS_SETTINGS);
        mItemDecorations = itemDecorations;
        mProfile = profile;
        mUpdateFirstVisibleTitle = updateFirstVisibleTitle;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
    }

    /** Initializes search UI, sets up listeners, backpress action handler, etc. */
    @Initializer
    public void initializeSearchUi(@Nullable Bundle savedState) {
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
        searchBox.setOnClickListener(this::onClickSearchBox);
        TooltipCompat.setTooltipText(
                searchBox.findViewById(R.id.search_icon),
                mActivity.getString(R.string.search_in_settings_hint));

        View query = mActivity.findViewById(R.id.search_query_container);
        Drawable bg = ContextCompat.getDrawable(mActivity, R.drawable.pill_background);
        int tint = SemanticColorUtils.getSettingsContainerBackgroundColor(mActivity);
        if (!ChromeFeatureList.sAndroidSettingsContainment.isEnabled()) {
            tint = SemanticColorUtils.getColorSurfaceContainerHighest(mActivity);
        }
        bg.setTint(tint);
        searchBox.setBackground(bg);
        query.setBackground(bg);
        if (mMultiColumnSettings != null) {
            mHandler.post(this::initializeMultiColumnSearchUi);
        } else {
            observeFragmentForVisibilityChange();
        }

        EditText queryEdit = mActivity.findViewById(R.id.search_query);
        setUpQueryEdit(queryEdit);
        View backToSettings = mActivity.findViewById(R.id.back_arrow_icon);
        backToSettings.setOnClickListener(v -> handleBackAction());
        TooltipCompat.setTooltipText(backToSettings, mActivity.getString(R.string.back));
        mBackActionCallback =
                new OnBackPressedCallback(false) {
                    @Override
                    public void handleOnBackPressed() {
                        handleBackAction();
                    }
                };
        mActivity.getOnBackPressedDispatcher().addCallback(mActivity, mBackActionCallback);
        View clearText = query.findViewById(R.id.clear_text);
        clearText.setOnClickListener(v -> clearQueryText());
        TooltipCompat.setTooltipText(
                clearText, mActivity.getString(R.string.search_in_settings_clear_query));
        if (savedState != null) {
            int state = savedState.getInt(KEY_FRAGMENT_STATE);
            if (state == FS_SEARCH || state == FS_RESULTS) {
                enterSearchState(/* isRestored= */ true);
                if (state == FS_RESULTS) enterResultState();
                String queryText = savedState.getString(KEY_QUERY);
                if (!TextUtils.isEmpty(queryText)) {
                    queryEdit.setText(queryText);
                    int selectionStart = savedState.getInt(KEY_SELECTION_START);
                    int selectionEnd = savedState.getInt(KEY_SELECTION_END);
                    queryEdit.setSelection(selectionStart, selectionEnd);
                }
                restoreFragmentState();
            }
            mPaneOpenedBySearch = savedState.getBoolean(KEY_PANE_OPENED_BY_SEARCH);
            mFirstUiEntered = savedState.getBoolean(KEY_FIRST_UI_ENTERED);
            mResultReturned = savedState.getBoolean(KEY_RESULT_RETURNED);
            mExitReasonLogged = savedState.getBoolean(KEY_EXIT_REASON_LOGGED);
            mHandler.post(() -> showTitleTextView(true));
        }
        mHandler.post(this::restoreRecentSearches);
    }

    private void showTitleTextView(boolean show) {
        Toolbar actionBar = mActivity.findViewById(R.id.action_bar);
        assumeNonNull(ToolbarUtils.getTitleTextView(actionBar))
                .setVisibility(show ? View.VISIBLE : View.GONE);
    }

    private void onClickSearchBox(View view) {
        RecordHistogram.recordBooleanHistogram("Settings.Search.UiOpened", true);
        if (mFirstUiEntered) {
            RecordHistogram.recordBooleanHistogram("Settings.Search.UiOpenedPerSession", true);
            mFirstUiEntered = false;
        }

        enterSearchState(/* isRestored= */ false);
    }

    private void restoreFragmentState() {
        var fm = getSettingsFragmentManager();
        var emptyFragment = (EmptyFragment) fm.findFragmentByTag(EMPTY_FRAGMENT);
        if (emptyFragment != null) emptyFragment.setOpenHelpCenter(this::openHelpCenter);

        var fragment = (SearchResultsPreferenceFragment) fm.findFragmentByTag(RESULT_FRAGMENT);
        if (fragment != null) {
            fragment.setSelectedCallback(this::onResultSelected);
            if (fragment instanceof RecentSearchesFragment rsf) {
                rsf.setDeleteCallback(this::deleteRecentSearches);
            }
        }
        // The restored query text triggers the text listener to perform search, replaces
        // the restored fragment immediately with the same results, causing a flash. Removing
        // the search runnable prevents it.
        if (mSearchRunnable != null) mHandler.removeCallbacks(mSearchRunnable);
    }

    @Override
    public void onTitleUpdated() {
        boolean reset = (getSettingsFragmentManager().getBackStackEntryCount() == 0);
        if (reset && (mFragmentState == FS_SEARCH || mFragmentState == FS_RESULTS)) {
            exitSearchState(/* clearFragment= */ false);
        }
    }

    @Override
    public void onAccessibilityStateChanged(
            AccessibilityState.State oldAccessibilityState,
            AccessibilityState.State newAccessibilityState) {
        if (mActivity.isFinishing() || mActivity.isDestroyed()) return;

        // If #onSaveInstance has already been called, we cannot commit Fragment transactions. The
        // UI update is safe to skip since the user cannot see the search view in this state.
        if (getSettingsFragmentManager().isStateSaved()) return;

        if (!oldAccessibilityState.equals(newAccessibilityState)) {
            if (mIndexData != null) {
                mIndexData.setNeedsIndexing();
                initIndex();

                EditText queryEdit = mActivity.findViewById(R.id.search_query);
                if (queryEdit == null) return;

                if (mFragmentState == FS_SEARCH) {
                    queryEdit.requestFocus();
                    onQueryUpdated(queryEdit.getText().toString());
                }
            }
        }
    }

    private void clearQueryText() {
        EditText queryEdit = mActivity.findViewById(R.id.search_query);
        if (queryEdit.getText().toString().isEmpty()) return;

        if (mMultiColumnSettings != null && mUseMultiColumn && mFragmentState == FS_RESULTS) {
            // If clicked while displaying search results, get out of FS_RESULTS state.
            setFragmentState(FS_SEARCH);
            mActivity.findViewById(R.id.search_query_container).setVisibility(View.VISIBLE);
            showBackArrowInSingleColumnMode(false);
            getSettingsFragmentManager()
                    .popBackStackImmediate(
                            RESULT_BACKSTACK, FragmentManager.POP_BACK_STACK_INCLUSIVE);
        }
        queryEdit.setText("");
        updateClearTextButton(queryEdit.getText());
        if (RecentSearchQueue.getInstance().isEmpty()) {
            clearFragment(
                    R.drawable.settings_zero_state, /* addToBackStack= */ false, emptyRunnable());
        } else {
            displayRecentSearches();
        }
        queryEdit.requestFocus();
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
                    if (mFragmentState != FS_SETTINGS) return;
                    searchBox.setVisibility(isShowingMainSettings() ? View.VISIBLE : View.GONE);
                });

        // Controls search UI visibility in single-column mode.
        mMultiColumnSettings
                .getSlidingPaneLayout()
                .addPanelSlideListener(
                        new SlidingPaneLayout.SimplePanelSlideListener() {
                            @Override
                            public void onPanelOpened(View panel) {
                                if (mUseMultiColumn) return;

                                mMultiColumnSettings.getMainSettings().saveListState();
                                showUiInSingleColumn(searchBox, /* show= */ false);
                                disableBackgroundTalkbackNavigation();
                            }

                            @Override
                            public void onPanelClosed(View panel) {
                                if (mUseMultiColumn) return;

                                mMultiColumnSettings.getMainSettings().restoreListState();

                                // The detail panel can be force-closed immediately after we enter
                                // the search state + open the detail pane. Because
                                // SlidingPaneLayout uses smooth animations, a rapid-fire tap on
                                // the header can re-trigger the selection logic before the first
                                // transition finishes, effectively "stuttering" the pane back to
                                // a closed state. We cancel the operation, and revert the state
                                // back to FS_SETTINGS. See https://crbug.com/482946558.
                                if (mFragmentState == FS_SEARCH) {
                                    exitSearchState(/* clearFragment= */ false);
                                }
                                showUiInSingleColumn(searchBox, /* show= */ true);
                                disableBackgroundTalkbackNavigation();
                            }
                        });
        var fm = getSettingsFragmentManager();

        // Help menu/icon layout may change from Fragment to Fragment. Monitor the Fragment resume
        // event to update the search bar width in response.
        fm.registerFragmentLifecycleCallbacks(
                new FragmentManager.FragmentLifecycleCallbacks() {
                    @Override
                    public void onFragmentResumed(FragmentManager fm, Fragment f) {
                        updateSearchUiWidth();
                        maybeInitSearchResultsFragmentCallback(f);
                    }
                },
                false);

        fm.addOnBackStackChangedListener(this::disableBackgroundTalkbackNavigation);
        adjustTalkbackTraversalOrder(searchBox);
    }

    private void maybeInitSearchResultsFragmentCallback(Fragment f) {
        if (f instanceof SearchResultsPreferenceFragment srpf) {
            srpf.setSelectedCallback(this::onResultSelected);
        }
    }

    // Prevent TalkBack from navigating background fragments.
    private void disableBackgroundTalkbackNavigation() {
        // When a new Fragment is added to (as opposed to replaces) the existing Fragment, it makes
        // the existing one still technically "active" and "visible" in the view hierarchy, which is
        // why TalkBack navigates through it. Same applies when the detail pane slides in and sits
        // on top of the header pane - MainSettings in the header pane remains active, therefore
        // talkback would navigate through it. Navigation on all the invisible (e.g. background)
        // fragments should be disabled.
        // Note that MainSettings in multi-column mode, or single-column mode with the detail pane
        // out of the screen, should be enabled since it is visible.
        List<Fragment> fragments = getSettingsFragmentManager().getFragments();
        boolean isHeaderPaneVisible = isShowingMainSettings();
        for (int i = 0; i <= fragments.size() - 2; i++) {
            Fragment f = fragments.get(i);
            if (f == null || f.getView() == null) continue;
            boolean enable = (f.getClass() == MainSettings.class) && isHeaderPaneVisible;
            enableTalkbackNavigation(f.getView(), enable);
        }
    }

    private static void enableTalkbackNavigation(View view, boolean enable) {
        if (enable) {
            view.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
        } else {
            view.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        }
    }

    private void adjustTalkbackTraversalOrder(View searchUi) {
        if (mMultiColumnSettings == null) return;

        Toolbar toolbar = mActivity.findViewById(R.id.action_bar);
        View toolbarTitle = ToolbarUtils.getTitleTextView(toolbar);
        if (toolbarTitle == null) return;

        if (toolbarTitle.getId() == View.NO_ID) toolbarTitle.setId(View.generateViewId());
        View headerPane = mActivity.findViewById(R.id.preferences_header);

        // Multi-column mode adjusts the traversal order:
        // Before: Toolbar(icon > title > search UI > menu) > header pane > detail pane.
        // After:  Toolbar(icon > title) > header pane > Toolbar(search UI > menu) > detail pane.
        if (mUseMultiColumn) {
            headerPane.setAccessibilityTraversalAfter(toolbarTitle.getId());
            searchUi.setAccessibilityTraversalAfter(headerPane.getId());
        } else {
            headerPane.setAccessibilityTraversalAfter(View.NO_ID);
            View searchBox = mActivity.findViewById(R.id.search_box);
            View queryContainer = mActivity.findViewById(R.id.search_query_container);
            searchBox.setAccessibilityTraversalAfter(View.NO_ID);
            queryContainer.setAccessibilityTraversalAfter(View.NO_ID);
        }
    }

    private void observeFragmentForVisibilityChange() {
        getSettingsFragmentManager()
                .registerFragmentLifecycleCallbacks(
                        new FragmentManager.FragmentLifecycleCallbacks() {
                            @Override
                            public void onFragmentResumed(FragmentManager fm, Fragment f) {
                                View searchBox = mActivity.findViewById(R.id.search_box);
                                if (f instanceof MainSettings) {
                                    showUiInSingleColumn(searchBox, true);
                                } else if (f instanceof PreferenceFragmentCompat) {
                                    showUiInSingleColumn(searchBox, false);
                                }
                                maybeInitSearchResultsFragmentCallback(f);
                            }
                        },
                        false);
        updateSearchUiWidth();
    }

    private void showUiInSingleColumn(View searchBox, boolean show) {
        // Delay showing the UI until its width gets set. This mitigates the UI being seen
        // with a wrong width initially.
        if (show && searchBox.getLayoutParams().width == ViewGroup.LayoutParams.MATCH_PARENT) {
            mHandler.post(() -> showUiInSingleColumn(searchBox, show));
            return;
        }
        searchBox.setOnClickListener(v -> {}); // Temporary disables search during the animation
        Transition transition =
                new TransitionSet()
                        .addTransition(new Fade(show ? Fade.IN : Fade.OUT))
                        .addTransition(new ChangeBounds())
                        .setOrdering(TransitionSet.ORDERING_TOGETHER)
                        .addListener(
                                new TransitionListenerAdapter() {
                                    @Override
                                    public void onTransitionEnd(Transition transition) {
                                        searchBox.setOnClickListener(v -> onClickSearchBox(v));
                                        updateSearchUiWidth();
                                    }
                                });
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

    /**
     * Handle back press action when search UI is enabled.
     *
     * @return Whether the search coordinator consumed the action.
     */
    public boolean handleBackAction() {
        if (mFragmentState == FS_SETTINGS) {
            // Do nothing. Let the default back action handler take care of it.
            return false;
        } else if (mFragmentState == FS_SEARCH) {
            exitSearchState(/* clearFragment= */ true);
        } else if (mFragmentState == FS_RESULTS) {
            stepBackInResultState();
        } else {
            assert false : "Unreachable state.";
        }
        return true;
    }

    /** Returns the size in px for a given dimension resource ID. */
    private int getPixelSize(@DimenRes int resId) {
        return mActivity.getResources().getDimensionPixelSize(resId);
    }

    /**
     * Ensures the Settings search index is built and ready to use. Safe to call multiple times; it
     * will only build if necessary.
     */
    public static SettingsIndexData ensureIndexBuilt(Context context, Profile profile) {
        SettingsIndexData indexData = SettingsIndexData.getInstance();
        if (indexData == null) {
            indexData = SettingsIndexData.createInstance();
        }

        if (indexData.needsIndexing()) {
            buildIndexInternal(context, profile, indexData);
            indexData.resetNeedsIndexing();
        }

        return indexData;
    }

    /**
     * Initializes the in-memory search index for all settings. It uses the providers found in
     * {@link SearchIndexProviderRegistry.ALL_PROVIDERS}.
     */
    @Initializer
    @EnsuresNonNull("mIndexData")
    private void initIndex() {
        mIndexData = ensureIndexBuilt(mActivity, mProfile);
    }

    @VisibleForTesting
    static void buildIndexInternal(Context context, Profile profile, SettingsIndexData indexData) {
        // This is done to avoid duplicate entries when parsing XML.
        indexData.clear();

        List<SearchIndexProvider> providers = SearchIndexProviderRegistry.ALL_PROVIDERS;
        Map<String, SearchIndexProvider> providerMap = createProviderMap(providers);
        Set<String> processedFragments = new HashSet<>();

        String mainSettingsClassName = MainSettings.class.getName();
        SearchIndexProvider rootProvider = providerMap.get(mainSettingsClassName);

        // The root provider needs to be registered.
        assert rootProvider != null;

        rootProvider.registerFragmentHeaders(context, indexData, providerMap, processedFragments);

        for (SearchIndexProvider provider : providers) {
            if (provider instanceof ChromeBaseSearchIndexProvider chromeProvider) {
                chromeProvider.initPreferenceXml(context, profile, indexData, providerMap);
            } else {
                provider.initPreferenceXml(context, indexData, providerMap);
            }
        }

        // Allow providers to make runtime modifications (e.g., hide preferences). Sometimes we also
        // need to update the title of a pref.
        for (SearchIndexProvider provider : providers) {
            if (provider instanceof ChromeSearchIndexProvider chromeProvider) {
                chromeProvider.updateDynamicPreferences(context, indexData, profile);
            } else {
                provider.updateDynamicPreferences(context, indexData);
            }
        }

        // Some exceptions whose dynamic preferences cannot be updated via SearchIndexProvider
        // #updateDynamicPreferences.
        SiteSettings.updateDynamicPreferences(
                context, new ChromeSiteSettingsDelegate(context, profile), indexData);
        AccessibilitySettings.updateDynamicPreferences(
                context, new ChromeAccessibilitySettingsDelegate(profile), indexData);

        // Resolve headers and remove any orphaned entries.
        indexData.resolveIndex(mainSettingsClassName);
    }

    /**
     * Creates a map from a fragment's class name to its corresponding SearchIndexProvider for
     * efficient lookups.
     *
     * @param providers A list of {@link SearchIndexProvider}s.
     * @return A map where keys are fragment class names and values are the providers.
     */
    private static Map<String, SearchIndexProvider> createProviderMap(
            List<SearchIndexProvider> providers) {
        Map<String, SearchIndexProvider> providerMap = new HashMap<>();
        for (SearchIndexProvider provider : providers) {
            providerMap.put(provider.getPrefFragmentName(), provider);
        }
        return providerMap;
    }

    void enterSearchState(boolean isRestored) {
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
        queryEdit.setText("");
        mQueryEntered = false;
        mResultReturned = false;
        mExitReasonLogged = false;
        if (!isRestored) {
            // Focus is required only when we display a zero-state illustration, not when we simply
            // mean to clear fragment at activity restoration.
            queryEdit.requestFocus();
            boolean slideInDetailPane =
                    mMultiColumnSettings != null && !mUseMultiColumn && isShowingMainSettings();
            clearFragmentWithCallback(
                    R.drawable.settings_zero_state,
                    /* addToBackStack= */ true,
                    emptyRunnable(),
                    () -> {
                        if (slideInDetailPane) {
                            assumeNonNull(mMultiColumnSettings).slideInDetailPane();
                            mPaneOpenedBySearch = true;
                        }
                    });
        }
        KeyboardUtils.showKeyboard(queryEdit);
        setFragmentState(FS_SEARCH);
        mBackActionCallback.setEnabled(true);
        // When being restored, MultiColumnTitleUpdater restores the first-visible title index
        // from the saved bundle. Do not override it.
        if (!isRestored) {
            int stackCount = getSettingsFragmentManager().getBackStackEntryCount();
            mUpdateFirstVisibleTitle.onResult(stackCount + 1);
        }
        if (!mUseMultiColumn) {
            updateSingleColumnSearchUiWidth();
        }

        updateHelpMenuVisibility();
        adjustTalkbackTraversalOrder(queryContainer);
    }

    private void setFragmentState(int state) {
        mFragmentState = state;
        if (!mUseMultiColumn) showTitleTextView(state != FS_SEARCH);
    }

    private void showBackArrowInSingleColumnMode(boolean show) {
        if (mUseMultiColumn) return;

        assumeNonNull(mActivity.getSupportActionBar()).setDisplayHomeAsUpEnabled(show);
    }

    @VisibleForTesting(otherwise = PRIVATE)
    void exitSearchState(boolean clearFragment) {
        // Back action in search state. Restore the settings fragment and search UI.
        View searchBox = mActivity.findViewById(R.id.search_box);
        View queryContainer = mActivity.findViewById(R.id.search_query_container);
        queryContainer.setVisibility(View.GONE);

        // In single-column mode, search box visibility is handled by
        // SlidingPaneLayout#SimplePanelSlideListener set up in initializeMultiColumnSearchUi
        // for mutli-column settings, and by FragmentManager.FragmentLifecycleCallbacks set up
        // in observeFragmentForVisibilityChange for single-column settings.
        if (mUseMultiColumn) searchBox.setVisibility(View.VISIBLE);

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
                && !mUseMultiColumn
                && mMultiColumnSettings.isLayoutOpen()
                && mPaneOpenedBySearch) {
            mMultiColumnSettings.getSlidingPaneLayout().closePane();
            mPaneOpenedBySearch = false;
        }

        setFragmentState(FS_SETTINGS);
        mBackActionCallback.setEnabled(false);
        if (mUseMultiColumn) mUpdateFirstVisibleTitle.onResult(0);

        updateHelpMenuVisibility();
        adjustTalkbackTraversalOrder(searchBox);
        logExitReason();
    }

    private void logExitReason() {
        // Do not log if user exit search UI without ever entering queries.
        if (!mQueryEntered) return;

        if (!mResultReturned) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Settings.Search.ExitReason", ExitReason.ABANDONED_NORESULTS, ExitReason.COUNT);
        } else if (!mExitReasonLogged) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Settings.Search.ExitReason", ExitReason.ABANDONED_RESULTS, ExitReason.COUNT);
        }
    }

    /** Update the visibility of the help menu on the toolbar. */
    public void updateHelpMenuVisibility() {
        ViewGroup menuView = (ViewGroup) getHelpMenuView();
        if (menuView == null) {
            mHandler.post(this::updateHelpMenuVisibility);
            return;
        }

        menuView.post(
                () -> {
                    boolean show = shouldShowHelpMenu();
                    if (!show) {
                        // Should show menu if we have the search view.
                        for (int i = 0; i < menuView.getChildCount(); i++) {
                            View button = menuView.getChildAt(i);
                            if (button instanceof SearchView) {
                                show = true;
                                break;
                            }
                        }
                    }
                    menuView.setVisibility(show ? View.VISIBLE : View.GONE);
                    updateSearchUiWidth();
                });
    }

    private void stepBackInResultState() {
        FragmentManager fragmentManager = getSettingsFragmentManager();
        int stackCount = fragmentManager.getBackStackEntryCount();
        if (stackCount > 0) {
            // Switch back to 'search' state if we go all the way back to the fragment
            // where we display the search results.
            String topStackEntry = fragmentManager.getBackStackEntryAt(stackCount - 1).getName();
            if (TextUtils.equals(RESULT_BACKSTACK, topStackEntry)) {
                setFragmentState(FS_SEARCH);
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
    private void clearFragmentWithCallback(
            int imageId, boolean addToBackStack, Runnable openHelpCenter, Runnable callback) {
        Fragment fragment;
        if (RecentSearchQueue.getInstance().isEmpty()) {
            fragment = clearFragment(imageId, addToBackStack, openHelpCenter);
        } else {
            fragment = displayRecentSearches();
        }
        var fragmentManager = getSettingsFragmentManager();
        fragmentManager.registerFragmentLifecycleCallbacks(
                new FragmentManager.FragmentLifecycleCallbacks() {
                    @Override
                    public void onFragmentResumed(FragmentManager fm, Fragment f) {
                        if (f == fragment) {
                            fm.unregisterFragmentLifecycleCallbacks(this);
                            callback.run();
                        }
                    }
                },
                false);
    }

    @SuppressWarnings("ReferenceEquality")
    private EmptyFragment clearFragment(
            int imageId, boolean addToBackStack, Runnable openHelpCenter) {
        var fragmentManager = getSettingsFragmentManager();
        int viewId = getViewIdForSearchDisplay();
        var transaction = fragmentManager.beginTransaction();
        var emptyFragment = new EmptyFragment();
        emptyFragment.setImageSrc(imageId);
        emptyFragment.setOpenHelpCenter(openHelpCenter);
        transaction.setReorderingAllowed(true);
        transaction.replace(viewId, emptyFragment, EMPTY_FRAGMENT);
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
        return emptyFragment;
    }

    private void openHelpCenter() {
        HelpAndFeedbackLauncherImpl.getForProfile(mProfile)
                .show(mActivity, mActivity.getString(R.string.help_context_settings), null);
    }

    private Fragment displayRecentSearches() {
        var fragment = new RecentSearchesFragment();
        fragment.setPreferenceData(new ArrayList<>(RecentSearchQueue.getInstance().values()));
        fragment.setDeleteCallback(this::deleteRecentSearches);
        fragment.setSelectedCallback(this::onResultSelected);

        // Get the FragmentManager and replace the current fragment in the container
        FragmentManager fragmentManager = getSettingsFragmentManager();
        fragmentManager
                .beginTransaction()
                .replace(getViewIdForSearchDisplay(), fragment, RESULT_FRAGMENT)
                .addToBackStack(null)
                .setReorderingAllowed(true)
                .commit();
        return fragment;
    }

    public void deleteRecentSearches() {
        RecentSearchQueue.getInstance().clear();
        clearFragment(R.drawable.settings_zero_state, /* addToBackStack= */ false, emptyRunnable());
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
        boolean showBackIcon = mFragmentState != FS_SEARCH;
        if (mUseMultiColumn) {
            View searchBox = mActivity.findViewById(R.id.search_box);
            View query = mActivity.findViewById(R.id.search_query_container);
            View detailPane = mActivity.findViewById(R.id.preferences_detail);
            if (searchBox == null || query == null || detailPane == null) return;

            int settingsMargin = getPixelSize(R.dimen.settings_item_margin);
            int detailPaneWidth = detailPane.getWidth();
            if (detailPaneWidth == 0 || getHelpMenuView() == null) {
                mHandler.post(this::updateSearchUiWidth);
                return;
            }
            int width = detailPaneWidth - settingsMargin * 2 - getMenuWidth();
            updateView(searchBox, 0, settingsMargin, width);
            updateView(query, 0, settingsMargin, width);

            showBackIcon = true;
        } else {
            updateSingleColumnSearchUiWidth();
        }
        assumeNonNull(mActivity.getSupportActionBar()).setDisplayHomeAsUpEnabled(showBackIcon);
    }

    private int getMenuWidth() {
        View menuView = getHelpMenuView();
        return menuView != null && menuView.getVisibility() == View.VISIBLE
                ? menuView.getWidth()
                : 0;
    }

    private static void updateView(View view, int startMargin, int endMargin, int width) {
        var lp = (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        lp.setMarginStart(startMargin);
        lp.setMarginEnd(endMargin);
        lp.width = width;
        view.setLayoutParams(lp);
    }

    public void updateSingleColumnSearchUiWidth() {
        var menuView = getHelpMenuView();
        if (menuView == null) {
            mHandler.post(this::updateSingleColumnSearchUiWidth);
            return;
        }

        View appBar = mActivity.findViewById(R.id.app_bar_layout);
        appBar.post(
                () -> {
                    int appBarWidth = appBar.getWidth();
                    View searchBox = mActivity.findViewById(R.id.search_box);
                    View query = mActivity.findViewById(R.id.search_query_container);

                    int minWidePadding = getPixelSize(R.dimen.settings_wide_display_min_padding);
                    int margin =
                            ViewResizerUtil.computePaddingForWideDisplay(
                                    mActivity, searchBox, minWidePadding);
                    boolean isOnWideScreen =
                            margin > minWidePadding
                                    || DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity);
                    int menuWidth = getMenuWidth();
                    int searchBoxWidth;
                    int queryWidth;
                    int startMargin = margin;
                    int endMargin = margin;
                    if (isOnWideScreen) {
                        int itemMargin = getPixelSize(R.dimen.settings_item_margin);
                        margin += itemMargin;
                        searchBoxWidth = appBarWidth - margin * 2;
                        queryWidth = searchBoxWidth;
                        // The menu icon on the right pushes the UI to left. Adjust the margin.
                        startMargin += menuWidth - itemMargin;
                        endMargin -= menuWidth - itemMargin;
                    } else {
                        searchBoxWidth = appBarWidth - margin * 2;
                        // Only on narrow screens, query UI needs shrinking to avoid overlapping
                        // with menu icon on the right side.
                        queryWidth = searchBoxWidth - menuWidth;
                    }
                    if (searchBox != null) updateView(searchBox, margin, margin, searchBoxWidth);
                    if (query != null) updateView(query, startMargin, endMargin, queryWidth);
                });
    }

    /** Show/hide search bar UI. */
    public void showSearchBar(boolean show) {
        mSuppressUi = !show;

        // This is called to restore search box UI when it was hidden for local search.
        // Should not do this when we're displaying search results fragment (or query edit
        // is visible), since search box should remain hidden.
        if (!mUseMultiColumn
                || mFragmentState == FS_RESULTS
                || mActivity.findViewById(R.id.search_query_container).getVisibility()
                        == View.VISIBLE) {
            return;
        }

        View searchBox = mActivity.findViewById(R.id.search_box);
        if (show) {
            // Deals with the situation where configuration change occurs while suppressed.
            var lp = (Toolbar.LayoutParams) searchBox.getLayoutParams();
            lp.gravity = Gravity.END;
            searchBox.setLayoutParams(lp);
            mHandler.post(this::updateSearchUiWidth);
        }
        searchBox.setVisibility(show ? View.VISIBLE : View.GONE);
    }

    /**
     * Updates the UI layout for the changes in column mode/window width.
     *
     * @see {@link Activity#onConfigurationChanged(Configuration)}.
     */
    public void onConfigurationChanged(Configuration newConfig) {
        // mUseMultiColumnSupplier doesn't return the right, updated value immediately.
        // Observe the content view enclosing the PreferenceFragment for view tree update.
        var contentView = mActivity.findViewById(R.id.content);
        var listener =
                new OnGlobalLayoutListener() {
                    @Override
                    public void onGlobalLayout() {
                        contentView.getViewTreeObserver().removeOnGlobalLayoutListener(this);
                        onConfigurationChangedInternal();
                    }
                };
        contentView.getViewTreeObserver().addOnGlobalLayoutListener(listener);
    }

    private void onConfigurationChangedInternal() {
        boolean useMultiColumn = mUseMultiColumnSupplier.getAsBoolean();

        // Changing the layout restarts the activity, and in which case the help icon should remain
        // invisible if in the search or results view.
        updateHelpMenuVisibility();

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

        disableBackgroundTalkbackNavigation();
        View searchBox = mActivity.findViewById(R.id.search_box);
        UiUtils.removeViewFromParent(searchBox);
        View query = mActivity.findViewById(R.id.search_query_container);
        if (mUseMultiColumn) {
            ViewGroup actionBar = mActivity.findViewById(R.id.action_bar);
            setSearchBoxVerticalMargin(searchBox, true);
            assumeNonNull(actionBar).addView(searchBox);
            if (mFragmentState == FS_SETTINGS) {
                if (mSuppressUi) {
                    searchBox.setVisibility(View.GONE);
                } else {
                    searchBox.setVisibility(View.VISIBLE);
                    var lp = (Toolbar.LayoutParams) searchBox.getLayoutParams();
                    lp.gravity = Gravity.END;
                    searchBox.setLayoutParams(lp);
                }
            }
            if (mFragmentState == FS_RESULTS || mFragmentState == FS_SEARCH) {
                // Make the query edit UI visible which was hidden in single-column mode.
                // But not when local search is visible.
                if (mSuppressUi) {
                    query.setVisibility(View.GONE);
                } else {
                    query.setVisibility(View.VISIBLE);
                    var lp = (Toolbar.LayoutParams) query.getLayoutParams();
                    lp.gravity = Gravity.END;
                    query.setLayoutParams(lp);
                }
                var lp = (Toolbar.LayoutParams) searchBox.getLayoutParams();
                lp.gravity = Gravity.END;
                searchBox.setLayoutParams(lp);
            }
            adjustTalkbackTraversalOrder(isVisible(query) ? query : searchBox);
        } else {
            // Search bar goes beneath the toolbar (app_bar_layout) in single-column layout.
            ViewGroup appBarLayout = mActivity.findViewById(R.id.app_bar_layout);
            setSearchBoxVerticalMargin(searchBox, false);
            appBarLayout.addView(searchBox);
            boolean showingMain = isShowingMainSettings();
            if (showingMain) {
                // No need to check against |mSuppressUi| here since in single-column mode
                // search UI is always hidden except at main settings in which the UI is
                // never suppressed.
                if (mFragmentState == FS_SETTINGS) {
                    searchBox.setVisibility(View.VISIBLE);
                }
            } else {
                searchBox.setVisibility(View.GONE);
                query.setVisibility(View.GONE);
            }
            // Query edit UI should be hidden while we're browsing results.
            if (mFragmentState == FS_RESULTS) query.setVisibility(View.GONE);

            // The param is immaterial as we reset the order.
            adjustTalkbackTraversalOrder(searchBox);

            // When switching from 2-column to single-column mode, we may be at non-main
            // settings where search cannot be initiated and search UI should be hidden.
            // For UI consistency, we revert to default state (FS_SETTINGS) after clearing
            // the fragment to prevent fragments overlapping crbug.com/511065590.
            if (mFragmentState == FS_SEARCH && !showingMain) {
                exitSearchState(/* clearFragment= */ true);
                mUpdateFirstVisibleTitle.onResult(0);
                return;
            }

            if (mFragmentState == FS_SEARCH || mFragmentState == FS_RESULTS) {
                if (showingMain) {
                    // Results in the detail pane should be slided in to be visible if the pane
                    // is showing main settings.
                    mMultiColumnSettings.getSlidingPaneLayout().openPane();
                    mPaneOpenedBySearch = true;
                }
            }
        }
    }

    private static boolean isVisible(View view) {
        return view.getVisibility() == View.VISIBLE;
    }

    /**
     * Single source of truth for whether the help menu should be visible. Currently, it is visible
     * only when we are in the main Settings state, not during Search or Results.
     */
    private boolean shouldShowHelpMenu() {
        return mFragmentState == FS_SETTINGS;
    }

    private void setSearchBoxVerticalMargin(View searchBox, boolean multiColumn) {
        var lp = (ViewGroup.MarginLayoutParams) searchBox.getLayoutParams();
        lp.topMargin = multiColumn ? 0 : getPixelSize(R.dimen.settings_search_ui_top_margin);
        lp.bottomMargin = multiColumn ? 0 : getPixelSize(R.dimen.settings_search_ui_bottom_margin);
        searchBox.setLayoutParams(lp);
    }

    private void setUpQueryEdit(EditText queryEdit) {
        // Prevents the fullscreen "Extract Mode" in landscape.
        queryEdit.setImeOptions(EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_EXTRACT_UI);
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
                                    RESULT_BACKSTACK, FragmentManager.POP_BACK_STACK_INCLUSIVE);
                            setFragmentState(FS_SEARCH);
                        }
                    }
                });
        queryEdit.setAccessibilityDelegate(
                new View.AccessibilityDelegate() {
                    @Override
                    public void onInitializeAccessibilityNodeInfo(
                            View host, AccessibilityNodeInfo info) {
                        super.onInitializeAccessibilityNodeInfo(host, info);
                        String orgText = info.getText() == null ? "" : info.getText().toString();
                        info.setText(
                                mActivity.getString(R.string.search_in_settings_hint, orgText));
                    }
                });
    }

    private void onQueryUpdated(String query) {
        if (mFragmentState == FS_SEARCH) {
            performSearch(query, SettingsSearchCoordinator.this::displayResultsFragment);
        }
    }

    public void onTitleTapped(@Nullable String entryName) {
        // Tap on the title 'Search results' should set the state to 'SEARCH'.
        if (RESULT_BACKSTACK.equals(entryName)) setFragmentState(FS_SEARCH);
    }

    /**
     * Performs search by sending the query to search backend.
     *
     * @param query The search query the user entered.
     * @param callback The callback function to be executed when results are available.
     */
    void performSearch(String query, SearchCallback callback) {
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
    @VisibleForTesting(otherwise = PRIVATE)
    void displayResultsFragment(SearchResults results) {
        mSearchRunnable = null;
        mExitReasonLogged = false;
        mResultReturned = !results.getItems().isEmpty();

        if (!mResultReturned) {
            clearFragment(
                    R.drawable.settings_no_match,
                    /* addToBackStack= */ false,
                    this::openHelpCenter);
            return;
        }

        // Create a new instance of the fragment and pass the results
        SearchResultsPreferenceFragment resultsFragment = new SearchResultsPreferenceFragment();
        resultsFragment.setPreferenceData(results.groupByHeader());
        resultsFragment.setSelectedCallback(this::onResultSelected);

        // Get the FragmentManager and replace the current fragment in the container
        FragmentManager fragmentManager = getSettingsFragmentManager();
        fragmentManager
                .beginTransaction()
                .replace(getViewIdForSearchDisplay(), resultsFragment, RESULT_FRAGMENT)
                .setReorderingAllowed(true)
                .commit();
    }

    /**
     * Called when a preference is chosen from search results. Open the associated fragment or
     * activity, and if possible, scrolls to the chosen item and highlights it.
     *
     * @param preferenceFragment Package name of the Fragment containing the chosen setting.
     * @param highlight Whether or not to highlight the item.
     * @param entry Entry data from the index.
     */
    private void onResultSelected(
            @Nullable String preferenceFragment, boolean highlight, SettingsIndexData.Entry entry) {
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.Search.ExitReason", ExitReason.CLICKED_RESULT, ExitReason.COUNT);
        mExitReasonLogged = true; // Avoid double-logging when search is exited
        RecentSearchQueue.getInstance().add(entry);
        EditText queryEdit = mActivity.findViewById(R.id.search_query);
        KeyboardUtils.hideAndroidSoftKeyboard(queryEdit);
        if (preferenceFragment == null) {
            if (MainSettings.openSearchResult(
                    mActivity,
                    mProfile,
                    entry.key,
                    entry.extras,
                    mModalDialogManagerSupplier.asNonNull().get())) {
                enterResultState();
            }
            return;
        }

        try {
            Class<?> fragment = Class.forName(preferenceFragment);
            Constructor<?> constructor = fragment.getConstructor();
            var f = (Fragment) constructor.newInstance();
            f.setArguments(entry.extras);
            FragmentManager fragmentManager = getSettingsFragmentManager();
            fragmentManager
                    .beginTransaction()
                    .replace(getViewIdForSearchDisplay(), f)
                    .addToBackStack(RESULT_BACKSTACK)
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
                                                        pf,
                                                        entry.key,
                                                        entry.highlightKey,
                                                        entry.subViewPos));
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

        enterResultState();
    }

    private void enterResultState() {
        setFragmentState(FS_RESULTS);
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
        boolean containmentStyleDisabled = mItemDecorations.isEmpty();
        if (containmentStyleDisabled) {
            fragment.scrollToPreference(key);
        } else {
            // Calling #scrollToPreference directly doesn't work when if containment styled is
            // enabled. But OnScrollListener#onScrolled is always invoked after the recycler view
            // layout pass is completed. Use this timing to actually scroll the fragment to
            // the chosen preference.
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
        }
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

    public void onSaveInstanceState(Bundle outState) {
        outState.putInt(KEY_FRAGMENT_STATE, mFragmentState);
        outState.putBoolean(KEY_PANE_OPENED_BY_SEARCH, mPaneOpenedBySearch);
        outState.putBoolean(KEY_FIRST_UI_ENTERED, mFirstUiEntered);
        outState.putBoolean(KEY_RESULT_RETURNED, mResultReturned);
        outState.putBoolean(KEY_EXIT_REASON_LOGGED, mExitReasonLogged);
        EditText queryEdit = mActivity.findViewById(R.id.search_query);
        String queryText = queryEdit != null ? queryEdit.getText().toString() : null;
        if (!TextUtils.isEmpty(queryText)) {
            outState.putString(KEY_QUERY, queryText);
            outState.putInt(KEY_SELECTION_START, queryEdit.getSelectionStart());
            outState.putInt(KEY_SELECTION_END, queryEdit.getSelectionEnd());
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

        RecentSearchQueue.getInstance().persistToDiskAndReset();
        if (mFragmentState != FS_SETTINGS) {
            logExitReason();
        }
    }

    private void restoreRecentSearches() {
        try {
            RecentSearchQueue.getInstance().restoreFromDisk();
        } catch (IllegalArgumentException e) {
            ChromePureJavaExceptionReporter.reportJavaException(e);
        }
    }

    boolean hasRecentSearchEntriesForTesting() {
        return !RecentSearchQueue.getInstance().isEmpty();
    }
}
