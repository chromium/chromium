// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;
import android.os.Bundle;
import android.view.View;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;

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

        // Only apply padding to PreferenceFragmentCompat subclasses or the main fragment.
        if (!(fragment instanceof PreferenceFragmentCompat)
                && (mMainFragmentTag == null || !mMainFragmentTag.equals(fragment.getTag()))) {
            return;
        }

        // Apply wide display padding exactly once synchronously when view is created so initial
        // frame renders with correct padding. Updates are handled by WideDisplayPadding, which
        // has an OnLayoutChangeListener.
        WideDisplayPadding.apply(fragment, mIsTwoColumnSettingsVisibleSupplier, paddingPx);
    }
}
