// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import androidx.fragment.app.Fragment;

import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ProfileDependentSetting;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;

/** Handles common dependencies for pages of the Privacy Guide. */
public abstract class PrivacyGuideBasePage extends Fragment
        implements ProfileDependentSetting,
                SettingsCustomTabLauncher.SettingsCustomTabLauncherClient {
    private Profile mProfile;
    private PrivacySandboxBridge mPrivacySandboxBridge;
    private SettingsCustomTabLauncher mCustomTabLauncher;

    /** Return the profile associated with this page. */
    public Profile getProfile() {
        return mProfile;
    }

    @Override
    public void setCustomTabLauncher(SettingsCustomTabLauncher customTabLauncher) {
        mCustomTabLauncher = customTabLauncher;
    }

    @Override
    public void setProfile(Profile profile) {
        mProfile = profile;
        mPrivacySandboxBridge = new PrivacySandboxBridge(profile);
    }

    /**
     * Return the {@link PrivacySandboxBridge} associated with the value set in {@link
     * #setProfile(Profile)}.
     */
    public PrivacySandboxBridge getPrivacySandboxBridge() {
        return mPrivacySandboxBridge;
    }

    /**
     * @return The launcher for CCT.
     */
    public SettingsCustomTabLauncher getCustomTabLauncher() {
        return mCustomTabLauncher;
    }
}
