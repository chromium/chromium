// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.datareduction;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.about.AboutSettingsBridge;

/**
 * Helper functions for displaying the various data reduction proxy promos. The promo screens
 * inform users of the benefits of Data Saver.
 */
public class DataReductionPromoUtils {
    /**
     * Key used to save whether the first run experience or second run promo screen has been shown.
     */
    private static final String SHARED_PREF_DISPLAYED_FRE_OR_SECOND_RUN_PROMO =
            "displayed_data_reduction_promo";
    /**
     * Key used to save the time in milliseconds since epoch that the first run experience or second
     * run promo was shown.
     */
    private static final String SHARED_PREF_DISPLAYED_FRE_OR_SECOND_PROMO_TIME_MS =
            "displayed_data_reduction_promo_time_ms";
    /**
     * Key used to save the Chrome version the first run experience or second run promo was shown
     * in.
     */
    private static final String SHARED_PREF_DISPLAYED_FRE_OR_SECOND_PROMO_VERSION =
            "displayed_data_reduction_promo_version";
    /**
     * Key used to save whether the user opted out of the data reduction proxy in the FRE promo.
     */
    private static final String SHARED_PREF_FRE_PROMO_OPT_OUT = "fre_promo_opt_out";
    /**
     * Key used to save whether the infobar promo has been shown.
     */
    private static final String SHARED_PREF_DISPLAYED_INFOBAR_PROMO =
            "displayed_data_reduction_infobar_promo";
    /**
     * Key used to save the Chrome version the infobar promo was shown in.
     */
    private static final String SHARED_PREF_DISPLAYED_INFOBAR_PROMO_VERSION =
            "displayed_data_reduction_infobar_promo_version";
    /**
     * Key used to save the saved bytes when the milestone promo was last shown. This value is
     * initialized to the bytes saved for data saver users that had data saver turned on when this
     * pref was added. This prevents us from showing promo for savings that have already happened
     * for existing users.
     * Note: For historical reasons, this pref key is misnamed. This promotion used to be conveyed
     * in a snackbar but was moved to an IPH in M74.
     */
    private static final String SHARED_PREF_DISPLAYED_MILESTONE_PROMO_SAVED_BYTES =
            "displayed_data_reduction_snackbar_promo_saved_bytes";

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
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(SHARED_PREF_DISPLAYED_FRE_OR_SECOND_RUN_PROMO, true)
                .putLong(SHARED_PREF_DISPLAYED_FRE_OR_SECOND_PROMO_TIME_MS,
                        System.currentTimeMillis())
                .putString(SHARED_PREF_DISPLAYED_FRE_OR_SECOND_PROMO_VERSION,
                        AboutSettingsBridge.getApplicationVersion())
                .apply();
    }

    /**
     * Returns whether the data reduction proxy first run experience or second run promo has been
     * displayed before.
     *
     * @return Whether the data reduction proxy promo has been displayed.
     */
    public static boolean getDisplayedFreOrSecondRunPromo() {
        return ContextUtils.getAppSharedPreferences().getBoolean(
                SHARED_PREF_DISPLAYED_FRE_OR_SECOND_RUN_PROMO, false);
    }

    /**
     * Returns the version the data reduction proxy first run experience or second run promo promo
     * was displayed on. If one of the promos has not been displayed, returns an empty string.
     *
     * @return The version the data reduction proxy promo was displayed on.
     */
    public static String getDisplayedFreOrSecondRunPromoVersion() {
        return ContextUtils.getAppSharedPreferences().getString(
                SHARED_PREF_DISPLAYED_FRE_OR_SECOND_PROMO_VERSION, "");
    }

    /**
     * Saves shared prefs indicating that the data reduction proxy first run experience promo screen
     * was displayed and the user opted out.
     *
     * @param optOut Whether the user opted out of using the data reduction proxy.
     */
    public static void saveFrePromoOptOut(boolean optOut) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(SHARED_PREF_FRE_PROMO_OPT_OUT, optOut)
                .apply();
    }

    /**
     * Returns whether the user saw the data reduction proxy first run experience promo and opted
     * out.
     *
     * @return Whether the user opted out of the data reduction proxy first run experience promo.
     */
    public static boolean getOptedOutOnFrePromo() {
        return ContextUtils.getAppSharedPreferences().getBoolean(
                SHARED_PREF_FRE_PROMO_OPT_OUT, false);
    }

    /**
     * Saves shared prefs indicating that the data reduction proxy infobar promo has been displayed
     * at the current time.
     */
    public static void saveInfoBarPromoDisplayed() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(SHARED_PREF_DISPLAYED_INFOBAR_PROMO, true)
                .putString(SHARED_PREF_DISPLAYED_INFOBAR_PROMO_VERSION,
                        AboutSettingsBridge.getApplicationVersion())
                .apply();
    }

    /**
     * Returns whether the data reduction proxy infobar promo has been displayed before.
     *
     * @return Whether the data reduction proxy infobar promo has been displayed.
     */
    public static boolean getDisplayedInfoBarPromo() {
        return ContextUtils.getAppSharedPreferences().getBoolean(
                SHARED_PREF_DISPLAYED_INFOBAR_PROMO, false);
    }

    /** See {@link #SHARED_PREF_DISPLAYED_MILESTONE_PROMO_SAVED_BYTES}. */
    public static void saveMilestonePromoDisplayed(long dataSavingsInBytes) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putLong(SHARED_PREF_DISPLAYED_MILESTONE_PROMO_SAVED_BYTES, dataSavingsInBytes)
                .apply();
    }

    /**
     * Returns the data savings in bytes from when the milestone promo was last displayed.
     *
     * @return The data savings in bytes, or -1 if the promo has not been displayed before.
     */
    public static long getDisplayedMilestonePromoSavedBytes() {
        return ContextUtils.getAppSharedPreferences().getLong(
                SHARED_PREF_DISPLAYED_MILESTONE_PROMO_SAVED_BYTES, -1);
    }

    /**
     * Returns a boolean indicating that the data savings in bytes on the first upgrade to the
     * version that shows the milestone promo has been initialized.
     *
     * @return Whether that the starting saved bytes have been initialized.
     */
    public static boolean hasMilestonePromoBeenInitWithStartingSavedBytes() {
        return ContextUtils.getAppSharedPreferences().contains(
                SHARED_PREF_DISPLAYED_MILESTONE_PROMO_SAVED_BYTES);
    }
}
