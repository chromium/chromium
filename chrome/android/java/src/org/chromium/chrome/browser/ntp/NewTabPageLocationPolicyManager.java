// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceUtil;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.url_constants.PolicyUrlOverrideRegistry;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Manages the NewTabPageLocation policy. */
@NullMarked
public class NewTabPageLocationPolicyManager {
    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;
    private @Nullable Profile mProfile;

    private static @Nullable PrefChangeRegistrar sPrefChangeRegistrarForTesting;

    /** Inner class to hold the singleton instance. */
    private static class LazyHolder {
        static final NewTabPageLocationPolicyManager INSTANCE =
                new NewTabPageLocationPolicyManager();
    }

    public static NewTabPageLocationPolicyManager getInstance() {
        return LazyHolder.INSTANCE;
    }

    private NewTabPageLocationPolicyManager() {}

    public void onFinishNativeInitialization(Profile profile) {
        mProfile = profile;
        if (mPrefChangeRegistrar != null) return;

        if (sPrefChangeRegistrarForTesting != null) {
            mPrefChangeRegistrar = sPrefChangeRegistrarForTesting;
        } else {
            mPrefChangeRegistrar = PrefServiceUtil.createFor(mProfile);
        }
        mPrefChangeRegistrar.addObserver(
                Pref.NEW_TAB_PAGE_LOCATION_OVERRIDE, this::onPreferenceChange);

        // Sync with the latest policy value.
        onPreferenceChange();
    }

    @VisibleForTesting
    void onPreferenceChange() {
        if (mProfile == null) return;

        PrefService prefService = UserPrefs.get(mProfile);
        PolicyUrlOverrideRegistry.setIsNewTabPageLocationOverriddenByPolicy(
                prefService.isManagedPreference(Pref.NEW_TAB_PAGE_LOCATION_OVERRIDE));
    }

    static void setPrefChangeRegistrarForTesting(PrefChangeRegistrar prefChangeRegistrarForTest) {
        var oldValue = sPrefChangeRegistrarForTesting;
        sPrefChangeRegistrarForTesting = prefChangeRegistrarForTest;
        ResettersForTesting.register(
                () -> {
                    sPrefChangeRegistrarForTesting = oldValue;
                });
    }

    void destroy() {
        if (mPrefChangeRegistrar != null) {
            mPrefChangeRegistrar.removeObserver(Pref.NEW_TAB_PAGE_LOCATION_OVERRIDE);
            mPrefChangeRegistrar.destroy();
            mPrefChangeRegistrar = null;
        }
        mProfile = null;
    }
}
