// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Interface implemented by activities hosting settings fragments. */
@NullMarked
public interface SettingsActivityInterface {
    /** Returns the FragmentManager for interacting with fragments associated with this activity. */
    FragmentManager getSupportFragmentManager();

    /** Returns the main fragment being displayed. */
    @Nullable Fragment getMainFragment();

    /**
     * Returns the MultiColumnSettings fragment if it is running in SettingsMultiColumn mode.
     * Returns it as a generic Fragment to avoid circular dependencies. Callers may safely cast the
     * return value to a MultiColumnSettings object.
     */
    @Nullable Fragment getMultiColumnSettings();

    /** Changes the desired orientation of this activity. */
    void setRequestedOrientation(int requestedOrientation);

    /** Finishes the specified settings fragment. */
    void finishCurrentSettings(Fragment fragment);
}
