// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Intent;
import android.os.Bundle;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceHeaderFragmentCompat;
import androidx.slidingpanelayout.widget.SlidingPaneLayout;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Preference container implementation for SettingsActivity in multi-column mode. */
@NullMarked
public class MultiColumnSettings extends PreferenceHeaderFragmentCompat {

    interface Observer {
        /** Called when detailed pane title is updated. */
        default void onTitleUpdated() {}

        /**
         * Called when the menu layout is updated. I.e. - Switching between the single pane mode and
         * the two pane mode. - In the two pane mode, size change of the header layout.
         */
        default void onHeaderLayoutUpdated() {}

        /** Called when the sliding state is updated. */
        default void onSlideStateUpdated(@SlideState int newState) {}
    }

    /**
     * Thresdhold window DP between narrow header and wide header. If the window width is as same or
     * wider than this, the wider header should be used.
     */
    private static final int WIDE_HEADER_SCREEN_WIDTH_DP = 1200;

    /** Represents the current state of sliding pane. */
    @IntDef({SlideState.CLOSING, SlideState.CLOSED, SlideState.OPENING, SlideState.OPENED})
    @Retention(RetentionPolicy.SOURCE)
    @interface SlideState {
        int CLOSING = 0;
        int CLOSED = 1;
        int OPENING = 2;
        int OPENED = 3;
    }

    /** Caches the view of the header panel. */
    private View mHeaderView;

    /**
     * Caches whether currently it is running in single pane mode or two pane mode to detect the
     * mode changes
     */
    private boolean mSlideable;

    private SlideStateTracker mSlideStateTracker;

    private InnerOnBackPressedCallback mOnBackPressedCallback;

    private @Nullable Intent mPendingFragmentIntent;

    private final List<Observer> mObservers = new ArrayList<>();

    private final FragmentTracker mFragmentTracker = new FragmentTracker(mObservers);

    @Override
    public PreferenceFragmentCompat onCreatePreferenceHeader() {
        // Main menu, which is the first page in one column mode (i.e. window is
        // small enough), or shown at left side pane in two column mode.
        return new MainSettings();
    }

    @Override
    public @Nullable Fragment onCreateInitialDetailFragment() {
        // Look at if there is a pending Intent and use it if it is.
        // Otherwise fallback to the original logic, i.e. use the first item in the main menu.
        Pair<Fragment, Boolean> processed = processPendingFragmentIntent();
        if (processed != null) {
            if (!(processed.first instanceof MainSettings)) {
                getSlidingPaneLayout().openPane();
            }
            return processed.first;
        }
        return super.onCreateInitialDetailFragment();
    }

    void setPendingFragmentIntent(Intent intent) {
        mPendingFragmentIntent = intent;
    }

    View getHeaderView() {
        return mHeaderView;
    }

    /** Whether the detail panel is open. */
    public boolean isLayoutOpen() {
        return getSlidingPaneLayout().isOpen();
    }

    @Override
    public void onResume() {
        // Update the detail pane, if the intent is specified.
        Pair<Fragment, Boolean> processed = processPendingFragmentIntent();
        if (processed != null) {
            var fragmentManager = getChildFragmentManager();

            // Opening a new page. If we already have back stack entries,
            // and the intent does NOT says the fragment transaction should be added
            // to the back stack (checked by processed.second), clean it up for
            // - back button behavior
            // - detailed page title
            if (!processed.second) {
                if (fragmentManager.getBackStackEntryCount() > 0) {
                    var entry = fragmentManager.getBackStackEntryAt(0);
                    fragmentManager.popBackStack(
                            entry.getId(), FragmentManager.POP_BACK_STACK_INCLUSIVE);
                }
            }

            // Then, open the fragment.
            var transaction = fragmentManager.beginTransaction();
            transaction
                    .setReorderingAllowed(true)
                    .replace(R.id.preferences_detail, processed.first);
            if (processed.second) {
                transaction.addToBackStack(null);
            }
            transaction.commit();
            getSlidingPaneLayout().open();
        }

        super.onResume();
    }

    /**
     * Processes the pending Intent if there is, and returns the Fragment to be used in the detailed
     * pane.
     *
     * @return a pair of processed fragment and whether or not to add the transaction to the back
     *     stack on success. Otherwise, null.
     */
    private @Nullable Pair<Fragment, Boolean> processPendingFragmentIntent() {
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
        return new Pair<>(
                Fragment.instantiate(requireActivity(), fragmentName, arguments), addToBackStack);
    }

