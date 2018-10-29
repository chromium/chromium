// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.SharedPreferences;
import android.support.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.crash.MinidumpUploadService.ProcessType;

import java.util.Collections;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

/**
 * ChromePreferenceManager stores and retrieves various values in Android shared preferences.
 */
public class ChromePreferenceManager {
    // For new int values with a default of 0, just document the key and its usage, and call
    // #readInt and #writeInt directly.
    // For new boolean values, document the key and its usage, call #readBoolean and #writeBoolean
    // directly. While calling #readBoolean, default value is required.

    /** An all-time counter of taps that triggered the Contextual Search peeking panel. */
    public static final String CONTEXTUAL_SEARCH_ALL_TIME_TAP_COUNT =
            "contextual_search_all_time_tap_count";
    /** An all-time counter of Contextual Search panel opens triggered by any gesture.*/
    public static final String CONTEXTUAL_SEARCH_ALL_TIME_OPEN_COUNT =
            "contextual_search_all_time_open_count";
    /**
     * The number of times a tap gesture caused a Contextual Search Quick Answer to be shown.
     * Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_ALL_TIME_TAP_QUICK_ANSWER_COUNT =
            "contextual_search_all_time_tap_quick_answer_count";
    /**
     * The number of times that a tap triggered the Contextual Search panel to peek since the last
     * time the panel was opened.  Note legacy string value without "open".
     */
    public static final String CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_COUNT =
            "contextual_search_tap_count";
    /**
     * The number of times a tap gesture caused a Contextual Search Quick Answer to be shown since
     * the last time the panel was opened.  Note legacy string value without "open".
     */
    public static final String CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_QUICK_ANSWER_COUNT =
            "contextual_search_tap_quick_answer_count";
    /**
     * The number of times the Contextual Search panel was opened with the opt-in promo visible.
     */
    public static final String CONTEXTUAL_SEARCH_PROMO_OPEN_COUNT =
            "contextual_search_promo_open_count";
    /**
     * The entity-data impressions count for Contextual Search, i.e. thumbnails shown in the Bar.
     * Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_ENTITY_IMPRESSIONS_COUNT =
            "contextual_search_entity_impressions_count";
    /**
     * The entity-data opens count for Contextual Search, e.g. Panel opens following thumbnails
     * shown in the Bar. Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_ENTITY_OPENS_COUNT =
            "contextual_search_entity_opens_count";
    /**
     * The Quick Action impressions count for Contextual Search, i.e. actions presented in the Bar.
     * Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_QUICK_ACTION_IMPRESSIONS_COUNT =
            "contextual_search_quick_action_impressions_count";
    /**
     * The Quick Actions taken count for Contextual Search, i.e. phone numbers dialed and similar
     * actions. Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_QUICK_ACTIONS_TAKEN_COUNT =
            "contextual_search_quick_actions_taken_count";
    /**
     * The Quick Actions ignored count, i.e. phone numbers available but not dialed.
     * Cumulative, starting at M-69.
     */
    public static final String CONTEXTUAL_SEARCH_QUICK_ACTIONS_IGNORED_COUNT =
            "contextual_search_quick_actions_ignored_count";
    /**
     * The user's previous preference setting before Unified Consent took effect, as an int, for
     * Contextual Search. This can be removed after the full rollout of Unified Consent.
     */
    public static final String CONTEXTUAL_SEARCH_PRE_UNIFIED_CONSENT_PREF =
            "contextual_search_pre_unified_consent_pref";

    /**
     * Whether the promotion for data reduction has been skipped on first invocation.
     * Default value is false.
     */
    public static final String PROMOS_SKIPPED_ON_FIRST_START = "promos_skipped_on_first_start";
    private static final String SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION =
            "signin_promo_last_shown_chrome_version";
    private static final String SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES =
            "signin_promo_last_shown_account_names";

    /**
     * This value may have been explicitly set to false when we used to keep existing low-end
     * devices on the normal UI rather than the simplified UI. We want to keep the existing device
     * settings. For all new low-end devices they should get the simplified UI by default.
     */
    public static final String ALLOW_LOW_END_DEVICE_UI = "allow_low_end_device_ui";
    private static final String PREF_WEBSITE_SETTINGS_FILTER = "website_settings_filter";
    private static final String CONTEXTUAL_SEARCH_TAP_TRIGGERED_PROMO_COUNT =
            "contextual_search_tap_triggered_promo_count";
    private static final String CONTEXTUAL_SEARCH_LAST_ANIMATION_TIME =
            "contextual_search_last_animation_time";
    private static final String CONTEXTUAL_SEARCH_CURRENT_WEEK_NUMBER =
            "contextual_search_current_week_number";

