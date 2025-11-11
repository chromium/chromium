// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.fragment.app.Fragment;
import androidx.preference.PreferenceFragmentCompat;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.PaddedItemDecorationWithDivider;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizer;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizerUtil;

import java.util.function.Supplier;

/** Applies the padding to the fragment for wide displays. */
@NullMarked
public class WideDisplayPadding {
    private final Context mContext;
    private final @Nullable View mContent;
    private final int mMinWidePaddingPixels;
    private final UiConfig mUiConfig;

    private WideDisplayPadding(Fragment fragment, SettingsActivity settingsActivity) {
        mContext = fragment.requireContext();
        mContent = fragment.getView();

        mMinWidePaddingPixels =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.settings_wide_display_min_padding);

        // For settings with a recycler view, add paddings to the side so the content is
        // scrollable; otherwise, add the padding to the content.
        assumeNonNull(mContent);
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
        settingsActivity.addOnConfigurationChangedListener(
                (newConfig) -> {
                    mUiConfig.updateDisplayStyle();
                });

        if (!hasPreferenceRecyclerView) {
            if (!settingsActivity.isMultiColumnSettingsVisible()) {
                // TODO(crbug.com/454247949): Short term workaround until margin for views are
                // updated.
                int defaultPadding =
                        ChromeFeatureList.sAndroidSettingsContainment.isEnabled()
                                ? mContext.getResources()
                                        .getDimensionPixelSize(
                                                R.dimen.settings_single_column_layout_margin)
                                : 0;
                ViewResizer.createAndAttach(
                        paddedView, mUiConfig, defaultPadding, mMinWidePaddingPixels);
            }
            return;
        }

        // Configure divider style if the fragment has a recycler view.
        // Remove the default divider that PreferenceFragmentCompat initialized. This is a
        // workaround as outer class has no access to the private DividerDecoration in
        // PreferenceFragmentCompat. See https://crbug.com/1293429.
        ((PreferenceFragmentCompat) fragment).setDivider(null);

        CustomDividerFragment customDividerFragment =
                fragment instanceof CustomDividerFragment ? (CustomDividerFragment) fragment : null;

        PaddedItemDecorationWithDivider itemDecoration =
                getPaddedItemDecorationWithDivider(settingsActivity, recyclerView);
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

    private PaddedItemDecorationWithDivider getPaddedItemDecorationWithDivider(
            SettingsActivity settingsActivity, RecyclerView recyclerView) {
        Supplier<Integer> itemOffsetSupplier =
                () -> {
                    boolean applyHorizontalPadding =
                            !settingsActivity.isMultiColumnSettingsVisible();

                    return applyHorizontalPadding
                            ? getItemOffset(mUiConfig.getCurrentDisplayStyle(), recyclerView)
                            : 0;
                };
        PaddedItemDecorationWithDivider itemDecoration =
                new PaddedItemDecorationWithDivider(itemOffsetSupplier);
        return itemDecoration;
    }

    /**
     * Applies the padding to the fragment for wide displays.
     *
     * <p>Call this method exactly once with a top-level fragment on its creation.
     *
     * @param fragment The fragment to apply padding to.
     * @param settingsActivity The settings activity to observe for configuration changes.
     */
    public static void apply(Fragment fragment, SettingsActivity settingsActivity) {
        new WideDisplayPadding(fragment, settingsActivity);
    }

    private Integer getItemOffset(DisplayStyle displayStyle, View view) {
        if (displayStyle.isWide()) {
            return ViewResizerUtil.computePaddingForWideDisplay(
                    mContext, view, mMinWidePaddingPixels);
        }
        return 0;
    }

    // Get the divider drawable from AndroidX Pref attribute to keep things consistent.
    private @Nullable Drawable getDividerDrawable() {
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
