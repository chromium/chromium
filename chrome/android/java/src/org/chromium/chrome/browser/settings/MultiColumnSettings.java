// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceHeaderFragmentCompat;
import androidx.slidingpanelayout.widget.SlidingPaneLayout;

import org.chromium.build.annotations.NullMarked;

/** Preference container implementation for SettingsActivity in multi-column mode. */
@NullMarked
public class MultiColumnSettings extends PreferenceHeaderFragmentCompat {
    /**
     * Thresdhold window DP between narrow header and wide header. If the window width is as same or
     * wider than this, the wider header should be used.
     */
    private static final int WIDE_HEADER_SCREEN_WIDTH_DP = 1200;

    /** Caches the current header panel width in px. */
    private int mHeaderPanelWidthPx;

    private InnerOnBackPressedCallback mOnBackPressedCallback;

    private @Nullable Intent mPendingFragmentIntent;

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
        Fragment fragment = processPendingFragmentIntent();
        if (fragment != null) {
            return fragment;
        }
        return super.onCreateInitialDetailFragment();
    }

    void setPendingFragmentIntent(Intent intent) {
        mPendingFragmentIntent = intent;
    }

    @Override
    public void onResume() {
        // Update the detail pane, if the intent is specified.
        Fragment fragment = processPendingFragmentIntent();
        if (fragment != null) {
            getChildFragmentManager()
                    .beginTransaction()
                    .setReorderingAllowed(true)
                    .replace(R.id.preferences_detail, fragment)
                    .addToBackStack(null)
                    .commit();
            getSlidingPaneLayout().open();
        }

        super.onResume();
    }

    /**
     * Processes the pending Intent if there is, and returns the Fragment to be used in the detailed
     * pane.
     */
    private @Nullable Fragment processPendingFragmentIntent() {
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
        return Fragment.instantiate(requireActivity(), fragmentName, arguments);
    }

    @Override
    public @NonNull View onCreateView(
            @NonNull LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View view = super.onCreateView(inflater, container, savedInstanceState);

        // Set up the initial width of child views.
        updateHeaderLayout(view.findViewById(R.id.preferences_header));
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

        // Update only when changed to avoid requesting re-layout to the system.
        LayoutParams params = view.getLayoutParams();
        if (headerWidth != params.width) {
            params.width = headerWidth;
            view.setLayoutParams(params);
            mHeaderPanelWidthPx = headerWidth;
        }
    }

    int getHeaderPanelWidthPx() {
        return mHeaderPanelWidthPx;
    }

    /** Returns whether the current layout is in two-pane mode. */
    boolean isTwoPane() {
        return !getSlidingPaneLayout().isSlideable();
    }

    private class InnerOnBackPressedCallback extends OnBackPressedCallback
            implements SlidingPaneLayout.PanelSlideListener {
        InnerOnBackPressedCallback() {
            super(true);
            getSlidingPaneLayout().addPanelSlideListener(this);
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

    @Override
    @SuppressWarnings("MissingSuperCall")
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        // Overrides the back press button behavior provided by the library as workaround.
        // The provided behavior does not close SettingsActivity even if it shows
        // main menu in two-pane mode. Revisit later if back button behavior in the library is
        // updated.
        mOnBackPressedCallback = new InnerOnBackPressedCallback();
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
    }

    @Override
    public void onDestroy() {
        if (mOnBackPressedCallback != null) {
            mOnBackPressedCallback.remove();
        }
        super.onDestroy();
    }
}
