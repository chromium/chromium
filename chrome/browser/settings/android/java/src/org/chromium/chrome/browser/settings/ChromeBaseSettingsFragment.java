// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import androidx.annotation.NonNull;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;

/**
 * Base class for settings in Chrome.
 *
 * <p>Common dependencies needed by the vast majority of settings screens can be added here for
 * convenience.
 */
public abstract class ChromeBaseSettingsFragment extends PreferenceFragmentCompat
        implements EmbeddableSettingsPage,
                ProfileDependentSetting,
                SettingsCustomTabLauncher.SettingsCustomTabLauncherClient {
    private Profile mProfile;
    private SettingsCustomTabLauncher mCustomTabLauncher;

    /**
     * @return The profile associated with the current Settings screen.
     */
    public Profile getProfile() {
        assert mProfile != null : "Attempting to use the profile before initialization.";
        return mProfile;
    }

    @Override
    public void setProfile(@NonNull Profile profile) {
        mProfile = profile;
    }

    @Override
    public void setCustomTabLauncher(SettingsCustomTabLauncher customTabLauncher) {
        mCustomTabLauncher = customTabLauncher;
    }

    /**
     * @return The launcher for help and feedback actions.
     */
    public HelpAndFeedbackLauncher getHelpAndFeedbackLauncher() {
        return HelpAndFeedbackLauncherFactory.getForProfile(mProfile);
    }

    /**
     * @return The launcher for CCT.
     */
    public SettingsCustomTabLauncher getCustomTabLauncher() {
        return mCustomTabLauncher;
    }
}
