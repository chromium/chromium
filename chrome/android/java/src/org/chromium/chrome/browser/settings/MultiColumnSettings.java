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
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceHeaderFragmentCompat;
import androidx.slidingpanelayout.widget.SlidingPaneLayout;

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
    @interface SlideState {
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

    private final List<Observer> mObservers = new ArrayList<>();

    private final FragmentTracker mFragmentTracker = new FragmentTracker(mObservers);

    private @Nullable Profile mProfile;

    private @Nullable Context mThemedContext;

    @Override
    public void onAttach(Context context) {
        // Traditional settings has the theme applied at the activity level.
        if (!ChromeFeatureList.sSettingsInTab.isEnabled()) {
            super.onAttach(context);
            return;
        }
        // Settings in a tab must apply the theme at a fragment level.
        mThemedContext = new ContextThemeWrapper(context, R.style.Theme_Chromium_Settings);
        super.onAttach(mThemedContext);
    }

    @Override
    public Context getContext() {
        return mThemedContext != null ? mThemedContext : assumeNonNull(super.getContext());
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

    // Fragment data passed as extras of Intent via SettingsNavigation.
    private static class FragmentData {
        public final Fragment fragment;
        public final boolean addToBackStack;
        public final @Nullable String tag;

        FragmentData(Fragment fragment, boolean addToBackStack, @Nullable String tag) {
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
            if (!(processed.fragment instanceof MainSettings)) {
                getSlidingPaneLayout().openPane();
            }
            return processed.fragment;
        }
        return super.onCreateInitialDetailFragment();
    }

    void setPendingFragmentIntent(Intent intent) {
        mPendingFragmentIntent = intent;
    }

    void setOnCreateViewRunnable(Runnable runnable) {
        mOnCreateViewRunnable = runnable;
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

    /** Whether the detail panel is open. */
    public boolean isLayoutOpen() {
        return getSlidingPaneLayout().isOpen();
    }

    /** Shows a fragment inside the detail pane (`preferences_detail`). */
    public void showDetailFragment(
            Fragment fragment, boolean addToBackStack, @Nullable String tag) {
        if (!isAdded()) {
            Intent intent = new Intent();
            intent.putExtra(SettingsActivity.EXTRA_SHOW_FRAGMENT, fragment.getClass().getName());
            if (fragment.getArguments() != null) {
                intent.putExtra(
                        SettingsActivity.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragment.getArguments());
            }
            intent.putExtra(SettingsActivity.EXTRA_ADD_TO_BACK_STACK, addToBackStack);
            if (tag != null) {
                intent.putExtra(SettingsActivity.EXTRA_FRAGMENT_TAG, tag);
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
        getSlidingPaneLayout().open();

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
        if (processed != null) {
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
        String fragmentName = intent.getStringExtra(SettingsActivity.EXTRA_SHOW_FRAGMENT);
        if (fragmentName == null) {
            return null;
        }
        Bundle arguments = intent.getBundleExtra(SettingsActivity.EXTRA_SHOW_FRAGMENT_ARGUMENTS);
        boolean addToBackStack =
                intent.getBooleanExtra(SettingsActivity.EXTRA_ADD_TO_BACK_STACK, false);
        String tag = intent.getStringExtra(SettingsActivity.EXTRA_FRAGMENT_TAG);
        return new FragmentData(
                Fragment.instantiate(requireActivity(), fragmentName, arguments),
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
                    for (Observer o : mObservers) o.onHeaderLayoutUpdated();
                    if (mOnCreateViewRunnable != null) mOnCreateViewRunnable.run();
                });
        mDetailView = detailView;
        return view;
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
        return !getSlidingPaneLayout().isSlideable();
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
            boolean prevSlideable = mSlideable;
            mSlideable = getSlidingPaneLayout().isSlideable();
            if (prevSlideable != mSlideable) {
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
            maybeNotifyObserver(prevState, mState);
        }

        @Override
        public void onPanelClosed(View panel) {
            @SlideState int prevState = mState;
            mState = SlideState.CLOSED;
            mOffset = 1f;
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
    private static @Nullable String getUUID(Fragment fragment) {
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

        private final List<Observer> mObservers;

        FragmentTracker(List<Observer> observers) {
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

            for (int i = 0; i < uuids.length; ++i) {
                String uuid = uuids[i];
                int backStackCount = backStackCounts[i];
                var page = uuidMap.get(uuid);
                assert page != null;
                mTitles.add(
                        new Title(
                                uuid, page.getPageTitle(), backStackCount, page.getMainMenuKey()));
            }
        }
    }

    List<Title> getTitles() {
        return mFragmentTracker.mTitles;
    }

    public void addObserver(Observer o) {
        mObservers.add(o);
    }

    public void removeObserver(Observer o) {
        mObservers.remove(o);
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
                        });

        requireActivity().getOnBackPressedDispatcher().addCallback(this, mOnBackPressedCallback);

        mCanBeBackToMain = getSlidingPaneLayout().isSlideable() && !getSlidingPaneLayout().isOpen();
        mSlideStateTracker = new SlideStateTracker();
        getSlidingPaneLayout().addPanelSlideListener(mSlideStateTracker);
        getSlidingPaneLayout().addOnLayoutChangeListener(mSlideStateTracker);

        @SlideState
        int initState = getSlidingPaneLayout().isOpen() ? SlideState.OPENED : SlideState.CLOSED;
        for (Observer o : mObservers) {
            o.onSlideStateUpdated(initState);
        }
    }

    @Override
    public void onDestroyView() {
        if (mSlideStateTracker != null) {
            getSlidingPaneLayout().removeOnLayoutChangeListener(mSlideStateTracker);
            getSlidingPaneLayout().removePanelSlideListener(mSlideStateTracker);
        }
        if (mOnBackPressedCallback != null) {
            getSlidingPaneLayout().removePanelSlideListener(mOnBackPressedCallback);
            mOnBackPressedCallback.remove();
        }
        super.onDestroyView();
    }

    @Override
    public void onDestroy() {
        getChildFragmentManager().unregisterFragmentLifecycleCallbacks(mFragmentTracker);
        super.onDestroy();
    }
}