    /**
     * Whether Chrome is set as the default browser.
     * Default value is false.
     */
    public static final String CHROME_DEFAULT_BROWSER = "applink.chrome_default_browser";

    /**
     * Deprecated in M70. This value may still exist in the shared preferences file. Do not reuse.
     * TODO(twellington): Remove preference from the file in a future pref cleanup effort.
     */
    @Deprecated
    private static final String CHROME_MODERN_DESIGN_ENABLED_KEY = "chrome_modern_design_enabled";

    /**
     * Whether or not the home page button is force enabled.
     * Default value is false.
     */
    public static final String HOME_PAGE_BUTTON_FORCE_ENABLED_KEY =
            "home_page_button_force_enabled";

    /**
     * Whether or not the homepage tile will be shown.
     * Default value is false.
     */
    public static final String HOMEPAGE_TILE_ENABLED_KEY = "homepage_tile_enabled";

    /**
     * Whether or not the new tab page button is enabled.
     * Default value is false.
     */
    public static final String NTP_BUTTON_ENABLED_KEY = "ntp_button_enabled";

    /**
     * Deprecated in M71. This value may still exist in the shared preferences file. Do not reuse.
     * TODO(twellington): Remove preference from the file in a future pref cleanup effort.
     */
    @Deprecated
    private static final String NTP_BUTTON_VARIANT_KEY = "ntp_button_variant";

    /**
     * Whether or not to inflate the ChromeTabbedActivity toolbar on a background thread async.
     * Default value is false.
     */
    public static final String INFLATE_TOOLBAR_ON_BACKGROUND_THREAD_KEY =
            "inflate_toolbar_on_background_thread";

    /**
     * Whether or not the bottom toolbar is enabled.
     * Default value is false.
     */
    public static final String BOTTOM_TOOLBAR_ENABLED_KEY = "bottom_toolbar_enabled";

    /**
     * Marks that the content suggestions surface has been shown.
     * Default value is false.
     */
    public static final String CONTENT_SUGGESTIONS_SHOWN_KEY = "content_suggestions_shown";

    /**
     * Whether the user dismissed the personalized sign in promo from the Settings.
     * Default value is false.
     */
    public static final String SETTINGS_PERSONALIZED_SIGNIN_PROMO_DISMISSED =
            "settings_personalized_signin_promo_dismissed";
    /**
     * Whether the user dismissed the personalized sign in promo from the new tab page.
     * Default value is false.
     */
    public static final String NTP_SIGNIN_PROMO_DISMISSED =
            "ntp.personalized_signin_promo_dismissed";

    private static final String NTP_SIGNIN_PROMO_SUPPRESSION_PERIOD_START =
            "ntp.signin_promo_suppression_period_start";

    private static final String SUCCESS_UPLOAD_SUFFIX = "_crash_success_upload";
    private static final String FAILURE_UPLOAD_SUFFIX = "_crash_failure_upload";

    /**
     * Whether or not Sole integration is enabled.
     * Default value is true.
     */
    public static final String SOLE_INTEGRATION_ENABLED_KEY = "sole_integration_enabled";

    /**
     * Whether or not command line on non-rooted devices is enabled.
     * Default value is false.
     */
    public static final String COMMAND_LINE_ON_NON_ROOTED_ENABLED_KEY =
            "command_line_on_non_rooted_enabled";

    private static final String VERIFIED_DIGITAL_ASSET_LINKS =
            "verified_digital_asset_links";
    private static final String TRUSTED_WEB_ACTIVITY_DISCLOSURE_ACCEPTED_PACKAGES =
            "trusted_web_activity_disclosure_accepted_packages";

    /**
     * Whether VR assets component should be registered on startup.
     * Default value is false.
     */
    public static final String SHOULD_REGISTER_VR_ASSETS_COMPONENT_ON_STARTUP =
            "should_register_vr_assets_component_on_startup";

    /*
     * Whether the simplified tab switcher is enabled when accessibility mode is enabled. Keep in
     * sync with accessibility_preferences.xml.
     * Default value is true.
     */
    public static final String ACCESSIBILITY_TAB_SWITCHER = "accessibility_tab_switcher";

    /**
     * When the user is shown a badge that the current Android OS version is unsupported, and they
     * tap it to display the menu (which has additional information), we store the current version
     * of Chrome to this preference to ensure we only show the badge once. The value is cleared
     * if the Chrome version later changes.
     */
    public static final String LATEST_UNSUPPORTED_VERSION = "android_os_unsupported_chrome_version";

