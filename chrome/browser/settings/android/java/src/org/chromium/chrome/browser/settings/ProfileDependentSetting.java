// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.SettingsFragment;

/** Specifies that this settings entry is dependent on the current profile. */
@NullMarked
public interface ProfileDependentSetting extends SettingsFragment {
    /**
     * @param profile The currently selected profile.
     */
    void setProfile(Profile profile);
}
