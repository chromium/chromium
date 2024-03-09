// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.privacy_sandbox.IpProtectionDelegate;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Chrome side implementation of the Ip protection delegate. Delivers Ip protection preference
 * functionality.
 */
public class ChromeIpProtectionDelegate implements IpProtectionDelegate {
    private final Profile mProfile;

    /**
     * Default {@link ChromeIpProtectionDelegate} constructor.
     *
     * @param profile {@link Profile} object.
     */
    public ChromeIpProtectionDelegate(Profile profile) {
        mProfile = profile;
    }

    /**
     * @return whether Ip protection is enabled.
     */
    @Override
    public boolean isIpProtectionEnabled() {
        return UserPrefs.get(mProfile).getBoolean(Pref.IP_PROTECTION_ENABLED);
    }

    /** Sets the value of the Ip protection preference. */
    @Override
    public void setIpProtection(boolean enabled) {
        UserPrefs.get(mProfile).setBoolean(Pref.IP_PROTECTION_ENABLED, enabled);
    }
}