    /**
     * Deprecated keys for Chrome Home.
     */
    private static final String CHROME_HOME_USER_ENABLED_KEY = "chrome_home_user_enabled";
    private static final String CHROME_HOME_OPT_OUT_SNACKBAR_SHOWN =
            "chrome_home_opt_out_snackbar_shown";
    public static final String CHROME_HOME_INFO_PROMO_SHOWN_KEY = "chrome_home_info_promo_shown";
    public static final String CHROME_HOME_SHARED_PREFERENCES_KEY = "chrome_home_enabled_date";

    private static class LazyHolder {
        static final ChromePreferenceManager INSTANCE = new ChromePreferenceManager();
    }

    private final SharedPreferences mSharedPreferences;

    private ChromePreferenceManager() {
        mSharedPreferences = ContextUtils.getAppSharedPreferences();
    }

    /**
     * Get the static instance of ChromePreferenceManager if exists else create it.
     * @return the ChromePreferenceManager singleton
     */
    public static ChromePreferenceManager getInstance() {
        return LazyHolder.INSTANCE;
    }

    /**
     * @return Number of times of successful crash upload.
     */
    public int getCrashSuccessUploadCount(@ProcessType String process) {
        // Convention to keep all the key in preference lower case.
        return mSharedPreferences.getInt(successUploadKey(process), 0);
    }

    public void setCrashSuccessUploadCount(@ProcessType String process, int count) {
        SharedPreferences.Editor sharedPreferencesEditor;

        sharedPreferencesEditor = mSharedPreferences.edit();
        // Convention to keep all the key in preference lower case.
        sharedPreferencesEditor.putInt(successUploadKey(process), count);
        sharedPreferencesEditor.apply();
    }

    public void incrementCrashSuccessUploadCount(@ProcessType String process) {
        setCrashSuccessUploadCount(process, getCrashSuccessUploadCount(process) + 1);
    }

    private String successUploadKey(@ProcessType String process) {
        return process.toLowerCase(Locale.US) + SUCCESS_UPLOAD_SUFFIX;
    }

    /**
     * @return Number of times of failure crash upload after reaching the max number of tries.
     */
    public int getCrashFailureUploadCount(@ProcessType String process) {
        return mSharedPreferences.getInt(failureUploadKey(process), 0);
    }

    public void setCrashFailureUploadCount(@ProcessType String process, int count) {
        SharedPreferences.Editor sharedPreferencesEditor;

        sharedPreferencesEditor = mSharedPreferences.edit();
        sharedPreferencesEditor.putInt(failureUploadKey(process), count);
        sharedPreferencesEditor.apply();
    }

    public void incrementCrashFailureUploadCount(@ProcessType String process) {
        setCrashFailureUploadCount(process, getCrashFailureUploadCount(process) + 1);
    }

    private String failureUploadKey(@ProcessType String process) {
        return process.toLowerCase(Locale.US) + FAILURE_UPLOAD_SUFFIX;
    }

    /**
     * @return The value for the website settings filter (the one that specifies
     * which sites to show in the list).
     */
    public String getWebsiteSettingsFilterPreference() {
        return mSharedPreferences.getString(
                ChromePreferenceManager.PREF_WEBSITE_SETTINGS_FILTER, "");
    }

    /**
     * Sets the filter value for website settings (which websites to show in the list).
     * @param prefValue The type to restrict the filter to.
     */
    public void setWebsiteSettingsFilterPreference(String prefValue) {
        SharedPreferences.Editor sharedPreferencesEditor = mSharedPreferences.edit();
        sharedPreferencesEditor.putString(
                ChromePreferenceManager.PREF_WEBSITE_SETTINGS_FILTER, prefValue);
        sharedPreferencesEditor.apply();
    }

    /**
     * Returns Chrome major version number when signin promo was last shown, or 0 if version number
     * isn't known.
     */
    public int getSigninPromoLastShownVersion() {
        return mSharedPreferences.getInt(SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION, 0);
    }

    /**
     * Sets Chrome major version number when signin promo was last shown.
     */
    public void setSigninPromoLastShownVersion(int majorVersion) {
        SharedPreferences.Editor editor = mSharedPreferences.edit();
        editor.putInt(SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION, majorVersion).apply();
    }

