// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.core.content.OnConfigurationChangedProvider;
import androidx.fragment.app.Fragment;
import androidx.preference.PreferenceFragmentCompat;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.PaddedItemDecorationWithDivider;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizer;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizerUtil;

/** Applies the padding to the fragment for wide displays. */
public class WideDisplayPadding {
    @NonNull private final Context mContext;
    @NonNull private final Fragment mFragment;
    @NonNull private final View mContent;
    private final int mMinWidePaddingPixels;
    @NonNull private final UiConfig mUiConfig;

    private WideDisplayPadding(
            Fragment fragment, OnConfigurationChangedProvider onConfigurationChangedProvider) {
        mContext = fragment.requireContext();
        mFragment = fragment;
        mContent = fragment.getView();

        mMinWidePaddingPixels =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.settings_wide_display_min_padding);

        // For settings with a recycler view, add paddings to the side so the content is
        // scrollable; otherwise, add the padding to the content.
        RecyclerView recyclerView = mContent.findViewById(R.id.recycler_view);
        View paddedView = recyclerView == null ? mContent : recyclerView;
        mUiConfig = new UiConfig(paddedView);

        boolean hasPreferenceRecyclerView =
                fragment instanceof PreferenceFragmentCompat && recyclerView != null;
        if (hasPreferenceRecyclerView) {
            // Invalidate decorations to reset.
            mUiConfig.addObserver(
                    (newDisplayStyle) -> {
                        recyclerView.invalidateItemDecorations();
                    });
        }

        // Update padding on configuration changes.
        onConfigurationChangedProvider.addOnConfigurationChangedListener(
                (newConfig) -> {
                    mUiConfig.updateDisplayStyle();
                });

        if (!hasPreferenceRecyclerView) {
            ViewResizer.createAndAttach(paddedView, mUiConfig, 0, mMinWidePaddingPixels);
            return;
        }

        // Configure divider style if the fragment has a recycler view.
        // Remove the default divider that PreferenceFragmentCompat initialized. This is a
        // workaround as outer class has no access to the private DividerDecoration in
        // PreferenceFragmentCompat. See https://crbug.com/1293429.
        ((PreferenceFragmentCompat) fragment).setDivider(null);

        CustomDividerFragment customDividerFragment =
                fragment instanceof CustomDividerFragment ? (CustomDividerFragment) fragment : null;

        Supplier<Integer> itemOffsetSupplier =
                () -> getItemOffset(mUiConfig.getCurrentDisplayStyle());
        PaddedItemDecorationWithDivider itemDecoration =
                new PaddedItemDecorationWithDivider(itemOffsetSupplier);
        Drawable dividerDrawable = getDividerDrawable();

        // Early return if (a)Fragment implements CustomDividerFragment and explicitly don't
        // want a divider OR (b) dividerDrawable not defined.
        if ((customDividerFragment != null && !customDividerFragment.hasDivider())
                || dividerDrawable == null) {
            recyclerView.addItemDecoration(itemDecoration);
            return;
        }

        // Configure the customized divider for the rest of the Fragments.
        Supplier<Integer> dividerStartPaddingSupplier =
                () ->
                        customDividerFragment != null
                                ? customDividerFragment.getDividerStartPadding()
                                : 0;
        Supplier<Integer> dividerEndPaddingSupplier =
                () ->
                        customDividerFragment != null
                                ? customDividerFragment.getDividerEndPadding()
                                : 0;
        itemDecoration.setDividerWithPadding(
                dividerDrawable, dividerStartPaddingSupplier, dividerEndPaddingSupplier);
        recyclerView.addItemDecoration(itemDecoration);
    }

    /**
     * Applies the padding to the fragment for wide displays.
     *
     * <p>Call this method exactly once with a top-level fragment on its creation.
     *
     * @param fragment The fragment to apply padding to.
     * @param onConfigurationChangedProvider Used to register for OnConfigurationChanged event to
     *     update the padding on configuration changes.
     */
    public static void apply(
            Fragment fragment, OnConfigurationChangedProvider onConfigurationChangedProvider) {
        new WideDisplayPadding(fragment, onConfigurationChangedProvider);
    }

    @NonNull
    private Integer getItemOffset(DisplayStyle displayStyle) {
        if (displayStyle.isWide()) {
            return ViewResizerUtil.computePaddingForWideDisplay(
                    mContext, null, mMinWidePaddingPixels);
        }
        return 0;
    }

    // Get the divider drawable from AndroidX Pref attribute to keep things consistent.
    private Drawable getDividerDrawable() {
        TypedArray ta =
                mContext.obtainStyledAttributes(
                        null,
                        R.styleable.PreferenceFragmentCompat,
                        R.attr.preferenceFragmentCompatStyle,
                        0);
        final Drawable divider =
                ta.getDrawable(R.styleable.PreferenceFragmentCompat_android_divider);
        ta.recycle();

        return divider;
    }
}
