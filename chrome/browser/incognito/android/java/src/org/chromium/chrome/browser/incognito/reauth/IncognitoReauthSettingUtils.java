// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import android.app.Activity;
import android.app.KeyguardManager;
import android.content.Context;
import android.content.Intent;
import android.provider.Settings;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.incognito.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.text.SpanApplier;

/** A utility class to provide helper methods for the Incognito re-authentication lock setting. */
public class IncognitoReauthSettingUtils {
    private static Boolean sIsDeviceScreenLockEnabledForTesting;

    /**
     * @return A boolean indicating if the screen lock is enabled in device or not.
     */
    public static boolean isDeviceScreenLockEnabled() {
        if (sIsDeviceScreenLockEnabledForTesting != null) {
            return sIsDeviceScreenLockEnabledForTesting;
        }

        KeyguardManager keyguardManager =
                ((KeyguardManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.KEYGUARD_SERVICE));
        assert keyguardManager != null;
        return keyguardManager.isDeviceSecure();
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
        return isDeviceScreenLockEnabled()
                ? activity.getString(
                        R.string.settings_incognito_tab_lock_summary_android_setting_on)
                : buildLinkToAndroidScreenLockSettings(activity);
    }

    /**
     * @return An {@link Intent} with the action to launch the Android security settings
     *         page.
     */
    public static Intent getSystemSecuritySettingsIntent() {
        Intent i = new Intent(Settings.ACTION_SECURITY_SETTINGS);
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return i;
    }

    public static void setIsDeviceScreenLockEnabledForTesting(boolean value) {
        sIsDeviceScreenLockEnabledForTesting = value;
        ResettersForTesting.register(() -> sIsDeviceScreenLockEnabledForTesting = null);
    }

    // TODO(crbug.com/40197623): Use NoUnderlineClickableSpan here to build the
    // summary string which takes the user to Android system settings. The summary
    // click behaviour is dependent on {@link IncognitoReauthSettingSwitchPreference} so
    // need to refactor that as well.
    private static SpannableString buildLinkToAndroidScreenLockSettings(Activity activity) {
        int color = SemanticColorUtils.getDefaultTextColorLink(activity);
        ForegroundColorSpan linkSpan = new ForegroundColorSpan(color);

        return SpanApplier.applySpans(
                activity.getString(
                        R.string.settings_incognito_tab_lock_summary_android_setting_off),
                new SpanApplier.SpanInfo("<link>", "</link>", linkSpan));
    }
}