    /**
     * Returns a set of account names on the device when signin promo was last shown,
     * or null if promo hasn't been shown yet.
     */
    public Set<String> getSigninPromoLastAccountNames() {
        return mSharedPreferences.getStringSet(SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES, null);
    }

    /**
     * Stores a set of account names on the device when signin promo is shown.
     */
    public void setSigninPromoLastAccountNames(Set<String> accountNames) {
        SharedPreferences.Editor editor = mSharedPreferences.edit();
        editor.putStringSet(SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES, accountNames).apply();
    }

    /**
     * @return The last time the search provider icon was animated on tap.
     */
    public long getContextualSearchLastAnimationTime() {
        return mSharedPreferences.getLong(CONTEXTUAL_SEARCH_LAST_ANIMATION_TIME, 0);
    }

    /**
     * Sets the last time the search provider icon was animated on tap.
     * @param time The last time the search provider icon was animated on tap.
     */
    public void setContextualSearchLastAnimationTime(long time) {
        SharedPreferences.Editor ed = mSharedPreferences.edit();
        ed.putLong(CONTEXTUAL_SEARCH_LAST_ANIMATION_TIME, time);
        ed.apply();
    }

    /**
     * @return Number of times the promo was triggered to peek by a tap gesture, or a negative value
     *         if in the disabled state.
     */
    public int getContextualSearchTapTriggeredPromoCount() {
        return mSharedPreferences.getInt(CONTEXTUAL_SEARCH_TAP_TRIGGERED_PROMO_COUNT, 0);
    }

    /**
     * Sets the number of times the promo was triggered to peek by a tap gesture.
     * Use a negative value to record that the counter has been disabled.
     * @param count Number of times the promo was triggered by a tap gesture, or a negative value
     *        to record that the counter has been disabled.
     */
    public void setContextualSearchTapTriggeredPromoCount(int count) {
        writeInt(CONTEXTUAL_SEARCH_TAP_TRIGGERED_PROMO_COUNT, count);
    }

    /**
     * @return The current week number, persisted for weekly CTR recording.
     */
    public int getContextualSearchCurrentWeekNumber() {
        return mSharedPreferences.getInt(CONTEXTUAL_SEARCH_CURRENT_WEEK_NUMBER, 0);
    }

    /**
     * Sets the current week number to persist.  Used for weekly CTR recording.
     * @param weekNumber The week number to store.
     */
    public void setContextualSearchCurrentWeekNumber(int weekNumber) {
        writeInt(CONTEXTUAL_SEARCH_CURRENT_WEEK_NUMBER, weekNumber);
    }

    /**
     * Returns timestamp of the suppression period start if signin promos in the New Tab Page are
     * temporarily suppressed; zero otherwise.
     * @return the epoch time in milliseconds (see {@link System#currentTimeMillis()}).
     */
    public long getNewTabPageSigninPromoSuppressionPeriodStart() {
        return readLong(NTP_SIGNIN_PROMO_SUPPRESSION_PERIOD_START, 0);
    }

    /**
     * Sets timestamp of the suppression period start if signin promos in the New Tab Page are
     * temporarily suppressed.
     * @param timeMillis the epoch time in milliseconds (see {@link System#currentTimeMillis()}).
     */
    public void setNewTabPageSigninPromoSuppressionPeriodStart(long timeMillis) {
        writeLong(NTP_SIGNIN_PROMO_SUPPRESSION_PERIOD_START, timeMillis);
    }

    /**
     * Removes the stored timestamp of the suppression period start when signin promos in the New
     * Tab Page are no longer suppressed.
     */
    public void clearNewTabPageSigninPromoSuppressionPeriodStart() {
        removeKey(NTP_SIGNIN_PROMO_SUPPRESSION_PERIOD_START);
    }

    /**
     * Clean up unused Chrome Home preferences.
     */
    public void clearObsoleteChromeHomePrefs() {
        removeKey(CHROME_HOME_USER_ENABLED_KEY);
        removeKey(CHROME_HOME_INFO_PROMO_SHOWN_KEY);
        removeKey(CHROME_HOME_OPT_OUT_SNACKBAR_SHOWN);
    }

    /**
     * Gets a set of Strings representing digital asset links that have been verified.
     * Set by {@link #setVerifiedDigitalAssetLinks(Set)}.
     */
    public Set<String> getVerifiedDigitalAssetLinks() {
        // From the official docs, modifying the result of a SharedPreferences.getStringSet can
        // cause bad things to happen including exceptions or ruining the data.
        return new HashSet<>(mSharedPreferences.getStringSet(VERIFIED_DIGITAL_ASSET_LINKS,
                Collections.emptySet()));
    }

