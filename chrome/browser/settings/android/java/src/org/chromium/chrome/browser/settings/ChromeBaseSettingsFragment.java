// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import androidx.annotation.NonNull;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.chrome.browser.feedback.FragmentHelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Base class for settings in Chrome.
 *
 * Common dependencies needed by the vast majority of settings screens can be added here for
 * convenience.
 */
public abstract class ChromeBaseSettingsFragment extends PreferenceFragmentCompat
        implements ProfileDependentSetting, FragmentHelpAndFeedbackLauncher {
    private Profile mProfile;
    private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;

    /** @return The profile associated with the current Settings screen. */
    public Profile getProfile() {
        assert mProfile != null : "Attempting to use the profile before initialization.";
        return mProfile;
    }

    @Override
    public void setProfile(@NonNull Profile profile) {
        mProfile = profile;
    }

    /** @return The launcher for help and feedback actions. */
    public HelpAndFeedbackLauncher getHelpAndFeedbackLauncher() {
        assert mHelpAndFeedbackLauncher
                != null : "Attempting to use the HelpAndFeedbackLauncher before initialization.";
        return mHelpAndFeedbackLauncher;
    }

    @Override
    public void setHelpAndFeedbackLauncher(
            @NonNull HelpAndFeedbackLauncher helpAndFeedbackLauncher) {
        mHelpAndFeedbackLauncher = helpAndFeedbackLauncher;
    }
}
