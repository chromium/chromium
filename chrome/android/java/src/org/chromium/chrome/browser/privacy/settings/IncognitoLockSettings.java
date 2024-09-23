// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import android.app.Activity;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthSettingSwitchPreference;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthSettingUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.user_prefs.UserPrefs;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A class to manage the Incognito lock setting shown in Privacy and Security page.
 * This is used in {@link PrivacySettings}.
 */
public class IncognitoLockSettings {
    private final IncognitoReauthSettingSwitchPreference mIncognitoReauthPreference;
    private final Profile mProfile;

    private boolean mIsChromeTriggered;

    @Nullable private IncognitoReauthManager mIncognitoReauthManager;

    /**
     * Represents the state of the Incognito lock setting that gets changed by the user.
     * DO NOT reorder items in this interface, because it's mirrored to UMA
     * (as IncognitoReauthToggleValueType).
     */
    @IntDef({
        IncognitoReauthToggleValueType.SETTING_DISABLED,
        IncognitoReauthToggleValueType.SETTING_ENABLED,
        IncognitoReauthToggleValueType.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface IncognitoReauthToggleValueType {
        int SETTING_DISABLED = 0;
        int SETTING_ENABLED = 1;

        int NUM_ENTRIES = 2;
    }

    public IncognitoLockSettings(
            IncognitoReauthSettingSwitchPreference incognitoReauthPreference, Profile profile) {
        mIncognitoReauthPreference = incognitoReauthPreference;
        mProfile = profile;
    }

    /**
     * Sets up the {@link IncognitoReauthSettingSwitchPreference}.
     *
     * @param activity The {@link Activity} where the {@link PrivacySettings} fragment is run. This
     *         is needed to launch the system settings activity when the preference is not
     *         interactable.
     */
    public void setUpIncognitoReauthPreference(Activity activity) {
        if (!IncognitoReauthManager.isIncognitoReauthFeatureAvailable()) {
            mIncognitoReauthPreference.setVisible(false);
            return;
        }

        mIncognitoReauthPreference.setLinkClickDelegate(
                () -> {
                    activity.startActivity(
                            IncognitoReauthSettingUtils.getSystemSecuritySettingsIntent());
                });

        mIncognitoReauthPreference.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    onIncognitoReauthPreferenceChange(activity, (boolean) newValue);
                    return true;
                });
        updateIncognitoReauthPreferenceIfNeeded(activity);
    }

    /**
     * Updates the summary, checked state and the interactability of the preference.
     *
     * This is called once when the  Incognito lock preference is setup and thereafter periodically
     * when PrivacySettings#onResume is called.
     *
     * @param activity The {@link Activity} where the {@link PrivacySettings} fragment is run. This
     *         is needed to fetch resources.
     */
    public void updateIncognitoReauthPreferenceIfNeeded(Activity activity) {
        if (!IncognitoReauthManager.isIncognitoReauthFeatureAvailable()) return;
        mIncognitoReauthPreference.setSummary(
                IncognitoReauthSettingUtils.getSummaryString(activity));
        mIncognitoReauthPreference.setPreferenceInteractable(
                IncognitoReauthSettingUtils.isDeviceScreenLockEnabled());

        boolean lastPrefValue =
                UserPrefs.get(mProfile).getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID);
        updateCheckedStatePerformedByChrome(lastPrefValue);
    }

    /** Performs cleanup. Should be called when the owning activity/fragment is destroyed. */
    public void destroy() {
        if (mIncognitoReauthManager != null) {
            mIncognitoReauthManager.destroy();
        }
    }

    /**
     * This method is responsible for initiating the re-authentication flow when a user tries to
     * change the preference value. The preference is updated iff the re-authentication was
     * successful.
     *
     * @param newValue A boolean indicating the value of the potential new state.
     */
    private void onIncognitoReauthPreferenceChange(Activity activity, boolean newValue) {
        if (mIsChromeTriggered) return;
        boolean lastPrefValue =
                UserPrefs.get(mProfile).getBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID);

        if (mIncognitoReauthManager == null) {
            mIncognitoReauthManager = new IncognitoReauthManager(activity, mProfile);
        }

        mIncognitoReauthManager.startReauthenticationFlow(
                new IncognitoReauthManager.IncognitoReauthCallback() {
                    @Override
                    public void onIncognitoReauthNotPossible() {
                        updateCheckedStatePerformedByChrome(lastPrefValue);
                    }

                    @Override
                    public void onIncognitoReauthSuccess() {
                        UserPrefs.get(mProfile)
                                .setBoolean(Pref.INCOGNITO_REAUTHENTICATION_FOR_ANDROID, newValue);
                        RecordHistogram.recordEnumeratedHistogram(
                                "Android.IncognitoReauth.PrefToggledFromSettingPage",
                                newValue
                                        ? IncognitoReauthToggleValueType.SETTING_ENABLED
                                        : IncognitoReauthToggleValueType.SETTING_DISABLED,
                                IncognitoReauthToggleValueType.NUM_ENTRIES);
                    }

                    @Override
                    public void onIncognitoReauthFailure() {
                        updateCheckedStatePerformedByChrome(lastPrefValue);
                    }
                });
    }

    /**
     * This methods updates the checked state of the preference. This is non-user triggered and
     * called only by Chrome.
     */
    private void updateCheckedStatePerformedByChrome(boolean value) {
        mIsChromeTriggered = true;
        mIncognitoReauthPreference.setChecked(value);
        mIsChromeTriggered = false;
    }
}
