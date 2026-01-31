// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.policy;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceUtil;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Provides information for New Tab Page customization related policies. Monitors changes for these
 * policies.
 */
@NullMarked
public class NtpCustomizationPolicyManager {
    /** The registrar for listening to preference changes. */
    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;

    /** The current user profile. */
    private @Nullable Profile mProfile;

    /** Whether the NTP background customization is enabled by policy. */
    private final boolean mIsNtpCustomBackgroundEnabled;

    /**
     * Inner class to hold the singleton instance. This pattern ensures thread-safe, lazy
     * initialization.
     */
    private static class LazyHolder {
        static final NtpCustomizationPolicyManager INSTANCE = new NtpCustomizationPolicyManager();
    }

    private static @Nullable NtpCustomizationPolicyManager sInstanceForTesting;
    private static @Nullable PrefChangeRegistrar sPrefChangeRegistrarForTesting;

    /**
     * @return The singleton instance of {@link NtpCustomizationPolicyManager}.
     */
    public static NtpCustomizationPolicyManager getInstance() {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return LazyHolder.INSTANCE;
    }

    @VisibleForTesting
    NtpCustomizationPolicyManager() {
        mIsNtpCustomBackgroundEnabled = getNtpCustomBackgroundEnabledFromSharedPreferences();
    }

    /** Called when profile is ready. */
    public void onFinishNativeInitialization(Profile profile) {
        mProfile = profile;
        if (mPrefChangeRegistrar != null) return;

        if (sPrefChangeRegistrarForTesting != null) {
            mPrefChangeRegistrar = sPrefChangeRegistrarForTesting;
        } else {
            mPrefChangeRegistrar = PrefServiceUtil.createFor(mProfile);
        }

        mPrefChangeRegistrar.addObserver(Pref.NTP_CUSTOM_BACKGROUND_DICT, this::onPreferenceChange);
        // Syncs with the latest policy value.
        onPreferenceChange();
    }

    /**
     * Called on Activity's onDeferredStartup() to clean up previously cached the NTP's custom
     * background data if policy is disabled.
     */
    public void onDeferredStartup() {
        if (mIsNtpCustomBackgroundEnabled) return;

        // Removes all NTP custom background related info if disabled by policy.
        NtpCustomizationUtils.resetNtpCustomBackgroundData();
    }

    /** Returns whether New Tab Page customization is allowed by enterprise policy. */
    public boolean isNtpCustomBackgroundEnabled() {
        return mIsNtpCustomBackgroundEnabled;
    }

    /**
     * Stops observing pref changes and destroys the singleton instance. Will be called from {@link
     * org.chromium.chrome.browser.ChromeActivitySessionTracker}.
     */
    public void destroy() {
        if (mPrefChangeRegistrar != null) {
            mPrefChangeRegistrar.removeObserver(Pref.NTP_CUSTOM_BACKGROUND_DICT);
            mPrefChangeRegistrar = null;
        }
        mProfile = null;
    }

    @VisibleForTesting
    void onPreferenceChange() {
        // We don't apply the policy change, but cache the latest value to the shared preference
        // and apply it on the next launch.
        setNtpCustomBackgroundEnabledToSharedPreferences(
                getNtpCustomBackgroundEnabledPolicyValue());
    }

    /** Returns the {@link PrefService} for the current profile. */
    private PrefService getPrefService() {
        return UserPrefs.get(assumeNonNull(mProfile));
    }

    /**
     * Returns the value of policy NtpCustomBackgroundEnabled.
     *
     * @return True if the preference {@link Pref.NTP_CUSTOM_BACKGROUND_DICT} isn't managed, false
     *     otherwise.
     */
    @VisibleForTesting
    boolean getNtpCustomBackgroundEnabledPolicyValue() {
        return !getPrefService().isManagedPreference(Pref.NTP_CUSTOM_BACKGROUND_DICT);
    }

    /**
     * Reads the cached value of the NTPCustomBackgroundEnabled policy from the shared preferences.
     */
    private boolean getNtpCustomBackgroundEnabledFromSharedPreferences() {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        return sharedPreferencesManager.readBoolean(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_ENABLED, true);
    }

    /**
     * Caches the value of the NTPCustomBackgroundEnabled policy to shared preferences.
     *
     * @param isNtpCustomBackgroundEnabled The value to cache.
     */
    private void setNtpCustomBackgroundEnabledToSharedPreferences(
            boolean isNtpCustomBackgroundEnabled) {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        sharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_ENABLED,
                isNtpCustomBackgroundEnabled);
    }

    public static void setInstanceForTesting(NtpCustomizationPolicyManager instance) {
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    public static void setPrefChangeRegistrarForTesting(
            PrefChangeRegistrar prefChangeRegistrarForTest) {
        var oldValue = sPrefChangeRegistrarForTesting;
        sPrefChangeRegistrarForTesting = prefChangeRegistrarForTest;
        ResettersForTesting.register(() -> sPrefChangeRegistrarForTesting = oldValue);
    }

    public void resetSharedPreferenceForTesting() {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        sharedPreferencesManager.removeKey(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_ENABLED);
    }
}
