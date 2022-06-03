// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.datareduction;

import org.chromium.chrome.browser.about_settings.AboutSettingsBridge;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Helper functions for displaying the various data reduction proxy promos. The promo screens
 * inform users of the benefits of Data Saver.
 */
public class DataReductionPromoUtils {

    /**
     * Returns whether any of the data reduction proxy promotions can be displayed. Checks if the
     * proxy is allowed by the DataReductionProxyConfig, already on, or if the user is managed. If
     * the data reduction proxy is managed by an administrator's policy, the user should not be
     * given a promotion to enable it.
     *
     * @return Whether the any data reduction proxy promotion has been displayed.
     */
    public static boolean canShowPromos() {
        if (!DataReductionProxySettings.getInstance().isDataReductionProxyPromoAllowed()) {
            return false;
        }
        if (DataReductionProxySettings.getInstance().isDataReductionProxyManaged()) return false;
        if (DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()) return false;
        return true;
    }

    /**
     * Saves shared prefs indicating that the data reduction proxy first run experience or second
     * run promo screen has been displayed at the current time.
     */
    public static void saveFreOrSecondRunPromoDisplayed() {
        SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();
        prefs.writeBoolean(
                ChromePreferenceKeys.DATA_REDUCTION_DISPLAYED_FRE_OR_SECOND_RUN_PROMO, true);
        prefs.writeLong(ChromePreferenceKeys.DATA_REDUCTION_DISPLAYED_FRE_OR_SECOND_PROMO_TIME_MS,
                System.currentTimeMillis());
        prefs.writeString(ChromePreferenceKeys.DATA_REDUCTION_DISPLAYED_FRE_OR_SECOND_PROMO_VERSION,
                AboutSettingsBridge.getApplicationVersion());
    }

    /**
     * Returns whether the data reduction proxy first run experience or second run promo has been
     * displayed before.
     *
     * @return Whether the data reduction proxy promo has been displayed.
     */
    public static boolean getDisplayedFreOrSecondRunPromo() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.DATA_REDUCTION_DISPLAYED_FRE_OR_SECOND_RUN_PROMO, false);
    }

    /**
     * Returns the version the data reduction proxy first run experience or second run promo promo
     * was displayed on. If one of the promos has not been displayed, returns an empty string.
     *
     * @return The version the data reduction proxy promo was displayed on.
     */
    public static String getDisplayedFreOrSecondRunPromoVersion() {
        return SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.DATA_REDUCTION_DISPLAYED_FRE_OR_SECOND_PROMO_VERSION, "");
    }

    /**
     * Saves shared prefs indicating that the data reduction proxy first run experience promo screen
     * was displayed and the user opted out.
     *
     * @param optOut Whether the user opted out of using the data reduction proxy.
     */
    public static void saveFrePromoOptOut(boolean optOut) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.DATA_REDUCTION_FRE_PROMO_OPT_OUT, optOut);
    }

    /**
     * Returns whether the user saw the data reduction proxy first run experience promo and opted
     * out.
     *
     * @return Whether the user opted out of the data reduction proxy first run experience promo.
     */
    public static boolean getOptedOutOnFrePromo() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.DATA_REDUCTION_FRE_PROMO_OPT_OUT, false);
    }

    /**
     * Saves shared prefs indicating that the data reduction proxy infobar promo has been displayed
     * at the current time.
     */
    public static void saveInfoBarPromoDisplayed() {
        SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();
        prefs.writeBoolean(ChromePreferenceKeys.DATA_REDUCTION_DISPLAYED_INFOBAR_PROMO, true);
        prefs.writeString(ChromePreferenceKeys.DATA_REDUCTION_DISPLAYED_INFOBAR_PROMO_VERSION,
                AboutSettingsBridge.getApplicationVersion());
    }

    /**
     * Returns whether the data reduction proxy infobar promo has been displayed before.
     *
     * @return Whether the data reduction proxy infobar promo has been displayed.
     */
    public static boolean getDisplayedInfoBarPromo() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.DATA_REDUCTION_DISPLAYED_INFOBAR_PROMO, false);
    }

    /** See {@link ChromePreferenceKeys#DATA_REDUCTION_DISPLAYED_MILESTONE_PROMO_SAVED_BYTES}. */
    public static void saveMilestonePromoDisplayed(long dataSavingsInBytes) {
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.DATA_REDUCTION_DISPLAYED_MILESTONE_PROMO_SAVED_BYTES,
                dataSavingsInBytes);
    }

    /**
     * Returns the data savings in bytes from when the milestone promo was last displayed.
     *
     * @return The data savings in bytes, or -1 if the promo has not been displayed before.
     */
    public static long getDisplayedMilestonePromoSavedBytes() {
        return SharedPreferencesManager.getInstance().readLong(
                ChromePreferenceKeys.DATA_REDUCTION_DISPLAYED_MILESTONE_PROMO_SAVED_BYTES, -1);
    }

    /**
     * Returns a boolean indicating that the data savings in bytes on the first upgrade to the
     * version that shows the milestone promo has been initialized.
     *
     * @return Whether that the starting saved bytes have been initialized.
     */
    public static boolean hasMilestonePromoBeenInitWithStartingSavedBytes() {
        return SharedPreferencesManager.getInstance().contains(
                ChromePreferenceKeys.DATA_REDUCTION_DISPLAYED_MILESTONE_PROMO_SAVED_BYTES);
    }
}