    @Override
    public @NonNull View onCreateView(
            @NonNull LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View view = super.onCreateView(inflater, container, savedInstanceState);
        boolean searchEnabled = ChromeFeatureList.sSearchInSettings.isEnabled();
        if (searchEnabled) {
            addTitleContainer(inflater, (SlidingPaneLayout) view);
        }
        mHeaderView = view.findViewById(R.id.preferences_header);

        // Set up the initial width of child views.
        {
            var resources = view.getResources();
            View detailView =
                    view.findViewById(
                            searchEnabled ? R.id.preferences_detail_pane : R.id.preferences_detail);
            LayoutParams params = detailView.getLayoutParams();
            // Set the minimum required width of detailed view here, so that the
            // SlidingPaneLayout handles single/multi column switch.
            params.width =
                    resources.getDimensionPixelSize(R.dimen.settings_min_multi_column_screen_width)
                            - resources.getDimensionPixelSize(R.dimen.settings_narrow_header_width);
            detailView.setLayoutParams(params);
        }
        // Register the callback to update header size if needed.
        view.addOnLayoutChangeListener(
                (View v,
                        int left,
                        int top,
                        int right,
                        int bottom,
                        int oldLeft,
                        int oldTop,
                        int oldRight,
                        int oldBottom) -> {
                    updateHeaderLayout(v.findViewById(R.id.preferences_header));
                });
        return view;
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

    /**
     * Updates the header layout depending on the current screen size.
     *
     * @param view The header view instance.
     */
    private void updateHeaderLayout(View view) {
        var resources = view.getResources();
        int screenWidthDp = resources.getConfiguration().screenWidthDp;
        int headerWidth =
                resources.getDimensionPixelSize(
                        screenWidthDp >= WIDE_HEADER_SCREEN_WIDTH_DP
                                ? org.chromium.chrome.R.dimen.settings_wide_header_width
                                : org.chromium.chrome.R.dimen.settings_narrow_header_width);

        boolean menuLayoutUpdated = mSlideable != getSlidingPaneLayout().isSlideable();
        mSlideable = getSlidingPaneLayout().isSlideable();

        // Update only when changed to avoid requesting re-layout to the system.
        LayoutParams params = view.getLayoutParams();
        if (headerWidth != params.width) {
            params.width = headerWidth;
            view.setLayoutParams(params);
            menuLayoutUpdated = true;
        }

        if (menuLayoutUpdated) {
            for (Observer o : mObservers) {
                o.onHeaderLayoutUpdated();
            }
        }
    }

    /** Returns whether the current layout is in two-pane mode. */
    boolean isTwoPane() {
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
            implements SlidingPaneLayout.PanelSlideListener {
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
            updateEnabled();
        }

        @Override
        public void onPanelClosed(View panel) {
            updateEnabled();
        }

        void updateEnabled() {
            // Trigger closePane() when
            // - in one-column mode
            // - the detailed pane is open (i.e., not on the main menu)
            // - the fragment back stack is empty (i.e., with the above condition
            //   this means the subpage directly under the main menu).
            boolean enabled =
                    getSlidingPaneLayout().isSlideable()
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
                ObservableSupplier<String> titleSupplier,
                int backStackCount,
                @Nullable String mainMenuKey) {
            this.uuid = uuid;
            this.titleSupplier = titleSupplier;
            this.backStackCount = backStackCount;
            this.mainMenuKey = mainMenuKey;
        }

        public final String uuid;

        public final ObservableSupplier<String> titleSupplier;

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

        @Override
        public void onFragmentResumed(@NonNull FragmentManager fm, @NonNull Fragment f) {
            if (f instanceof MainSettings) {
                // Skip main settings which is visible in the header pane.
                return;
            }

            if (f instanceof DialogFragment dialogFragment && dialogFragment.getShowsDialog()) {
                // Skip on showing a dialog UI.
                return;
            }

            // This is coming from the click on header pane pref.
            int backStackCount = fm.getBackStackEntryCount();
            if (backStackCount == 0) {
                mTitles.clear();
            }

            if (f instanceof EmbeddableSettingsPage page) {
                ObservableSupplier<String> titleSupplier = page.getPageTitle();
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
                } else {
                    // Move back from the detailed page.
                    for (int i = mTitles.size() - 1; i > index; --i) {
                        mTitles.remove(i);
                    }
                }
            }

            for (Observer o : mObservers) {
                o.onTitleUpdated();
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

    public void addObserver(@NonNull Observer o) {
        mObservers.add(o);
    }

    public void removeObserver(@NonNull Observer o) {
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
                            mOnBackPressedCallback.updateEnabled();
                        });
        getChildFragmentManager()
                .addOnBackStackChangedListener(
                        () -> {
                            mOnBackPressedCallback.updateEnabled();
                        });

        requireActivity().getOnBackPressedDispatcher().addCallback(this, mOnBackPressedCallback);

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
