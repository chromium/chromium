// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.provider.Settings;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.R;
import org.chromium.ui.text.SpanApplier;

/**
 * This class is responsible for managing the reauthentication flow when the Incognito session is
 * locked.
 */
public class IncognitoReauthManager {
    @VisibleForTesting
    private static Boolean sIsDeviceScreenLockEnabledForTesting;

    @VisibleForTesting
    private static Boolean sShouldShowSettingForTesting;

    /**
     * @return A boolean indicating if the Incognito lock setting needs to be shown in the Privacy
     *         and Security settings.
     */
    public static boolean shouldShowSetting() {
        if (sShouldShowSettingForTesting != null) {
            return sShouldShowSettingForTesting;
        }

        // The current phase relies on using the {@link BiometricManager} API which was added in
        // Android Version 29.
        return ChromeFeatureList.isEnabled(ChromeFeatureList.INCOGNITO_REAUTHENTICATION_FOR_ANDROID)
                && (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q);
    }

    /**
     * @return A boolean indicating if the screen lock is enabled in device or not.
     */
    public static boolean isDeviceScreenLockEnabled() {
        if (sIsDeviceScreenLockEnabledForTesting != null) {
            return sIsDeviceScreenLockEnabledForTesting;
        }

        // TODO(crbug.com/1227656): This would be added later when the Incognito reauth MVC is in
        // place.
        return false;
    }

    /**
     * This method returns the summary string that needs to be set in the Incognito lock setting.
     * The content of the summary string depends on whether the screen lock is enabled on device or
     * not.
     *
     * @param activity The {@link Activity} from which the string resource must be fetched.
     * @return A {@link CharSequence} containing the summary string for the Incognito lock setting.
     */
    public static CharSequence getSummaryString(Activity activity) {
        return (isDeviceScreenLockEnabled()) ? activity.getString(
                       R.string.settings_incognito_tab_lock_summary_android_setting_on)
                                             : buildIntentToAndroidScreenLockSettingsLink(activity);
    }

    /**
     * @return An {@link Intent} intent with the action to launch the Android security settings
     *         page.
     */
    public static Intent getSystemLocationSettingsIntent() {
        Intent i = new Intent(Settings.ACTION_SECURITY_SETTINGS);
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return i;
    }

    @VisibleForTesting
    public static void setShouldShowSettingForTesting(boolean value) {
        sShouldShowSettingForTesting = value;
    }

    @VisibleForTesting
    public static void setIsDeviceScreenLockEnabledForTesting(boolean value) {
        sIsDeviceScreenLockEnabledForTesting = value;
    }

    private static SpannableString buildIntentToAndroidScreenLockSettingsLink(Activity activity) {
        int color = ApiCompatibilityUtils.getColor(
                activity.getResources(), R.color.default_control_color_active);
        ForegroundColorSpan linkSpan = new ForegroundColorSpan(color);

        return SpanApplier.applySpans(
                activity.getString(
                        R.string.settings_incognito_tab_lock_summary_android_setting_off),
                new SpanApplier.SpanInfo("<link>", "</link>", linkSpan));
    }
}
