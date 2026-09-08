// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.IntDef;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceHeaderFragmentCompat;
import androidx.slidingpanelayout.widget.SlidingPaneLayout;

import org.chromium.base.ObserverList;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.search.EmptyFragment;
import org.chromium.chrome.browser.settings.search.SettingsSearchCoordinator;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.ui.KeyboardVisibilityDelegate;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Preference container implementation for SettingsActivity in multi-column mode. */
@NullMarked
public class MultiColumnSettings extends PreferenceHeaderFragmentCompat
        implements ProfileDependentSetting {

    public interface Observer {
        /** Called when detailed pane title is updated. */
        default void onTitleUpdated() {}

        /**
         * Called when the menu layout is updated. I.e. - Switching between the single pane mode and
         * the two pane mode. - In the two pane mode, size change of the header layout.
         */
        default void onHeaderLayoutUpdated() {}

        /**
         * Called when the detail pane layout is updated i.e. its width is updated as the window is
         * resized. This is only effective in two pane mode.
         */
        default void onDetailLayoutUpdated() {}

        /** Called when the sliding state is updated. */
        default void onSlideStateUpdated(@SlideState int newState) {}
    }

    /** Represents the current state of sliding pane. */
    @IntDef({SlideState.CLOSING, SlideState.CLOSED, SlideState.OPENING, SlideState.OPENED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SlideState {
        int CLOSING = 0;
        int CLOSED = 1;
        int OPENING = 2;
        int OPENED = 3;
    }

    /** Caches the view of the detail panel. */
    private View mDetailView;

    private @Nullable MainSettings mMainSettings;

    private boolean mCanBeBackToMain;

    private SlideStateTracker mSlideStateTracker;

    private InnerOnBackPressedCallback mOnBackPressedCallback;

    private @Nullable Runnable mOnCreateViewRunnable;

    private @Nullable Intent mPendingFragmentIntent;

    private final ObserverList<Observer> mObservers = new ObserverList<>();

    private final FragmentTracker mFragmentTracker = new FragmentTracker(mObservers);

    private @Nullable Profile mProfile;

    private @Nullable Context mThemedContext;

    private @Nullable String mInitialUrl;

    @Override
    public void onAttach(Context context) {
        // Traditional settings has the theme applied at the activity level.
        if (!SettingsInTab.isEnabled()) {
            super.onAttach(context);
            return;
        }
        // Settings in a tab must apply the theme at a fragment level.
        mThemedContext = new ContextThemeWrapper(context, R.style.ThemeOverlay_Chromium_Settings);
        super.onAttach(mThemedContext);
    }

    @Override
    public Context getContext() {
        return mThemedContext != null ? mThemedContext : assumeNonNull(super.getContext());
    }

    @Override
    public LayoutInflater onGetLayoutInflater(@Nullable Bundle savedInstanceState) {
        LayoutInflater inflater = super.onGetLayoutInflater(savedInstanceState);
        // Ensure we use the themed context if available.
        return inflater.cloneInContext(getContext());
    }

    @Override
    public PreferenceFragmentCompat onCreatePreferenceHeader() {
        // Main menu, which is the first page in one column mode (i.e. window is
        // small enough), or shown at left side pane in two column mode.
        // Note that this method (and onCreateInitialDetailFragment) is not invoked when
        // SettingsActivity restarts, since this method is typically used to define or
        // inflate the initial hierarchy of headers (the left pane). During restoration,
        // the FragmentManager automatically restores the existing child fragments (the
        // left list pane and the right detail pane) from the saved state. Rerunning this
        // method would overwrite or duplicate the restored fragment state.
        mMainSettings = new MainSettings();

        return mMainSettings;
    }

    public MainSettings getMainSettings() {
        if (mMainSettings == null) mMainSettings = new MainSettings();
        return mMainSettings;
    }

    /** Sets the initial settings URL for tab-based navigation. */
    public void setInitialUrl(@Nullable String initialUrl) {
        mInitialUrl = initialUrl;
    }

    /** Returns the initial settings URL for tab-based navigation. */
    public @Nullable String getInitialUrl() {
        return mInitialUrl;
    }

    // Fragment data passed as extras of Intent via SettingsNavigation.
    private static class FragmentData {
        public final @Nullable Fragment fragment;
        public final boolean addToBackStack;
        public final @Nullable String tag;

        FragmentData(@Nullable Fragment fragment, boolean addToBackStack, @Nullable String tag) {
            this.fragment = fragment;
            this.addToBackStack = addToBackStack;
            this.tag = tag;
        }
    }

    @Override
    public @Nullable Fragment onCreateInitialDetailFragment() {
        // Look at if there is a pending Intent and use it if it is.
        // Otherwise fallback to the original logic, i.e. use the first item in the main menu.
        FragmentData processed = processPendingFragmentIntent();
        if (processed != null) {
            SlidingPaneLayout slidingPane = getSlidingPaneLayoutOrNull();
            if (slidingPane != null
                    && processed.fragment != null
                    && !(processed.fragment instanceof MainSettings)) {
                slidingPane.openPane();
            }
            return processed.fragment;
        }

        // Under SettingsInTab mode, check if an initial settings URL was
        // set (e.g. "chrome://settings/appearance"). If present, instantiate
        // the target fragment directly as the initial detail fragment rather
        // than defaulting to Account/GoogleServices.
        if (ChromeFeatureList.sSettingsInTabUrlNav.isEnabled() && mInitialUrl != null) {
            String initialUrl = mInitialUrl;
            mInitialUrl = null;
            var fragmentClass = SettingsFragmentRegistry.getFragmentClassForUrl(initialUrl);

            if (fragmentClass != null && !MainSettings.class.equals(fragmentClass)) {
                Bundle args = SettingsFragmentRegistry.parseUrlArguments(initialUrl);
                Fragment initialDetailFragment =
                        Fragment.instantiate(requireContext(), fragmentClass.getName(), args);

                SlidingPaneLayout slidingPane = getSlidingPaneLayoutOrNull();
                if (slidingPane != null) {
                    slidingPane.openPane();
                }
                return initialDetailFragment;
            }
        }

        // When SettingsInTab is enabled in single-column mode, do not instantiate an initial detail
        // fragment if no sub-fragment intent was specified. Returning null prevents
        // PreferenceHeaderFragmentCompat from calling openPane() on SlidingPaneLayout, keeping
        // MainSettings displayed as the top-level root settings page, with no detail fragment. In
        // two-column mode, fallback to super.onCreateInitialDetailFragment() to populate the
        // default detail pane.
        if (SettingsInTab.isEnabled() && !isTwoColumn()) {
            // Remove any existing stale detail fragments (e.g. after a sign-out or when returning
            // to root settings in single-column mode) and clear the back stack so that stale
            // detail fragments are not resurrected when transitioning to two-column mode.
            //
            // If there are entries in the back stack, asynchronously pop them. We must not use
            // popBackStackImmediate() because this method can be invoked while FragmentManager is
            // already executing transactions (e.g. during onStart() lifecycle dispatch when an
            // account was removed in the background). Once the pop transactions complete and the
            // back stack becomes empty, onBackStackEmpty() will remove any remaining detail
            // fragment, close the pane, and update focusability.
            //
            // If the back stack is already empty, directly remove any current detail fragment.
            FragmentManager fragmentManager = getChildFragmentManager();
            if (fragmentManager.getBackStackEntryCount() > 0) {
                fragmentManager.popBackStack(null, FragmentManager.POP_BACK_STACK_INCLUSIVE);
            } else {
                Fragment currentDetail = fragmentManager.findFragmentById(R.id.preferences_detail);
                if (currentDetail != null) {
                    fragmentManager
                            .beginTransaction()
                            .remove(currentDetail)
                            .commitAllowingStateLoss();
                }
            }
            return null;
        }

        return super.onCreateInitialDetailFragment();
    }

    /**
     * Ensures an initial detail fragment is populated when transitioning from one-column mode,
     * which does not have a detail fragment, to two-column mode, which does. See
     * onCreateInitialDetailFragment() above.
     */
    public void ensureInitialDetailFragment() {
        if (!isTwoColumn()) return;

        if (getChildFragmentManager().findFragmentById(R.id.preferences_detail) != null) return;

        Fragment initialDetail = super.onCreateInitialDetailFragment();
        if (initialDetail == null) return;

        showDetailFragment(initialDetail, /* addToBackStack= */ false, /* tag= */ null);
    }

    /**
     * Handles back stack becoming empty after FragmentManager finishes executing transactions. In
     * two-column mode, populates the initial detail fragment so the detail pane does not remain
     * blank. In single-column mode, removes any remaining detail fragment (if SettingsInTab is
     * enabled), closes the sliding pane, and restores header focusability.
     */
    private void onBackStackEmpty() {
        if (getView() == null) return;

        FragmentManager fragmentManager = getChildFragmentManager();
        if (fragmentManager.getBackStackEntryCount() != 0) return;

        if (isTwoColumn()) {
            ensureInitialDetailFragment();
        } else if (SettingsInTab.isEnabled()) {
            // When SettingsInTab is enabled in single-column mode, there should be no detail
            // fragment when at the root settings level. If any detail fragment remains (e.g.
            // an un-backstacked base fragment after popping all back stack entries), remove it
            // and close the sliding pane.
            Fragment currentDetail = fragmentManager.findFragmentById(R.id.preferences_detail);
            if (currentDetail != null) {
                fragmentManager.beginTransaction().remove(currentDetail).commitAllowingStateLoss();
            }
            getSlidingPaneLayout().closePane();
            updateHeaderPaneFocusability();
        } else if (fragmentManager.findFragmentById(R.id.preferences_detail) == null) {
            // When SettingsInTab is disabled, single-column mode (e.g. portrait on a tablet)
            // retains an initial detail fragment. Only close the sliding pane and restore
            // header focusability if no detail fragment remains (e.g. after exiting search).
            getSlidingPaneLayout().closePane();
            updateHeaderPaneFocusability();
        }
    }

    void setPendingFragmentIntent(Intent intent) {
        mPendingFragmentIntent = intent;
    }

    void setOnCreateViewRunnable(@Nullable Runnable runnable) {
        mOnCreateViewRunnable = runnable;
    }

    /**
     * Returns {@link SlidingPaneLayout} if the fragment's view is created, or null.
     *
     * <p>{@link PreferenceHeaderFragmentCompat#getSlidingPaneLayout()} internally calls {@link
     * Fragment#requireView()}, which throws {@link IllegalStateException} if called before {@code
     * onCreateView()} returns or after {@code onDestroyView()} (e.g. during tab closure or view
     * teardown). Callers should use this method when accessing the sliding pane layout
     * asynchronously or during lifecycle transitions to safely handle the null view case.
     */
    public @Nullable SlidingPaneLayout getSlidingPaneLayoutOrNull() {
        return getView() != null ? getSlidingPaneLayout() : null;
    }

    View getDetailView() {
        return mDetailView;
    }

    /**
     * Open the (detail) pane. In single-column mode, this has the detail pane outside the screen
     * slide in and come into view.
     */
    public void slideInDetailPane() {
        getSlidingPaneLayout().openPane();
    }

    /**
     * Whether the detail panel is visible (slid in) or not (slid out). Always returns true when the
     * layout is in two column mode.
     */
    public boolean isLayoutOpen() {
        // getView() may be null in tests before the fragment's view is created.
        if (getView() == null) return false;

        if (isTwoColumn()) {
            return true;
        }
        SlidingPaneLayout slidingPane = getSlidingPaneLayout();
        // In single-column mode, once laid out, SlidingPaneLayout determines whether the detail
        // pane is open (slid in) or closed.
        if (ViewCompat.isLaidOut(slidingPane)) {
            return slidingPane.isOpen();
        }
        // Before the initial layout pass in single-column mode, SlidingPaneLayout.isOpen() defaults
        // to true because isSlideable() is initially false (!mCanSlide). In that pre-layout state,
        // check if a detail fragment is actually present and added in the detail container.
        Fragment detail = getChildFragmentManager().findFragmentById(R.id.preferences_detail);
        return detail != null && detail.isAdded();
    }

    @Override
    public boolean onPreferenceStartFragment(
            PreferenceFragmentCompat caller, Preference preference) {
        // Under SettingsInTab mode, preference selection in the primary
        // navigation header (e.g. clicking "Appearance" or "Privacy" in the
        // left column of MultiColumnSettings) must be routed through
        // Tab.loadUrl() rather than directly triggering showDetailFragment().
        //
        // This ensures the target fragment class and arguments are translated
        // into a canonical chrome://settings/<path> URL string, pushing a new
        // NavigationEntry onto WebContents navigation history, updating the
        // Omnibox URL, and synchronizing browser Back/Forward navigation.
        if (!SettingsInTab.isEnabled() || !ChromeFeatureList.sSettingsInTabUrlNav.isEnabled()) {
            return super.onPreferenceStartFragment(caller, preference);
        }

        String fragmentClassName = preference.getFragment();
        if (fragmentClassName == null) {
            return super.onPreferenceStartFragment(caller, preference);
        }

        Class<? extends Fragment> fragmentClass;
        try {
            fragmentClass = Class.forName(fragmentClassName).asSubclass(Fragment.class);
        } catch (ClassNotFoundException ignored) {
            // Fall back to super method if fragment class cannot be loaded.
            return super.onPreferenceStartFragment(caller, preference);
        }

        SettingsNavigation navigation =
                SettingsNavigationFactory.createSettingsNavigation(requireContext());
        if (navigation == null) {
            return super.onPreferenceStartFragment(caller, preference);
        }

        navigation.startSettings(requireContext(), fragmentClass, preference.getExtras());
        return true;
    }

    /** Shows a fragment inside the detail pane (`preferences_detail`). */
    public void showDetailFragment(
            Fragment fragment, boolean addToBackStack, @Nullable String tag) {
        if (!isAdded()) {
            Intent intent = new Intent();
            intent.putExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT, fragment.getClass().getName());
            if (fragment.getArguments() != null) {
                intent.putExtra(
                        SettingsIntentUtil.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragment.getArguments());
            }
            intent.putExtra(SettingsIntentUtil.EXTRA_ADD_TO_BACK_STACK, addToBackStack);
            if (tag != null) {
                intent.putExtra(SettingsIntentUtil.EXTRA_FRAGMENT_TAG, tag);
            }
            setPendingFragmentIntent(intent);
            return;
        }

        var fragmentManager = getChildFragmentManager();

        // Opening a new page. If we already have back stack entries,
        // and the intent does NOT says the fragment transaction should be added
        // to the back stack (checked by processed.addToBackStack), clean it up for
        // - back button behavior
        // - detailed page title
        if (!addToBackStack && fragmentManager.getBackStackEntryCount() > 0) {
            var entry = fragmentManager.getBackStackEntryAt(0);
            fragmentManager.popBackStack(entry.getId(), FragmentManager.POP_BACK_STACK_INCLUSIVE);
        }

        // Then, open the fragment.
        var transaction = fragmentManager.beginTransaction();
        transaction.setReorderingAllowed(true).replace(R.id.preferences_detail, fragment);
        if (addToBackStack) {
            transaction.addToBackStack(tag);
        }
        transaction.commit();
        // Execute the transaction synchronously so the detail fragment is attached to
        // R.id.preferences_detail before updateHeaderPaneFocusability() evaluates isLayoutOpen().
        // During lifecycle startup (e.g. onStart()), FragmentManager is already executing
        // transactions and will execute the committed transaction automatically; attempting
        // synchronous execution then throws an IllegalStateException.
        if (isResumed()) {
            fragmentManager.executePendingTransactions();
        }
        getSlidingPaneLayout().open();
        updateHeaderPaneFocusability();

        // When navigating in Single Activity mode, the new fragment's view might not be
        // laid out yet when it requests focus. If it requests focus while it has zero
        // size, the keyboard might not show up. Wait for the layout pass and then
        // ensure focus and keyboard are shown.
        final Fragment finalFragment = fragment;
        getSlidingPaneLayout()
                .post(
                        () -> {
                            View detailView = finalFragment.getView();
                            if (detailView == null) return;

                            // Only proceed if the fragment contains an EditText that might
                            // need the keyboard.
                            if (findEditText(detailView) == null) return;

                            // Check if it's already laid out. If so, act immediately.
                            if (detailView.getWidth() > 0 && detailView.getHeight() > 0) {
                                ensureFocusAndKeyboard(detailView);
                                return;
                            }

                            // Otherwise, wait for the first layout pass.
                            detailView.addOnLayoutChangeListener(
                                    new View.OnLayoutChangeListener() {
                                        @Override
                                        public void onLayoutChange(
                                                View v,
                                                int l,
                                                int t,
                                                int r,
                                                int b,
                                                int ol,
                                                int ot,
                                                int or,
                                                int ob) {
                                            int width = r - l;
                                            int height = b - t;
                                            if (width > 0 && height > 0) {
                                                detailView.removeOnLayoutChangeListener(this);
                                                ensureFocusAndKeyboard(detailView);
                                            }
                                        }
                                    });
                        });
    }

    @Override
    public void onResume() {
        // Update the detail pane, if the intent is specified.
        FragmentData processed = processPendingFragmentIntent();
        if (processed != null && processed.fragment != null) {
            showDetailFragment(processed.fragment, processed.addToBackStack, processed.tag);
        }

        super.onResume();

        if (getChildFragmentManager().findFragmentById(R.id.preferences_header)
                instanceof MainSettings mainSettings) {
            mainSettings.addObserver(mOnBackPressedCallback);
        }
    }

    private void ensureFocusAndKeyboard(View detailView) {
        View focusable = detailView.findFocus();
        if (focusable == null) {
            focusable = findEditText(detailView);
        }
        if (focusable != null) {
            focusable.requestFocus();
            if (getActivity() != null && getActivity().getWindow() != null) {
                WindowInsetsControllerCompat controller =
                        new WindowInsetsControllerCompat(getActivity().getWindow(), detailView);
                controller.show(WindowInsetsCompat.Type.ime());
            } else {
                KeyboardVisibilityDelegate.getInstance().showKeyboard(focusable);
            }
        }
    }

    private @Nullable View findEditText(View view) {
        if (view instanceof android.widget.EditText) {
            return view;
        }
        if (view instanceof ViewGroup group) {
            for (int i = 0; i < group.getChildCount(); i++) {
                View child = group.getChildAt(i);
                View result = findEditText(child);
                if (result != null) return result;
            }
        }
        return null;
    }

    @Override
    public void onPause() {
        if (getChildFragmentManager().findFragmentById(R.id.preferences_header)
                instanceof MainSettings mainSettings) {
            mainSettings.removeObserver(mOnBackPressedCallback);
        }
        super.onPause();
    }

    /**
     * Processes the pending Intent if there is, and returns the Fragment to be used in the detailed
     * pane.
     *
     * @return a pair of processed fragment and whether or not to add the transaction to the back
     *     stack on success. Otherwise, null.
     */
    private @Nullable FragmentData processPendingFragmentIntent() {
        if (mPendingFragmentIntent == null) {
            return null;
        }
        Intent intent = mPendingFragmentIntent;
        mPendingFragmentIntent = null;

        // The logic here should be conceptually consistent with
        // SettingsActivity.instantiateMainFragment.
        String fragmentName = intent.getStringExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT);
        Bundle arguments = intent.getBundleExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT_ARGUMENTS);
        boolean addToBackStack =
                intent.getBooleanExtra(SettingsIntentUtil.EXTRA_ADD_TO_BACK_STACK, false);
        String tag = intent.getStringExtra(SettingsIntentUtil.EXTRA_FRAGMENT_TAG);

        // Consume the "show fragment" extras so future launches of settings go to the main pane.
        // This is simpler than trying to keep track of whether an intent was processed.
        intent.removeExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT);
        intent.removeExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT_ARGUMENTS);
        intent.removeExtra(SettingsIntentUtil.EXTRA_ADD_TO_BACK_STACK);
        intent.removeExtra(SettingsIntentUtil.EXTRA_FRAGMENT_TAG);

        // If there's no fragment to show, bail out.
        if (fragmentName == null) {
            return null;
        }
        // Use a null fragment to indicate MainSettings.
        if (SettingsInTab.isEnabled() && MainSettings.class.getName().equals(fragmentName)) {
            return new FragmentData(null, addToBackStack, tag);
        }
        // Use requireContext() instead of requireActivity() to include themed contexts used by
        // SettingsInTab.
        return new FragmentData(
                Fragment.instantiate(requireContext(), fragmentName, arguments),
                addToBackStack,
                tag);
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View view = super.onCreateView(inflater, container, savedInstanceState);
        addTitleContainer(inflater, (SlidingPaneLayout) view);

        var resources = view.getResources();
        View headerView = view.findViewById(R.id.preferences_header);
        LayoutParams headerParams = headerView.getLayoutParams();
        headerParams.width = resources.getDimensionPixelSize(R.dimen.settings_narrow_header_width);
        headerView.setLayoutParams(headerParams);

        View detailView = view.findViewById(R.id.preferences_detail_pane);
        LayoutParams params = detailView.getLayoutParams();
        // Set the minimum required width of detailed view here, so that the SlidingPaneLayout
        // handles single/multi column switch.
        params.width =
                resources.getDimensionPixelSize(R.dimen.settings_min_multi_column_screen_width)
                        - resources.getDimensionPixelSize(R.dimen.settings_narrow_header_width);
        detailView.setLayoutParams(params);
        detailView.addOnLayoutChangeListener(
                (v, l, t, r, b, ol, ot, or, ob) -> {
                    if (r - l != or - ol) {
                        for (Observer o : mObservers) o.onDetailLayoutUpdated();
                    }
                });
        view.post(
                () -> {
                    updateHeaderPaneFocusability();
                    for (Observer o : mObservers) o.onHeaderLayoutUpdated();
                    if (mOnCreateViewRunnable != null) {
                        mOnCreateViewRunnable.run();
                        mOnCreateViewRunnable = null;
                    }
                });
        mDetailView = detailView;
        return view;
    }

    /**
     * Updates descendant focusability and accessibility for the header pane. When the detail pane
     * is opened in single-column mode, the header pane is hidden behind the detail pane, so its
     * descendants must be blocked from keyboard focus and hidden from accessibility to prevent
     * keyboard tab navigation and screen readers from traversing obscured header items.
     */
    void updateHeaderPaneFocusability() {
        // May be called before the view is inflated.
        if (getView() == null) return;

        View headerView = getView().findViewById(R.id.preferences_header);
        // View may not exist in tests.
        if (headerView == null) return;

        ViewGroup headerGroup = (ViewGroup) headerView;
        boolean isHeaderObscured = !isTwoColumn() && isLayoutOpen();
        if (isHeaderObscured) {
            headerGroup.setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);
            headerGroup.setImportantForAccessibility(
                    View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        } else {
            headerGroup.setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);
            headerGroup.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_AUTO);
        }
    }

    /** Sets the Profile required for generating the search index. Called by the host Activity. */
    @EnsuresNonNull("mProfile")
    @Override
    public void setProfile(Profile profile) {
        mProfile = profile;
    }

    /**
     * Returns the breadcrumb path for the currently displayed detail fragment. This uses the
     * Settings search index to find the shortest path from the root. If the index hasn't been built
     * yet (e.g. user just opened the app via deep link), it will force-build the index
     * synchronously.
     */
    public @Nullable List<SettingsIndexData.Entry> getBreadcrumbEntriesForCurrentFragment() {
        assert mProfile != null;

        assertNonNull(mProfile);

        Fragment fragment = getChildFragmentManager().findFragmentById(R.id.preferences_detail);

        assertNonNull(fragment);

        SettingsIndexData indexData =
                SettingsSearchCoordinator.ensureIndexBuilt(getActivity(), mProfile);

        return indexData.getBreadcrumbEntries(
                fragment.getClass().getName(), fragment.getArguments());
    }

    // Replaces the detailed pane added in super.onCreateView with a new one that displays
    // the title at the top of the pane.
    private void addTitleContainer(LayoutInflater inflater, SlidingPaneLayout slidingPaneLayout) {
        View oldDetailedView = slidingPaneLayout.findViewById(R.id.preferences_detail);
        slidingPaneLayout.removeView(oldDetailedView);
        var newDetailedView = inflater.inflate(R.layout.settings_preference_detail_pane, null);
        var detailLayoutParams =
                new SlidingPaneLayout.LayoutParams(
                        getResources().getDimensionPixelSize(R.dimen.preferences_detail_width),
                        SlidingPaneLayout.LayoutParams.MATCH_PARENT);
        detailLayoutParams.weight =
                getResources().getInteger(R.integer.preferences_detail_pane_weight);
        slidingPaneLayout.addView(newDetailedView, detailLayoutParams);
    }

    /** Returns whether the current layout is in two-column mode. */
    boolean isTwoColumn() {
        SlidingPaneLayout slidingPane = getSlidingPaneLayoutOrNull();
        // If SlidingPaneLayout has already completed layout, use its computed slideable state.
        if (slidingPane != null
                && ViewCompat.isLaidOut(slidingPane)
                && slidingPane.getWidth() > 0) {
            return !slidingPane.isSlideable();
        }
        // Before the initial layout pass, isSlideable() defaults to false. Fall back to comparing
        // available container/window width to prevent incorrectly assuming two-column mode before
        // measurement.
        int minMultiColumnWidth =
                getResources()
                        .getDimensionPixelSize(R.dimen.settings_min_multi_column_screen_width);
        int availableWidth = getAvailableWidth(slidingPane);
        return availableWidth >= minMultiColumnWidth;
    }

    /** Returns the available width for the sliding pane. */
    private int getAvailableWidth(@Nullable SlidingPaneLayout slidingPane) {
        // If the sliding pane is laid out, use its width
        if (slidingPane != null && slidingPane.getWidth() > 0) {
            return slidingPane.getWidth();
        }
        // Otherwise, use the activity's window width.
        if (getActivity() != null && getActivity().getWindow() != null) {
            View decorView = getActivity().getWindow().getDecorView();
            if (decorView.getWidth() > 0) {
                return decorView.getWidth();
            }
        }
        // Fall back to display width.
        return getResources().getDisplayMetrics().widthPixels;
    }

    private class SlideStateTracker
            implements SlidingPaneLayout.PanelSlideListener, View.OnLayoutChangeListener {
        @SlideState int mState;
        private float mOffset;
        private boolean mSlideable;

        SlideStateTracker() {
            mState = getSlidingPaneLayout().isOpen() ? SlideState.OPENED : SlideState.CLOSED;
            mOffset = mState == SlideState.OPENED ? 0f : 1f;
            mSlideable = getSlidingPaneLayout().isSlideable();
        }

        @Override
        public void onLayoutChange(
                View v,
                int left,
                int top,
                int right,
                int bottom,
                int oldLeft,
                int oldTop,
                int oldRight,
                int oldBottom) {
            updateHeaderPaneFocusability();
            boolean prevSlideable = mSlideable;
            mSlideable = getSlidingPaneLayout().isSlideable();
            if (prevSlideable != mSlideable) {
                // Transitioning from one-column to two-column mode, or vice versa.
                ensureInitialDetailFragment();
                for (Observer o : mObservers) {
                    o.onHeaderLayoutUpdated();
                }
            }
            if (prevSlideable == mSlideable) {
                return;
            }

            if (getSlidingPaneLayout().isOpen()) {
                if (mState != SlideState.OPENED) {
                    onPanelOpened(v);
                }
            } else {
                if (mState != SlideState.CLOSED) {
                    onPanelClosed(v);
                }
            }
        }

        @Override
        public void onPanelSlide(View panel, float slideOffset) {
            @SlideState int prevState = mState;
            mState = mOffset > slideOffset ? SlideState.OPENING : SlideState.CLOSING;
            mOffset = slideOffset;
            maybeNotifyObserver(prevState, mState);
        }

        @Override
        public void onPanelOpened(View panel) {
            @SlideState int prevState = mState;
            mState = SlideState.OPENED;
            mOffset = 0f;
            updateHeaderPaneFocusability();
            maybeNotifyObserver(prevState, mState);
        }

        @Override
        public void onPanelClosed(View panel) {
            @SlideState int prevState = mState;
            mState = SlideState.CLOSED;
            mOffset = 1f;
            updateHeaderPaneFocusability();
            maybeNotifyObserver(prevState, mState);
        }

        private void maybeNotifyObserver(@SlideState int prevState, @SlideState int newState) {
            if (prevState == newState) {
                return;
            }

            for (Observer o : mObservers) {
                o.onSlideStateUpdated(newState);
            }
        }
    }

    private class InnerOnBackPressedCallback extends OnBackPressedCallback
            implements SlidingPaneLayout.PanelSlideListener, MainSettings.Observer {
        InnerOnBackPressedCallback() {
            super(true);
        }

        @Override
        public void handleOnBackPressed() {
            getSlidingPaneLayout().closePane();
        }

        @Override
        public void onPanelSlide(View panel, float slideOffset) {}

        @Override
        public void onPanelOpened(View panel) {
            updateEnabledState();
        }

        @Override
        public void onPanelClosed(View panel) {
            updateEnabledState();
        }

        @Override
        public void onPreferenceSelected(Preference preference) {
            // If a preferene of the main menu is selected, navigate user back to the main
            // menu, even if in single column mode.
            mCanBeBackToMain = true;
        }

        void updateEnabledState() {
            // This method may be called from delayed tasks that outlive the view, for example the
            // postDelayed() call in onViewCreated().
            if (getView() == null) return;

            // Trigger closePane() when
            // - the first page was the main menu, or main menu is not yet created
            //   after activity restart.
            // - in one-column mode
            // - the detailed pane is open (i.e., not on the main menu)
            // - the fragment back stack is empty (i.e., with the above condition
            //   this means the subpage directly under the main menu).
            boolean enabled =
                    (mCanBeBackToMain || mMainSettings == null)
                            && getSlidingPaneLayout().isSlideable()
                            && getSlidingPaneLayout().isOpen()
                            && (getChildFragmentManager().getBackStackEntryCount() == 0);
            setEnabled(enabled);
        }
    }

    // Workaround for fragment identifying issue.
    static @Nullable String getUUID(Fragment fragment) {
        // This function depends on internal structure of Fragment.toString().
        // In fragment, an UUID is assigned, which survives at activity recreation.
        // The expected format begins with "<classname>{<hash>} (<UUID>...".
        // Also, the UUID format is [0-9a-f]+(-[0-9a-f])*.

        // Find the open paren.
        String s = fragment.toString();
        int begin = s.indexOf("(");
        if (begin < 0) {
            return null;
        }
        ++begin; // Exclude the beginning '('.

        // Find first character not in '0-9a-f' nor '-'.
        int end = begin;
        for (; end < s.length(); ++end) {
            char c = s.charAt(end);
            if ("0123456789abcdef-".indexOf(c) < 0) {
                break;
            }
        }
        return s.substring(begin, end);
    }

    static class Title {
        Title(
                String uuid,
                MonotonicObservableSupplier<String> titleSupplier,
                int backStackCount,
                @Nullable String mainMenuKey) {
            this.uuid = uuid;
            this.titleSupplier = titleSupplier;
            this.backStackCount = backStackCount;
            this.mainMenuKey = mainMenuKey;
        }

        public final String uuid;

        public final MonotonicObservableSupplier<String> titleSupplier;

        /** the number of back stack entries when the fragment started */
        public final int backStackCount;

        /**
         * the "key" tag specified in main_preference, if it should be highlighted when this item is
         * at the bottom of the back stack.
         */
        public final @Nullable String mainMenuKey;
    }

    static class FragmentUuidMapCreator extends FragmentManager.FragmentLifecycleCallbacks {
        final Map<String, EmbeddableSettingsPage> mMap = new HashMap<>();

        @Override
        public void onFragmentCreated(
                FragmentManager fm, Fragment f, @Nullable Bundle savedInstanceState) {
            if (f instanceof EmbeddableSettingsPage page) {
                String uuid = getUUID(f);
                if (uuid != null) {
                    mMap.put(uuid, page);
                }
            }
        }
    }

    static class FragmentTracker extends FragmentManager.FragmentLifecycleCallbacks {
        final List<Title> mTitles = new ArrayList<>();

        // Used to force-trigger the observers after activity re-creation, when the title updater
        // need to display the breadcrumb from the restored titles.
        private boolean mTitleInitialized;

        private final ObserverList<Observer> mObservers;

        FragmentTracker(ObserverList<Observer> observers) {
            mObservers = observers;
        }

        private static final String TAG = "FragmentTracker";

        // Note: in order to support recreation of the activity, this fragment stores the "titles"
        // as the state to be restored.
        // This is because, unfortunately, there's no way to identify the fragment from the
        // FragmentManager.BackStackEntry information.
        // So, instead we track the fragments in FragmentTracker and record the order in the Bundle
        // then restore it on activity recreation.
        // We couldn't record the position information in each fragment's Bundle state in
        // FragmentTracer, because, in some edge cases, the saved value is not sent back on
        // restoring the fragment. (it looks framework/library issue, but anyways we have to deal
        // with the situation).
        // Thus, we store UUID of the fragment, used in androidx.fragment.app.Fragment, because
        // there's no other reliable identifiers we can use. See getUUID method for implementation
        // details.

        // Key used for saving title fragment UUIDs.
        private static final String KEY_TITLE_UUIDS = "TitleUUIDs";

        // Key used for saving back stack positions.
        private static final String KEY_BACK_STACK_COUNTS = "BackStackCounts";

        @SuppressWarnings("ReferenceEquality")
        private boolean isTopFragment(FragmentManager fm, Fragment f) {
            List<Fragment> fragments = fm.getFragments();
            return f == fragments.get(fragments.size() - 1);
        }

        @Override
        public void onFragmentResumed(FragmentManager fm, Fragment f) {
            if (f instanceof MainSettings) {
                // Skip main settings which is visible in the header pane.
                return;
            }

            if (f instanceof DialogFragment dialogFragment && dialogFragment.getShowsDialog()) {
                // Skip on showing a dialog UI.
                return;
            }
            // onFragmentResumed signifies that the Fragment is in the RESUMED state of its
            // lifecycle, not necessarily that it is the "top-most" or "currently focused"
            // fragment in a specific container. If the detail pane has a back stack, the
            // fragment being popped and the fragment being revealed can occasionally overlap
            // in their lifecycle states during the transition. Android system may briefly
            // initialize or resume the underlying fragment before the top-most one fully
            // takes over. EmptyFragment is often immediately followed by real the top-most
            // ragment. This causes an issue that inadvertently mangles the breadcrumb.
            // It should be filtered to prevent it.
            if (f.getClass() == EmptyFragment.class && !isTopFragment(fm, f)) {
                return;
            }

            boolean updated = false;

            // This is coming from the click on header pane pref.
            int backStackCount = fm.getBackStackEntryCount();
            if (backStackCount == 0) {
                if (!(f instanceof EmbeddableSettingsPage page)
                        || mTitles.size() != 1
                        || mTitles.get(0).titleSupplier != page.getPageTitle()) {
                    mTitles.clear();
                    updated = true;
                }
            }

            if (f instanceof EmbeddableSettingsPage page) {
                MonotonicObservableSupplier<String> titleSupplier = page.getPageTitle();
                String uuid = getUUID(f);
                assert uuid != null;
                int index = -1;
                for (int i = 0; i < mTitles.size(); ++i) {
                    Title candidate = mTitles.get(i);
                    if (candidate.titleSupplier == titleSupplier) {
                        index = i;
                        break;
                    }
                }

                if (index < 0) {
                    // Enter into more detailed page.
                    mTitles.add(
                            new Title(uuid, titleSupplier, backStackCount, page.getMainMenuKey()));
                    updated = true;
                } else {
                    // Move back from the detailed page.
                    for (int i = mTitles.size() - 1; i > index; --i) {
                        mTitles.remove(i);
                        updated = true;
                    }
                }
                if (!updated) {
                    // All the search results fragments share their |titleSupplier|. Replaces its
                    // uuid to the latest one if the fragment is present at the end of the list.
                    int pos = mTitles.size() - 1;
                    Title result = mTitles.get(pos);
                    if (titleSupplier == result.titleSupplier
                            && !TextUtils.equals(uuid, result.uuid)) {
                        mTitles.set(
                                pos, new Title(uuid, titleSupplier, result.backStackCount, null));
                    }
                }
            }

            if (updated || !mTitleInitialized) {
                for (Observer o : mObservers) o.onTitleUpdated();
                mTitleInitialized = true;
            }
        }

        void saveTitles(Bundle outState) {
            String[] uuids = new String[mTitles.size()];
            int[] backStackCounts = new int[mTitles.size()];
            for (int i = 0; i < mTitles.size(); ++i) {
                uuids[i] = mTitles.get(i).uuid;
                backStackCounts[i] = mTitles.get(i).backStackCount;
            }
            outState.putStringArray(KEY_TITLE_UUIDS, uuids);
            outState.putIntArray(KEY_BACK_STACK_COUNTS, backStackCounts);
        }

        void restoreTitles(
                @Nullable Bundle savedInstanceState, Map<String, EmbeddableSettingsPage> uuidMap) {
            if (savedInstanceState == null) {
                return;
            }

            assert mTitles.isEmpty();
            String[] uuids = savedInstanceState.getStringArray(KEY_TITLE_UUIDS);
            int[] backStackCounts = savedInstanceState.getIntArray(KEY_BACK_STACK_COUNTS);
            if (uuids == null || backStackCounts == null) {
                return;
            }
            assert uuids.length == backStackCounts.length;

            // Tracks created fragments that haven't been matched to a saved title yet. Under normal
            // navigation, fragments match their saved UUIDs directly. However, if an intermediate
            // fragment was replaced in-place without adding to the back stack (e.g.
            // SearchResultsPreferenceFragment replacing EmptyFragment), its UUID will not be in
            // uuidMap upon activity recreation, and the original back stack fragment
            // (EmptyFragment) will be left in remainingMap to be matched as a fallback. See
            // https://crbug.com/542323396
            Map<String, EmbeddableSettingsPage> remainingMap = new HashMap<>(uuidMap);
            Title[] restoredTitles = new Title[uuids.length];
            List<Integer> unmatchedIndices = new ArrayList<>();

            for (int i = 0; i < uuids.length; ++i) {
                String uuid = uuids[i];
                int backStackCount = backStackCounts[i];
                EmbeddableSettingsPage page = remainingMap.remove(uuid);
                if (page != null) {
                    restoredTitles[i] =
                            new Title(
                                    uuid,
                                    page.getPageTitle(),
                                    backStackCount,
                                    page.getMainMenuKey());
                } else {
                    unmatchedIndices.add(i);
                }
            }

            // Match any remaining unmatched titles with remaining recreated fragments.
            var remainingEntries = new ArrayList<>(remainingMap.entrySet());
            remainingEntries.sort(Map.Entry.comparingByKey());
            int pageIndex = 0;
            for (int index : unmatchedIndices) {
                if (pageIndex < remainingEntries.size()) {
                    var entry = remainingEntries.get(pageIndex++);
                    String uuid = entry.getKey();
                    EmbeddableSettingsPage page = entry.getValue();
                    int backStackCount = backStackCounts[index];
                    restoredTitles[index] =
                            new Title(
                                    uuid,
                                    page.getPageTitle(),
                                    backStackCount,
                                    page.getMainMenuKey());
                }
            }

            for (Title title : restoredTitles) {
                if (title != null) {
                    mTitles.add(title);
                }
            }
        }
    }

    List<Title> getTitles() {
        return mFragmentTracker.mTitles;
    }

    public void addObserver(Observer o) {
        mObservers.addObserver(o);
    }

    public void removeObserver(Observer o) {
        mObservers.removeObserver(o);
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        var fragmentManager = getChildFragmentManager();

        // Capture created fragments in super.onCreate() specifically for activity recreating cases.
        // The fragments are used in order to restore titles.
        if (savedInstanceState != null) {
            var uuidMapCreator = new FragmentUuidMapCreator();
            fragmentManager.registerFragmentLifecycleCallbacks(
                    uuidMapCreator, /* recursive= */ false);
            try {
                super.onCreate(savedInstanceState);
            } finally {
                fragmentManager.unregisterFragmentLifecycleCallbacks(uuidMapCreator);
            }
            // Also collect any fragments already present in FragmentManager that may not have
            // been captured by the lifecycle callbacks.
            for (Fragment f : fragmentManager.getFragments()) {
                if (f instanceof EmbeddableSettingsPage page) {
                    String uuid = getUUID(f);
                    if (uuid != null) {
                        uuidMapCreator.mMap.putIfAbsent(uuid, page);
                    }
                }
            }
            mFragmentTracker.restoreTitles(savedInstanceState, uuidMapCreator.mMap);
        } else {
            super.onCreate(savedInstanceState);
        }

        fragmentManager.registerFragmentLifecycleCallbacks(
                mFragmentTracker, /* recursive= */ false);
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        mFragmentTracker.saveTitles(outState);
    }

    @Override
    @SuppressWarnings("MissingSuperCall")
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        // Overrides the back press button behavior provided by the library as workaround.
        // The provided behavior does not close SettingsActivity even if it shows
        // main menu in two-pane mode. Revisit later if back button behavior in the library is
        // updated.
        mOnBackPressedCallback = new InnerOnBackPressedCallback();
        getSlidingPaneLayout().addPanelSlideListener(mOnBackPressedCallback);
        getSlidingPaneLayout()
                .addOnLayoutChangeListener(
                        (View v,
                                int left,
                                int top,
                                int right,
                                int bottom,
                                int oldLeft,
                                int oldTop,
                                int oldRight,
                                int oldBottom) -> {
                            mOnBackPressedCallback.updateEnabledState();
                        });
        getChildFragmentManager()
                .addOnBackStackChangedListener(
                        () -> {
                            // This listener can outlive the View lifecycle.
                            if (getView() == null) return;
                            // On some specific devices, FragmentManager's BackStackChangedListener
                            // seems to be called *before* the back stack is updated, specifically
                            // if this is triggered from the system back button and the fragment
                            // manager's back stack will become empty by the event.
                            // Thus, updateEnabledState() above may NOT update the state to the
                            // expected one.
                            // As a workaround, post updateEnabledState with some delay, which
                            // should invoke the method *after* the back stack is updated so the
                            // "back button" in the following pages can work as expected.
                            // Unfortunately, this is not perfect solution, as there still is some
                            // short timing that enabled is not properly set, but still provides
                            // better UX. See crbug.com/465040723 for more context.
                            if (getChildFragmentManager().getBackStackEntryCount() == 1) {
                                getSlidingPaneLayout()
                                        .postDelayed(
                                                mOnBackPressedCallback::updateEnabledState, 100);
                            }
                            // Clean up after the back stack empties. Post the task so
                            // FragmentManager finishes executing the pop transaction before we
                            // run any new fragment transactions or update the UI.
                            if (getChildFragmentManager().getBackStackEntryCount() == 0) {
                                getSlidingPaneLayout().post(this::onBackStackEmpty);
                            }
                        });

        // For SettingsInTabUrlNav, rely on the Chrome Navigation Stack to handle detailFragment
        // loading.
        if (!ChromeFeatureList.sSettingsInTabUrlNav.isEnabled()) {
            requireActivity()
                    .getOnBackPressedDispatcher()
                    .addCallback(this, mOnBackPressedCallback);
        }

        mCanBeBackToMain = getSlidingPaneLayout().isSlideable() && !getSlidingPaneLayout().isOpen();
        mSlideStateTracker = new SlideStateTracker();
        getSlidingPaneLayout().addPanelSlideListener(mSlideStateTracker);
        getSlidingPaneLayout().addOnLayoutChangeListener(mSlideStateTracker);

        @SlideState
        int initState = getSlidingPaneLayout().isOpen() ? SlideState.OPENED : SlideState.CLOSED;
        updateHeaderPaneFocusability();
        for (Observer o : mObservers) {
            o.onSlideStateUpdated(initState);
        }
    }

    @Override
    public void onDestroyView() {
        SlidingPaneLayout slidingPane = getSlidingPaneLayoutOrNull();
        if (slidingPane != null) {
            if (mSlideStateTracker != null) {
                slidingPane.removeOnLayoutChangeListener(mSlideStateTracker);
                slidingPane.removePanelSlideListener(mSlideStateTracker);
            }
            if (mOnBackPressedCallback != null) {
                slidingPane.removePanelSlideListener(mOnBackPressedCallback);
            }
        }
        if (mOnBackPressedCallback != null) {
            mOnBackPressedCallback.remove();
        }
        mOnCreateViewRunnable = null;
        super.onDestroyView();
    }

    @Override
    public void onDestroy() {
        getChildFragmentManager().unregisterFragmentLifecycleCallbacks(mFragmentTracker);
        super.onDestroy();
    }

    /** Returns the number of entries in the child fragment manager back stack. */
    public int getBackStackEntryCount() {
        return isAdded() ? getChildFragmentManager().getBackStackEntryCount() : 0;
    }

    /** Pops the top entry from the child fragment manager back stack. */
    public void popBackStack() {
        assert isAdded();
        getChildFragmentManager().popBackStack();
    }

    /** Returns the child fragment manager if attached, or null. */
    public @Nullable FragmentManager getChildFragmentManagerOrNull() {
        return isAdded() ? getChildFragmentManager() : null;
    }
}
