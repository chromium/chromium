// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Provides information for the user feedback related policies. Monitors changes for the feedback
 * preference. Safe to call pre-native.
 *
 * <p>TODO(crbug.com/467060116): Update this class from application-scoped to one-per-profile when
 * Chrome Android supports multiple profile switching.
 */
@NullMarked
public class FeedbackPolicyManager {
    private final SharedPreferencesManager mSharedPreferenceManager;

    /** The registrar for listening to preference changes. */
    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;

    /** The current user profile. */
    private @Nullable Profile mProfile;

    /**
     * Inner class to hold the singleton instance. This pattern ensures thread-safe, lazy
     * initialization.
     */
    private static class LazyHolder {
        static final FeedbackPolicyManager INSTANCE = new FeedbackPolicyManager();
    }

    private static @Nullable FeedbackPolicyManager sInstanceForTesting;
    private static @Nullable PrefChangeRegistrar sPrefChangeRegistrarForTesting;

    /**
     * @return The singleton instance of {@link FeedbackPolicyManager}.
     */
    public static FeedbackPolicyManager getInstance() {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return LazyHolder.INSTANCE;
    }

    @VisibleForTesting
    FeedbackPolicyManager() {
        mSharedPreferenceManager = ChromeSharedPreferences.getInstance();
    }

    /**
     * Called when profile is ready to transition the manager to native mode.
     *
     * @param profile The active user profile.
     */
    public void onFinishNativeInitialization(Profile profile) {
        // Feedback policy is only applicable to regular profiles; If the profile hasn't changed, we
        // don't need to re-initialize.
        if (profile.isOffTheRecord() || mProfile == profile) return;

        // Tear down the old registrar before binding to the new profile.
        destroy();

        mProfile = profile;
        mPrefChangeRegistrar =
                (sPrefChangeRegistrarForTesting != null)
                        ? sPrefChangeRegistrarForTesting
                        : new PrefChangeRegistrar(UserPrefs.get(mProfile));

        mPrefChangeRegistrar.addObserver(Pref.USER_FEEDBACK_ALLOWED, this::onPreferenceChange);
        onPreferenceChange();
    }

    /**
     * @return True if the user feedback is allowed by enterprise policy. Safe to call pre-native.
     */
    public boolean isUserFeedbackAllowed() {
        if (!ChromeFeatureList.sUserFeedbackAllowedPolicy.isEnabled()) {
            return true; // Default fallback when flag is disabled
        }
        return mSharedPreferenceManager.readBoolean(
                ChromePreferenceKeys.POLICY_USER_FEEDBACK_ALLOWED, true);
    }

    /** Stops observing pref changes and clears the active profile. */
    @VisibleForTesting
    void destroy() {
        if (mPrefChangeRegistrar != null) {
            mPrefChangeRegistrar.destroy();
            mPrefChangeRegistrar = null;
        }
        mProfile = null;
    }

    private void onPreferenceChange() {
        boolean allowed =
                UserPrefs.get(assumeNonNull(mProfile)).getBoolean(Pref.USER_FEEDBACK_ALLOWED);

        mSharedPreferenceManager.writeBoolean(
                ChromePreferenceKeys.POLICY_USER_FEEDBACK_ALLOWED, allowed);
    }

    // Testing Helper Methods
    public static void setInstanceForTesting(FeedbackPolicyManager instance) {
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    public static void setPrefChangeRegistrarForTesting(PrefChangeRegistrar registrar) {
        var oldValue = sPrefChangeRegistrarForTesting;
        sPrefChangeRegistrarForTesting = registrar;
        ResettersForTesting.register(
                () -> {
                    sPrefChangeRegistrarForTesting = oldValue;
                });
    }
}
