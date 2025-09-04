// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceHeaderFragmentCompat;

import org.chromium.build.annotations.NullMarked;

/** Preference container implementation for SettingsActivity in multi-column mode. */
@NullMarked
public class MultiColumnSettings extends PreferenceHeaderFragmentCompat {
    @Override
    public PreferenceFragmentCompat onCreatePreferenceHeader() {
        // Main menu, which is the first page in one column mode (i.e. window is
        // small enough), or shown at left side pane in two column mode.
        return new MainSettings();
    }
}
