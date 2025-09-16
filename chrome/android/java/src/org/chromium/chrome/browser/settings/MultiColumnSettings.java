// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceHeaderFragmentCompat;

import org.chromium.build.annotations.NullMarked;

/** Preference container implementation for SettingsActivity in multi-column mode. */
@NullMarked
public class MultiColumnSettings extends PreferenceHeaderFragmentCompat {
    /**
     * Thresdhold window DP between narrow header and wide header. If the window width is as same or
     * wider than this, the wider header should be used.
     */
    private static final int WIDE_HEADER_SCREEN_WIDTH_DP = 1200;

    @Override
    public PreferenceFragmentCompat onCreatePreferenceHeader() {
        // Main menu, which is the first page in one column mode (i.e. window is
        // small enough), or shown at left side pane in two column mode.
        return new MainSettings();
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
    private static void updateHeaderLayout(View view) {
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
        }
    }
}
