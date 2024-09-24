// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.site_settings.ChromeSiteSettingsDelegate;
import org.chromium.components.browser_ui.site_settings.SiteSettingsDelegate;
import org.chromium.components.privacy_sandbox.TrackingProtectionDelegate;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserContextHandle;

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

    @Override
    public boolean shouldShowTrackingProtectionBrandedUi() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.TRACKING_PROTECTION_3PCD_UX);
    }

    @Override
    public boolean shouldDisplayIpProtection() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.IP_PROTECTION_UX);
    }

    @Override
    public boolean isIpProtectionEnabled() {
        return UserPrefs.get(mProfile).getBoolean(Pref.IP_PROTECTION_ENABLED);
    }

    @Override
    public void setIpProtection(boolean enabled) {
        UserPrefs.get(mProfile).setBoolean(Pref.IP_PROTECTION_ENABLED, enabled);
    }

    @Override
    public boolean shouldDisplayFingerprintingProtection() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.FINGERPRINTING_PROTECTION_UX);
    }

    @Override
    public boolean isFingerprintingProtectionEnabled() {
        return UserPrefs.get(mProfile).getBoolean(Pref.FINGERPRINTING_PROTECTION_ENABLED);
    }

    @Override
    public void setFingerprintingProtection(boolean enabled) {
        UserPrefs.get(mProfile).setBoolean(Pref.FINGERPRINTING_PROTECTION_ENABLED, enabled);
    }

    @Override
    public BrowserContextHandle getBrowserContext() {
        return mProfile;
    }

    @Override
    public SiteSettingsDelegate getSiteSettingsDelegate(Context context) {
        return new ChromeSiteSettingsDelegate(context, mProfile);
    }
}
