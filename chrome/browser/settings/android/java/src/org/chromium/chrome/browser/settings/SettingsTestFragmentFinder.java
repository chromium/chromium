// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import androidx.fragment.app.Fragment;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Test-only helpers for locating a settings fragment hosted by a settings activity. */
@NullMarked
final class SettingsTestFragmentFinder {
    private SettingsTestFragmentFinder() {}

    /**
     * Returns the fragment of {@code fragmentClass} that {@code host} is showing, or the host's
     * main fragment if no such fragment is found.
     *
     * <p>{@link SettingsActivityInterface#getMainFragment()} returns the detail pane fragment when
     * settings is shown in multi-column mode. MainSettings lives in the header pane instead, so the
     * children of MultiColumnSettings are searched as well. See https://crbug.com/563146381.
     *
     * @param host The activity hosting settings.
     * @param fragmentClass The fragment class the test asked to be shown, or null if the test did
     *     not name one.
     */
    static @Nullable Fragment find(
            SettingsActivityInterface host, @Nullable Class<? extends Fragment> fragmentClass) {
        Fragment mainFragment = host.getMainFragment();
        if (fragmentClass == null || fragmentClass.isInstance(mainFragment)) {
            return mainFragment;
        }

        Fragment multiColumnSettings = host.getMultiColumnSettings();
        if (multiColumnSettings == null) return mainFragment;

        for (Fragment child : multiColumnSettings.getChildFragmentManager().getFragments()) {
            if (fragmentClass.isInstance(child)) return child;
        }
        return mainFragment;
    }
}
