// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.content.res.Resources;
import android.text.TextUtils;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState;
import org.chromium.chrome.browser.omaha.inline.FakeAppUpdateManagerWrapper;
import org.chromium.components.variations.VariationsAssociatedData;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

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
    private static final String ENABLED_VALUE = "true";
    private static final String CUSTOM_SUMMARY = "custom_summary";
    private static final String MIN_REQUIRED_STORAGE_MB = "min_required_storage_for_update_mb";

    // Update state switch values.
    private static final String NONE_SWITCH_VALUE = "none";
    private static final String UPDATE_AVAILABLE_SWITCH_VALUE = "update_available";
    private static final String UNSUPPORTED_OS_VERSION_SWITCH_VALUE = "unsupported_os_version";
    private static final String INLINE_UPDATE_SUCCESS_SWITCH_VALUE = "inline_update_success";
    private static final String INLINE_UPDATE_DIALOG_CANCELED_SWITCH_VALUE =
            "inline_update_dialog_canceled";
    private static final String INLINE_UPDATE_DIALOG_FAILED_SWITCH_VALUE =
            "inline_update_dialog_failed";
    private static final String INLINE_UPDATE_DOWNLOAD_FAILED_SWITCH_VALUE =
            "inline_update_download_failed";
    private static final String INLINE_UPDATE_DOWNLOAD_CANCELED_SWITCH_VALUE =
            "inline_update_download_canceled";
    private static final String INLINE_UPDATE_INSTALL_FAILED_SWITCH_VALUE =
            "inline_update_install_failed";

    private static final String UPDATE_FLOW_PARAM_NAME = "flow";

    private static final String UPDATE_ATTRIBUTION_WINDOW_PARAM_NAME =
            "update_attribution_window_days";
    private static final String UPDATE_NOTIFICATION_INTERVAL_PARAM_NAME =
            "update_notification_interval_days";
    private static final String UPDATE_NOTIFICATION_STATE_PARAM_NAME = "update_notification_state";
    private static final String UPDATE_NOTIFICATION_EXPERIMENTAL_PARAM_NAME =
            "update_notification_experimental_context";

    private static final long DEFAULT_UPDATE_NOTIFICATION_INTERVAL = 21 * DateUtils.DAY_IN_MILLIS;
    private static final long DEFAULT_UPDATE_ATTRIBUTION_WINDOW_MS = 2 * DateUtils.DAY_IN_MILLIS;

    /** Possible update flow configurations. */
    @IntDef({UpdateFlowConfiguration.NEVER_SHOW, UpdateFlowConfiguration.INTENT_ONLY,
            UpdateFlowConfiguration.INLINE_ONLY, UpdateFlowConfiguration.BEST_EFFORT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UpdateFlowConfiguration {
        /** Turns off all update indicators. */
        int NEVER_SHOW = 1;

        /**
         * Requires Omaha to say an update is available, and only ever Intents out to Play Store.
         */
        int INTENT_ONLY = 2;

        /**
         * Requires both Omaha and Play Store to say an update is available. Only ever uses the
         * inline update flow.
         */
        int INLINE_ONLY = 3;

        /**
         * Checks both Omaha and Play Store. If Omaha says there is an update available, but Play
         * Store says there is no update available, Intents out to Play Store. If both Omaha and
         * Play Store says an update is available, uses the inline update flow.
         */
        int BEST_EFFORT = 4;
    }

    /**
     * @return The minimum required storage to show the update prompt or {@code -1} if there is no
     * minimum.
     */
    public static int getMinRequiredStorage() {
        String value = CommandLine.getInstance().getSwitchValue(MIN_REQUIRED_STORAGE_MB);
        if (TextUtils.isEmpty(value)) {
            value = VariationsAssociatedData.getVariationParamValue(
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
            case INLINE_UPDATE_SUCCESS_SWITCH_VALUE: // Intentional fallthrough.
            case INLINE_UPDATE_DIALOG_CANCELED_SWITCH_VALUE: // Intentional fallthrough.
            case INLINE_UPDATE_DIALOG_FAILED_SWITCH_VALUE: // Intentional fallthrough.
            case INLINE_UPDATE_DOWNLOAD_FAILED_SWITCH_VALUE: // Intentional fallthrough.
            case INLINE_UPDATE_DOWNLOAD_CANCELED_SWITCH_VALUE: // Intentional fallthrough.
            case INLINE_UPDATE_INSTALL_FAILED_SWITCH_VALUE:
                // The chrome://flags configuration refers to how the inline update flow should end,
                // but we will always start at the beginning of the flow.
                return UpdateState.INLINE_UPDATE_AVAILABLE;
            default:
                return null;
        }
    }

    /**
     * For when there is a test scenario for inline updates, it returns the end state. If there is
     * no test scenario, it returns
     * {@link FakeAppUpdateManagerWrapper.Type.NO_SIMULATION}.
     * @return the end state for what the fake inline scenario should be.
     */
    public static @FakeAppUpdateManagerWrapper.Type int getMockInlineScenarioEndState() {
        String forcedUpdateType =
                UpdateConfigs.getStringParamValue(ChromeSwitches.FORCE_UPDATE_MENU_UPDATE_TYPE);
        if (TextUtils.isEmpty(forcedUpdateType)) {
            return FakeAppUpdateManagerWrapper.Type.NO_SIMULATION;
        }

        switch (forcedUpdateType) {
            case UpdateConfigs.INLINE_UPDATE_SUCCESS_SWITCH_VALUE:
                return FakeAppUpdateManagerWrapper.Type.SUCCESS;
            case UpdateConfigs.INLINE_UPDATE_DIALOG_CANCELED_SWITCH_VALUE:
                return FakeAppUpdateManagerWrapper.Type.FAIL_DIALOG_CANCEL;
            case UpdateConfigs.INLINE_UPDATE_DIALOG_FAILED_SWITCH_VALUE:
                return FakeAppUpdateManagerWrapper.Type.FAIL_DIALOG_UPDATE_FAILED;
            case UpdateConfigs.INLINE_UPDATE_DOWNLOAD_FAILED_SWITCH_VALUE:
                return FakeAppUpdateManagerWrapper.Type.FAIL_DOWNLOAD;
            case UpdateConfigs.INLINE_UPDATE_DOWNLOAD_CANCELED_SWITCH_VALUE:
                return FakeAppUpdateManagerWrapper.Type.FAIL_DOWNLOAD_CANCEL;
            case UpdateConfigs.INLINE_UPDATE_INSTALL_FAILED_SWITCH_VALUE:
                return FakeAppUpdateManagerWrapper.Type.FAIL_INSTALL;
            case UpdateConfigs.NONE_SWITCH_VALUE: // Intentional fallthrough.
            case UpdateConfigs.UPDATE_AVAILABLE_SWITCH_VALUE: // Intentional fallthrough.
            case UpdateConfigs.UNSUPPORTED_OS_VERSION_SWITCH_VALUE: // Intentional fallthrough.
            default:
                // The config requires to run through a test scenario without inline updates.
                return FakeAppUpdateManagerWrapper.Type.NONE;
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
        String configuration = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.INLINE_UPDATE_FLOW, UPDATE_ATTRIBUTION_WINDOW_PARAM_NAME);
        try {
            return Long.parseLong(configuration) * DateUtils.DAY_IN_MILLIS;
        } catch (NumberFormatException e) {
            return DEFAULT_UPDATE_ATTRIBUTION_WINDOW_MS;
        }
    }

    /**
     * @return A time interval for scheduling update notification. Unit: mills.
     */
    public static long getUpdateNotificationInterval() {
        String configuration = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.INLINE_UPDATE_FLOW, UPDATE_NOTIFICATION_INTERVAL_PARAM_NAME);
        try {
            return Long.parseLong(configuration) * DateUtils.DAY_IN_MILLIS;
        } catch (NumberFormatException e) {
            return DEFAULT_UPDATE_NOTIFICATION_INTERVAL;
        }
    }

    /**
     * @return true if update notification is enabled, false if disabled.
     */
    public static boolean isUpdateNotificationEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INLINE_UPDATE_FLOW, UPDATE_NOTIFICATION_STATE_PARAM_NAME, false);
    }

    /**
     * @return A title of update notification.
     */
    public static String getUpdateNotificationTitle() {
        Resources resources = ContextUtils.getApplicationContext().getResources();
        String configuration = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.INLINE_UPDATE_FLOW, UPDATE_NOTIFICATION_EXPERIMENTAL_PARAM_NAME);
        switch (configuration) {
            case "experimental_update_chrome":
                return resources.getString(
                        R.string.update_notification_title_experimental_update_chrome);
            case "experimental_translate_the_web":
                return resources.getString(
                        R.string.update_notification_title_experimental_translate_the_web);
            case "default": // Intentional fallthrough.
            default:
                return resources.getString(R.string.update_notification_title_default);
        }
    }

    /**
     * @return A text body of update notification.
     */
    public static String getUpdateNotificationTextBody() {
        Resources resources = ContextUtils.getApplicationContext().getResources();
        String configuration = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.INLINE_UPDATE_FLOW, UPDATE_NOTIFICATION_EXPERIMENTAL_PARAM_NAME);
        switch (configuration) {
            case "experimental_update_chrome":
                return resources.getString(
                        R.string.update_notification_text_body_experimental_update_chrome);
            case "experimental_translate_the_web":
                return resources.getString(
                        R.string.update_notification_text_body_experimental_translate_the_web);
            case "default": // Intentional fallthrough.
            default:
                return resources.getString(R.string.update_notification_text_body_default);
        }
    }

    /**
     * Gets a String VariationsAssociatedData parameter. Also checks for a command-line switch
     * with the same name, for easy local testing.
     * @param paramName The name of the parameter (or command-line switch) to get a value for.
     * @return The command-line flag value if present, or the param is value if present.
     */

    @Nullable
    private static String getStringParamValue(String paramName) {
        String value = CommandLine.getInstance().getSwitchValue(paramName);
        if (TextUtils.isEmpty(value)) {
            value = VariationsAssociatedData.getVariationParamValue(FIELD_TRIAL_NAME, paramName);
        }
        return value;
    }

    /**
     * When the inline update flow is enabled, returns the variation for the current session. With
     * the inline update flow disabled it returns {@link UpdateFlowConfiguration#INTENT_ONLY}.
     * @return the current inline update flow configuration.
     */
    @UpdateFlowConfiguration
    static int getConfiguration() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.INLINE_UPDATE_FLOW)) {
            // Always use the the old flow if the inline update flow feature is not enabled.
            return UpdateFlowConfiguration.INTENT_ONLY;
        }

        switch (getFieldTrialConfigurationLowerCase()) {
            case "never_show":
                return UpdateFlowConfiguration.NEVER_SHOW;
            case "intent_only":
                return UpdateFlowConfiguration.INTENT_ONLY;
            case "inline_only":
                return UpdateFlowConfiguration.INLINE_ONLY;
            case "best_effort":
                return UpdateFlowConfiguration.BEST_EFFORT;
            default:
                // By just turning on the feature flag, assume INLINE_ONLY.
                return UpdateFlowConfiguration.INLINE_ONLY;
        }
    }

    @NonNull
    private static String getFieldTrialConfigurationLowerCase() {
        String configuration = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.INLINE_UPDATE_FLOW, UPDATE_FLOW_PARAM_NAME);
        if (configuration == null) return "";
        return configuration.toLowerCase(Locale.US);
    }
}