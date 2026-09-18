// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;
import android.os.Bundle;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;

import java.util.function.BooleanSupplier;

/** Fragment lifecycle callbacks to apply padding to settings fragments on wide displays. */
@NullMarked
public class WideDisplayPaddingApplier extends FragmentManager.FragmentLifecycleCallbacks {
    private final Context mContext;
    private final BooleanSupplier mIsTwoColumnSettingsVisibleSupplier;
    private final @Nullable String mMainFragmentTag;

    /**
     * @param context The context to retrieve resources.
     * @param isTwoColumnSettingsVisibleSupplier Supplier to check if two-column settings is
     *     visible.
     * @param mainFragmentTag Optional tag of the main fragment to apply padding to even if it is
     *     not PreferenceFragmentCompat.
     */
    public WideDisplayPaddingApplier(
            Context context,
            BooleanSupplier isTwoColumnSettingsVisibleSupplier,
            @Nullable String mainFragmentTag) {
        mContext = context;
        mIsTwoColumnSettingsVisibleSupplier = isTwoColumnSettingsVisibleSupplier;
        mMainFragmentTag = mainFragmentTag;
    }

    @Override
    public void onFragmentViewCreated(
            FragmentManager fragmentManager,
            Fragment fragment,
            View view,
            @Nullable Bundle savedInstanceState) {
        int minGapPx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.settings_multi_column_pane_gap);
        int paddingPx = (fragment instanceof MainSettings) ? 0 : minGapPx;

        if (!shouldApplyPadding(fragment)) {
            return;
        }

        // Apply wide display padding exactly once synchronously when view is created so initial
        // frame renders with correct padding. Updates are handled by WideDisplayPadding, which
        // has an OnLayoutChangeListener.
        WideDisplayPadding.apply(fragment, mIsTwoColumnSettingsVisibleSupplier, paddingPx);
    }

    /** Returns whether the given fragment should have wide display padding applied to it. */
    @VisibleForTesting
    boolean shouldApplyPadding(Fragment fragment) {
        // MultiColumnSettings is the container that hosts both columns. Its child fragments are
        // padded individually, so padding the container too would inset the content twice.
        if (fragment instanceof MultiColumnSettings) return false;

        // Callbacks are registered recursively, so the FragmentManager also contains fragments
        // that are not settings pages, most notably dialog fragments. PreferenceFragmentCompat
        // and EmbeddableSettingsPage together identify the actual settings pages. The main
        // fragment is included as well.
        return fragment instanceof PreferenceFragmentCompat
                || fragment instanceof EmbeddableSettingsPage
                || (mMainFragmentTag != null && mMainFragmentTag.equals(fragment.getTag()));
    }
}
