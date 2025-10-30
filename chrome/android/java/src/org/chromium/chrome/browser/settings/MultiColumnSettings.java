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
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

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

    /** Caches the current header panel width in px. */
    private int mHeaderPanelWidthPx;

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
        mHeaderView = view.findViewById(R.id.preferences_header);
        // Set up the initial width of child views.
        {
            var resources = view.getResources();
            View detailView = view.findViewById(R.id.preferences_detail);
            LayoutParams params = detailView.getLayoutParams();
            // Set the minimum required width of detailed view here, so that the
            // SlidingPaneLayout handles single/multi column switch.
            params.width =
                    resources.getDimensionPixelSize(
                                    org.chromium.chrome.R.dimen
                                            .settings_min_multi_column_screen_width)
                            - resources.getDimensionPixelSize(
                                    org.chromium.chrome.R.dimen.settings_narrow_header_width);
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
            mHeaderPanelWidthPx = headerWidth;
            menuLayoutUpdated = true;
        }

        if (menuLayoutUpdated) {
            for (Observer o : mObservers) {
                o.onHeaderLayoutUpdated();
            }
        }
    }

    public int getHeaderPanelWidthPx() {
        return mHeaderPanelWidthPx;
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

    static class Title {
        Title(ObservableSupplier<String> titleSupplier, int backStackCount) {
            this.titleSupplier = titleSupplier;
            this.backStackCount = backStackCount;
        }

        public final ObservableSupplier<String> titleSupplier;

        /** the number of back stack entries when the fragment started */
        public final int backStackCount;
    }

    static class FragmentTracker extends FragmentManager.FragmentLifecycleCallbacks {
        final List<Title> mTitles = new ArrayList<>();
        private final List<Observer> mObservers;

        FragmentTracker(List<Observer> observers) {
            mObservers = observers;
        }

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
                    mTitles.add(new Title(titleSupplier, backStackCount));
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
        super.onCreate(savedInstanceState);
        getChildFragmentManager()
                .registerFragmentLifecycleCallbacks(mFragmentTracker, /* recursive= */ false);
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