    /**
     * Sets a set of digital asset links (represented a strings) that have been verified.
     * Can be retrieved by {@link #getVerifiedDigitalAssetLinks()}.
     */
    public void setVerifiedDigitalAssetLinks(Set<String> links) {
        mSharedPreferences.edit().putStringSet(VERIFIED_DIGITAL_ASSET_LINKS, links).apply();
    }

    /** Do not modify the set returned by this method. */
    private Set<String> getTrustedWebActivityDisclosureAcceptedPackages() {
        return mSharedPreferences.getStringSet(
                TRUSTED_WEB_ACTIVITY_DISCLOSURE_ACCEPTED_PACKAGES, Collections.emptySet());
    }

    /**
     * Sets that the user has accepted the Trusted Web Activity "Running in Chrome" disclosure for
     * TWAs launched by the given package.
     */
    public void setUserAcceptedTwaDisclosureForPackage(String packageName) {
        Set<String> packages = new HashSet<>(getTrustedWebActivityDisclosureAcceptedPackages());
        packages.add(packageName);
        mSharedPreferences.edit().putStringSet(
                TRUSTED_WEB_ACTIVITY_DISCLOSURE_ACCEPTED_PACKAGES, packages).apply();
    }

    /**
     * Checks whether the given package was previously passed to
     * {@link #setUserAcceptedTwaDisclosureForPackage(String)}.
     */
    public boolean hasUserAcceptedTwaDisclosureForPackage(String packageName) {
        return getTrustedWebActivityDisclosureAcceptedPackages().contains(packageName);
    }

    /**
     * Writes the given int value to the named shared preference.
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    public void writeInt(String key, int value) {
        SharedPreferences.Editor ed = mSharedPreferences.edit();
        ed.putInt(key, value);
        ed.apply();
    }

    /**
     * Reads the given int value from the named shared preference, defaulting to 0 if not found.
     * @param key The name of the preference to return.
     * @return The value of the preference.
     */
    public int readInt(String key) {
        return mSharedPreferences.getInt(key, 0);
    }

    /**
     * Increments the integer value specified by the given key.  If no initial value is present then
     * an initial value of 0 is assumed and incremented, so a new value of 1 is set.
     * @param key The key specifying which integer value to increment.
     * @return The newly incremented value.
     */
    public int incrementInt(String key) {
        int value = mSharedPreferences.getInt(key, 0);
        writeInt(key, ++value);
        return value;
    }

    /**
     * Writes the given long to the named shared preference.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    private void writeLong(String key, long value) {
        SharedPreferences.Editor ed = mSharedPreferences.edit();
        ed.putLong(key, value);
        ed.apply();
    }

    /**
     * Reads the given long value from the named shared preference.
     *
     * @param key The name of the preference to return.
     * @param defaultValue The default value to return if there's no value stored.
     * @return The value of the preference if stored; defaultValue otherwise.
     */
    private long readLong(String key, long defaultValue) {
        return mSharedPreferences.getLong(key, defaultValue);
    }

    /**
     * Writes the given boolean to the named shared preference.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    public void writeBoolean(String key, boolean value) {
        SharedPreferences.Editor ed = mSharedPreferences.edit();
        ed.putBoolean(key, value);
        ed.apply();
    }

    /**
     * Reads the given boolean value from the named shared preference.
     *
     * @param key The name of the preference to return.
     * @param defaultValue The default value to return if there's no value stored.
     * @return The value of the preference if stored; defaultValue otherwise.
     */
    public boolean readBoolean(String key, boolean defaultValue) {
        return mSharedPreferences.getBoolean(key, defaultValue);
    }

    /**
     * Writes the given string to the named shared preference.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    public void writeString(String key, String value) {
        SharedPreferences.Editor ed = mSharedPreferences.edit();
        ed.putString(key, value);
        ed.apply();
    }

    /**
     * Reads the given String value from the named shared preference.
     *
     * @param key The name of the preference to return.
     * @param defaultValue The default value to return if there's no value stored.
     * @return The value of the preference if stored; defaultValue otherwise.
     */
    public String readString(String key, @Nullable String defaultValue) {
        return mSharedPreferences.getString(key, defaultValue);
    }

    /**
     * Removes the shared preference entry.
     *
     * @param key The key of the preference to remove.
     */
    public void removeKey(String key) {
        SharedPreferences.Editor ed = mSharedPreferences.edit();
        ed.remove(key);
        ed.apply();
    }
}
