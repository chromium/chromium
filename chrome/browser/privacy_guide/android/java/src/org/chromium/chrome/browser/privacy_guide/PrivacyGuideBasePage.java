// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import androidx.fragment.app.Fragment;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ProfileDependentSetting;

/**
 * Handles common dependencies for pages of the Privacy Guide.
 */
public abstract class PrivacyGuideBasePage extends Fragment implements ProfileDependentSetting {
    private Profile mProfile;

    /** Return the profile associated with this page. */
    public Profile getProfile() {
        return mProfile;
    }

    @Override
    public void setProfile(Profile profile) {
        mProfile = profile;
    }
}
