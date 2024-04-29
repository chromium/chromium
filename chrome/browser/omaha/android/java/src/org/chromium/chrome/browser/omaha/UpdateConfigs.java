// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.text.TextUtils;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState;
import org.chromium.components.variations.VariationsAssociatedData;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Helper class for retrieving experiment configuration values and for manually testing update
 * functionality.  Use the following switches to test locally:
 * - {@link ChromeSwitches#FORCE_UPDATE_MENU_UPDATE_TYPE} (required)
 * - {@link ChromeSwitches#FORCE_SHOW_UPDATE_MENU_BADGE} (optional)
 * - {@link ChromeSwitches#MARKET_URL_FOR_TESTING} (optional)
 */
public class UpdateConfigs {
    // VariationsAssociatedData configs
    private static final String FIELD_TRIAL_NAME = "UpdateMenuItem";
    private static final String CUSTOM_SUMMARY = "custom_summary";
    private static final String MIN_REQUIRED_STORAGE_MB = "min_required_storage_for_update_mb";

    // Update state switch values.
    private static final String NONE_SWITCH_VALUE = "none";
    private static final String UPDATE_AVAILABLE_SWITCH_VALUE = "update_available";
    private static final String UNSUPPORTED_OS_VERSION_SWITCH_VALUE = "unsupported_os_version";

    private static final long DEFAULT_UPDATE_ATTRIBUTION_WINDOW_MS = 2 * DateUtils.DAY_IN_MILLIS;

    /** Possible update flow configurations. */
    @IntDef({UpdateFlowConfiguration.NEVER_SHOW, UpdateFlowConfiguration.INTENT_ONLY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UpdateFlowConfiguration {
        /** Turns off all update indicators. */
        int NEVER_SHOW = 1;

        /**
         * Requires Omaha to say an update is available, and only ever Intents out to Play Store.
         */
        int INTENT_ONLY = 2;
    }

    /**
     * @return The minimum required storage to show the update prompt or {@code -1} if there is no
     * minimum.
     */
    public static int getMinRequiredStorage() {
        String value = CommandLine.getInstance().getSwitchValue(MIN_REQUIRED_STORAGE_MB);
        if (TextUtils.isEmpty(value)) {
            value =
                    VariationsAssociatedData.getVariationParamValue(
                            FIELD_TRIAL_NAME, MIN_REQUIRED_STORAGE_MB);
        }
        if (TextUtils.isEmpty(value)) return -1;

        try {
            return Integer.parseInt(value);
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    /**
     * @return A custom update menu summary to show.  This should override the default summary for
     * 'update available' menu items.
     */
    public static String getCustomSummary() {
        return getStringParamValue(CUSTOM_SUMMARY);
    }

    /**
     * @return Whether or not to always show the update badge on the menu depending on the update
     * state.
     */
    public static boolean getAlwaysShowMenuBadge() {
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.FORCE_SHOW_UPDATE_MENU_BADGE)) {
            return true;
        }

        return false;
    }

    /** @return A test {@link UpdateState} to use or {@code null} if no test state was specified. */
    public static @UpdateState Integer getMockUpdateState() {
        String forcedUpdateType = getStringParamValue(ChromeSwitches.FORCE_UPDATE_MENU_UPDATE_TYPE);
        if (TextUtils.isEmpty(forcedUpdateType)) return null;

        switch (forcedUpdateType) {
            case NONE_SWITCH_VALUE:
                return UpdateState.NONE;
            case UPDATE_AVAILABLE_SWITCH_VALUE:
                return UpdateState.UPDATE_AVAILABLE;
            case UNSUPPORTED_OS_VERSION_SWITCH_VALUE:
                return UpdateState.UNSUPPORTED_OS_VERSION;
            default:
                return null;
        }
    }

    /**
     * @return A URL to use when an update is available if mocking out the update available menu
     * item.
     */
    public static String getMockMarketUrl() {
        return getStringParamValue(ChromeSwitches.MARKET_URL_FOR_TESTING);
    }

    /**
     * @return How long to wait before attributing an update success or failure to the Chrome update
     * mechanism.
     */
    public static long getUpdateAttributionWindowMs() {
        return DEFAULT_UPDATE_ATTRIBUTION_WINDOW_MS;
    }

    /**
     * Gets a String VariationsAssociatedData parameter. Also checks for a command-line switch
     * with the same name, for easy local testing.
     * @param paramName The name of the parameter (or command-line switch) to get a value for.
     * @return The command-line flag value if present, or the param is value if present.
     */
    private static @Nullable String getStringParamValue(String paramName) {
        String value = CommandLine.getInstance().getSwitchValue(paramName);
        if (TextUtils.isEmpty(value)) {
            value = VariationsAssociatedData.getVariationParamValue(FIELD_TRIAL_NAME, paramName);
        }
        return value;
    }
}
