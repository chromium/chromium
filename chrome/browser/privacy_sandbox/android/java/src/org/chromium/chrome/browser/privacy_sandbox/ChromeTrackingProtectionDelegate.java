// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.privacy_sandbox.TrackingProtectionDelegate;
import org.chromium.components.user_prefs.UserPrefs;

public class ChromeTrackingProtectionDelegate implements TrackingProtectionDelegate {
    private final Profile mProfile;

    public ChromeTrackingProtectionDelegate(Profile profile) {
        mProfile = profile;
    }

    @Override
    public boolean isBlockAll3PCDEnabled() {
        return UserPrefs.get(mProfile).getBoolean(Pref.BLOCK_ALL3PC_TOGGLE_ENABLED);
    }

    @Override
    public void setBlockAll3PCD(boolean enabled) {
        UserPrefs.get(mProfile).setBoolean(Pref.BLOCK_ALL3PC_TOGGLE_ENABLED, enabled);
    }

    @Override
    public boolean isDoNotTrackEnabled() {
        return UserPrefs.get(mProfile).getBoolean(Pref.ENABLE_DO_NOT_TRACK);
    }

    @Override
    public void setDoNotTrack(boolean enabled) {
        UserPrefs.get(mProfile).setBoolean(Pref.ENABLE_DO_NOT_TRACK, enabled);
    }
}
