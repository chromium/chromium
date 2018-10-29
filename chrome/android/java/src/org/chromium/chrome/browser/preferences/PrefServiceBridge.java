// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.content.SharedPreferences;
import android.support.annotation.Nullable;
import android.util.Log;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.chrome.browser.download.DownloadPromptStatus;
import org.chromium.chrome.browser.preferences.languages.LanguageItem;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.browser.preferences.website.ContentSettingException;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * PrefServiceBridge is a singleton which provides access to some native preferences. Ideally
 * preferences should be grouped with their relevant functionality but this is a grab-bag for other
 * preferences.
 */
public class PrefServiceBridge {
    // These values must match the native enum values in
    // SupervisedUserURLFilter::FilteringBehavior
    public static final int SUPERVISED_USER_FILTERING_ALLOW = 0;
    public static final int SUPERVISED_USER_FILTERING_WARN = 1;
    public static final int SUPERVISED_USER_FILTERING_BLOCK = 2;

    private static final String MIGRATION_PREF_KEY = "PrefMigrationVersion";
    private static final int MIGRATION_CURRENT_VERSION = 4;

    /** The android permissions associated with requesting location. */
    private static final String[] LOCATION_PERMISSIONS = {
            android.Manifest.permission.ACCESS_FINE_LOCATION,
            android.Manifest.permission.ACCESS_COARSE_LOCATION};
    /** The android permissions associated with requesting access to the camera. */
    private static final String[] CAMERA_PERMISSIONS = {android.Manifest.permission.CAMERA};
    /** The android permissions associated with requesting access to the microphone. */
    private static final String[] MICROPHONE_PERMISSIONS = {
            android.Manifest.permission.RECORD_AUDIO};
    /** Signifies there are no permissions associated. */
    private static final String[] EMPTY_PERMISSIONS = {};

    private static final String LOG_TAG = "PrefServiceBridge";

    // Constants related to the Contextual Search preference.
    private static final String CONTEXTUAL_SEARCH_DISABLED = "false";
    private static final String CONTEXTUAL_SEARCH_ENABLED = "true";

    /**
     * Structure that holds all the version information about the current Chrome browser.
     */
    public static class AboutVersionStrings {
        private final String mApplicationVersion;
        private final String mOSVersion;

        private AboutVersionStrings(String applicationVersion, String osVersion) {
            mApplicationVersion = applicationVersion;
            mOSVersion = osVersion;
        }

        public String getApplicationVersion() {
            return mApplicationVersion;
        }

        public String getOSVersion() {
            return mOSVersion;
        }
    }

    @CalledByNative
    private static AboutVersionStrings createAboutVersionStrings(String applicationVersion,
            String osVersion) {
        return new AboutVersionStrings(applicationVersion, osVersion);
    }

    // Singleton constructor. Do not call directly unless for testing purpose.
    @VisibleForTesting
    protected PrefServiceBridge() {}

    private static PrefServiceBridge sInstance;

    /**
     * @return The singleton preferences object.
     */
    public static PrefServiceBridge getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new PrefServiceBridge();

            // Put initialization here to make instantiation in unit tests easier.
            TemplateUrlService.getInstance().load();
        }
        return sInstance;
    }

    /**
     * @return Whether the preferences have been initialized.
     */
    public static boolean isInitialized() {
        return sInstance != null;
    }

    /**
     * @param preference The name of the preference.
     * @return Whether the specified preference is enabled.
     */
    public boolean getBoolean(@Pref int preference) {
        return nativeGetBoolean(preference);
    }

    /**
     * @param preference The name of the preference.
     * @param value The value the specified preference will be set to.
     */
    public void setBoolean(@Pref int preference, boolean value) {
        nativeSetBoolean(preference, value);
    }

    /**
     * Migrates (synchronously) the preferences to the most recent version.
     */
    public void migratePreferences(Context context) {
        SharedPreferences preferences = ContextUtils.getAppSharedPreferences();
        int currentVersion = preferences.getInt(MIGRATION_PREF_KEY, 0);
        if (currentVersion == MIGRATION_CURRENT_VERSION) return;
        if (currentVersion > MIGRATION_CURRENT_VERSION) {
            Log.e(LOG_TAG, "Saved preferences version is newer than supported.  Attempting to "
                    + "run an older version of Chrome without clearing data is unsupported and "
                    + "the results may be unpredictable.");
        }

        if (currentVersion < 1) {
            nativeMigrateJavascriptPreference();
        }
        // Steps 2,3,4 intentionally skipped.
        preferences.edit().putInt(MIGRATION_PREF_KEY, MIGRATION_CURRENT_VERSION).apply();
    }

    /**
     * Returns whether a particular content setting type is enabled.
     * @param contentSettingsType The content setting type to check.
     */
    public boolean isContentSettingEnabled(int contentSettingsType) {
        return nativeIsContentSettingEnabled(contentSettingsType);
    }

    /**
     * @return Whether a particular content setting type is managed by policy.
     * @param contentSettingsType The content setting type to check.
     */
    public boolean isContentSettingManaged(int contentSettingsType) {
        return nativeIsContentSettingManaged(contentSettingsType);
    }

    /**
     * Sets a default value for content setting type.
     * @param contentSettingsType The content setting type to check.
     * @param enabled Whether the default value should be disabled or enabled.
     */
    public void setContentSettingEnabled(int contentSettingsType, boolean enabled) {
        nativeSetContentSettingEnabled(contentSettingsType, enabled);
    }

    /**
     * Returns all the currently saved exceptions for a given content settings type.
     * @param contentSettingsType The type to fetch exceptions for.
     */
    public List<ContentSettingException> getContentSettingsExceptions(int contentSettingsType) {
        List<ContentSettingException> list = new ArrayList<ContentSettingException>();
        nativeGetContentSettingsExceptions(contentSettingsType, list);
        return list;
    }

    @CalledByNative
    private static void addContentSettingExceptionToList(
            ArrayList<ContentSettingException> list,
            int contentSettingsType,
            String pattern,
            int contentSetting,
            String source) {
        ContentSettingException exception = new ContentSettingException(
                contentSettingsType, pattern, ContentSetting.fromInt(contentSetting), source);
        list.add(exception);
    }

    @CalledByNative
    private static void addNewLanguageItemToList(List<LanguageItem> list, String code,
            String displayName, String nativeDisplayName, boolean supportTranslate) {
        list.add(new LanguageItem(code, displayName, nativeDisplayName, supportTranslate));
    }

    @CalledByNative
    private static void copyStringArrayToList(List<String> list, String[] source) {
        list.addAll(Arrays.asList(source));
    }

    /**
     * Return the list of android permission strings for a given {@link ContentSettingsType}.  If
     * there is no permissions associated with the content setting, then an empty array is returned.
     *
     * @param contentSettingType The content setting to get the android permission for.
     * @return The android permissions for the given content setting.
     */
    @CalledByNative
    public static String[] getAndroidPermissionsForContentSetting(int contentSettingType) {
        switch (contentSettingType) {
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_GEOLOCATION:
                return Arrays.copyOf(LOCATION_PERMISSIONS, LOCATION_PERMISSIONS.length);
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
                return Arrays.copyOf(MICROPHONE_PERMISSIONS, MICROPHONE_PERMISSIONS.length);
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
                return Arrays.copyOf(CAMERA_PERMISSIONS, CAMERA_PERMISSIONS.length);
            default:
                return EMPTY_PERMISSIONS;
        }
    }

    /**
     * @return Whether cookies acceptance is modifiable by the user
     */
    public boolean isAcceptCookiesUserModifiable() {
        return nativeGetAcceptCookiesUserModifiable();
    }

    /**
     * @return Whether cookies acceptance is configured by the user's custodian
     * (for supervised users).
     */
    public boolean isAcceptCookiesManagedByCustodian() {
        return nativeGetAcceptCookiesManagedByCustodian();
    }

    public boolean isBlockThirdPartyCookiesEnabled() {
        return nativeGetBlockThirdPartyCookiesEnabled();
    }

    /**
     * @return Whether third-party cookie blocking is configured by policy
     */
    public boolean isBlockThirdPartyCookiesManaged() {
        return nativeGetBlockThirdPartyCookiesManaged();
    }

    public boolean isRememberPasswordsEnabled() {
        return nativeGetRememberPasswordsEnabled();
    }

    public boolean isPasswordManagerAutoSigninEnabled() {
        return nativeGetPasswordManagerAutoSigninEnabled();
    }

    /**
     * @return Whether password storage is configured by policy
     */
    public boolean isRememberPasswordsManaged() {
        return nativeGetRememberPasswordsManaged();
    }

    public boolean isPasswordManagerAutoSigninManaged() {
        return nativeGetPasswordManagerAutoSigninManaged();
    }

    /**
     * @return Whether vibration is enabled for notifications.
     */
    public boolean isNotificationsVibrateEnabled() {
        return nativeGetNotificationsVibrateEnabled();
    }

    /**
     * @return Whether geolocation information can be shared with content.
     */
    public boolean isAllowLocationEnabled() {
        return nativeGetAllowLocationEnabled();
    }

    /**
     * @return Whether geolocation information access is set to be shared with all sites, by policy.
     */
    public boolean isLocationAllowedByPolicy() {
        return nativeGetLocationAllowedByPolicy();
    }

    /**
     * @return Whether the location preference is modifiable by the user.
     */
    public boolean isAllowLocationUserModifiable() {
        return nativeGetAllowLocationUserModifiable();
    }

    /**
     * @return Whether the location preference is
     * being managed by the custodian of the supervised account.
     */
    public boolean isAllowLocationManagedByCustodian() {
        return nativeGetAllowLocationManagedByCustodian();
    }

    /**
     * @return Whether Do Not Track is enabled
     */
    public boolean isDoNotTrackEnabled() {
        return nativeGetDoNotTrackEnabled();
    }

    public boolean getPasswordEchoEnabled() {
        return nativeGetPasswordEchoEnabled();
    }

    /**
     * @return Whether EULA has been accepted by the user.
     */
    public boolean isFirstRunEulaAccepted() {
        return nativeGetFirstRunEulaAccepted();
    }

    /**
     * @return Whether JavaScript is managed by policy.
     */
    public boolean javaScriptManaged() {
        return isContentSettingManaged(ContentSettingsType.CONTENT_SETTINGS_TYPE_JAVASCRIPT);
    }

    /**
     * @return true if background sync is managed by policy.
     */
    public boolean isBackgroundSyncManaged() {
        return isContentSettingManaged(ContentSettingsType.CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC);
    }

    /**
     * @return true if automatic downloads is managed by policy.
     */
    public boolean isAutomaticDownloadsManaged() {
        return isContentSettingManaged(
                ContentSettingsType.CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS);
    }

    /**
     * Sets the preference that controls translate
     */
    public void setTranslateEnabled(boolean enabled) {
        nativeSetTranslateEnabled(enabled);
    }

    /**
     * Sets the preference that signals when the user has accepted the EULA.
     */
    public void setEulaAccepted() {
        nativeSetEulaAccepted();
    }

    /**
     * Resets translate defaults if needed
     */
    public void resetTranslateDefaults() {
        nativeResetTranslateDefaults();
    }

    /**
     * @return the last account id associated with sync.
     */
    public String getSyncLastAccountId() {
        return nativeGetSyncLastAccountId();
    }

    /**
     * @return the last account username associated with sync.
     */
    public String getSyncLastAccountName() {
        return nativeGetSyncLastAccountName();
    }

    /**
     * @return Whether Search Suggest is enabled.
     */
    public boolean isSearchSuggestEnabled() {
        return nativeGetSearchSuggestEnabled();
    }

    /**
     * Sets whether search suggest should be enabled.
     */
    public void setSearchSuggestEnabled(boolean enabled) {
        nativeSetSearchSuggestEnabled(enabled);
    }

    /**
     * @return Whether Search Suggest is configured by policy.
     */
    public boolean isSearchSuggestManaged() {
        return nativeGetSearchSuggestManaged();
    }

    /**
     * @return the Contextual Search preference.
     */
    public String getContextualSearchPreference() {
        return nativeGetContextualSearchPreference();
    }

    /**
     * Sets the Contextual Search preference.
     * @param prefValue one of "", CONTEXTUAL_SEARCH_ENABLED or CONTEXTUAL_SEARCH_DISABLED.
     */
    public void setContextualSearchPreference(String prefValue) {
        nativeSetContextualSearchPreference(prefValue);
    }

    /**
     * @return Whether the Contextual Search feature was disabled by the user explicitly.
     */
    public boolean isContextualSearchDisabled() {
        return getContextualSearchPreference().equals(CONTEXTUAL_SEARCH_DISABLED);
    }

    /**
     * @return Whether the Contextual Search feature is disabled by policy.
     */
    public boolean isContextualSearchDisabledByPolicy() {
        return nativeGetContextualSearchPreferenceIsManaged()
                && isContextualSearchDisabled();
    }

    /**
     * @return Whether the Contextual Search feature is uninitialized (preference unset by the
     *         user).
     */
    public boolean isContextualSearchUninitialized() {
        return getContextualSearchPreference().isEmpty();
    }

    /**
     * @param enabled Whether Contextual Search should be enabled.
     */
    public void setContextualSearchState(boolean enabled) {
        setContextualSearchPreference(enabled
                ? CONTEXTUAL_SEARCH_ENABLED : CONTEXTUAL_SEARCH_DISABLED);
    }

    /**
     * @return Whether Safe Browsing Extended Reporting is currently enabled.
     */
    public boolean isSafeBrowsingExtendedReportingEnabled() {
        return nativeGetSafeBrowsingExtendedReportingEnabled();
    }

    /**
     * @param enabled Whether Safe Browsing Extended Reporting should be enabled.
     */
    public void setSafeBrowsingExtendedReportingEnabled(boolean enabled) {
        nativeSetSafeBrowsingExtendedReportingEnabled(enabled);
    }

    /**
     * @return Whether Safe Browsing Extended Reporting is managed
     */
    public boolean isSafeBrowsingExtendedReportingManaged() {
        return nativeGetSafeBrowsingExtendedReportingManaged();
    }

    /**
     * @return Whether Safe Browsing is currently enabled.
     */
    public boolean isSafeBrowsingEnabled() {
        return nativeGetSafeBrowsingEnabled();
    }

    /**
     * @param enabled Whether Safe Browsing should be enabled.
     */
    public void setSafeBrowsingEnabled(boolean enabled) {
        nativeSetSafeBrowsingEnabled(enabled);
    }

    /**
     * @return Whether Safe Browsing is managed
     */
    public boolean isSafeBrowsingManaged() {
        return nativeGetSafeBrowsingManaged();
    }

    /**
     * @return Whether there is a user set value for kNetworkPredictionOptions.  This should only be
     * used for preference migration. See http://crbug.com/334602
     */
    public boolean obsoleteNetworkPredictionOptionsHasUserSetting() {
        return nativeObsoleteNetworkPredictionOptionsHasUserSetting();
    }

    /**
     * @return Network predictions preference.
     */
    public boolean getNetworkPredictionEnabled() {
        return nativeGetNetworkPredictionEnabled();
    }

    /**
     * Sets network predictions preference.
     */
    public void setNetworkPredictionEnabled(boolean enabled) {
        nativeSetNetworkPredictionEnabled(enabled);
    }

    /**
     * @return Whether Network Predictions is configured by policy.
     */
    public boolean isNetworkPredictionManaged() {
        return nativeGetNetworkPredictionManaged();
    }

    /**
     * Checks whether network predictions are allowed given preferences and current network
     * connection type.
     * @return Whether network predictions are allowed.
     */
    public boolean canPrefetchAndPrerender() {
        return nativeCanPrefetchAndPrerender();
    }

    /**
     * @return Whether the web service to resolve navigation error is enabled.
     */
    public boolean isResolveNavigationErrorEnabled() {
        return nativeGetResolveNavigationErrorEnabled();
    }

    /**
     * @return Whether the web service to resolve navigation error is configured by policy.
     */
    public boolean isResolveNavigationErrorManaged() {
        return nativeGetResolveNavigationErrorManaged();
    }

    /**
     * @return true if translate is enabled, false otherwise.
     */
    public boolean isTranslateEnabled() {
        return nativeGetTranslateEnabled();
    }

    /**
     * @return Whether translate is configured by policy
     */
    public boolean isTranslateManaged() {
        return nativeGetTranslateManaged();
    }

    /**
     * Sets whether the web service to resolve navigation error should be enabled.
     */
    public void setResolveNavigationErrorEnabled(boolean enabled) {
        nativeSetResolveNavigationErrorEnabled(enabled);
    }

    /**
     * Checks the state of deletion preference for a certain browsing data type.
     * @param dataType The requested browsing data type (from the shared enum
     *      {@link org.chromium.chrome.browser.browsing_data.BrowsingDataType}).
     * @param clearBrowsingDataTab Indicates if this is a checkbox on the default, basic or advanced
     *      tab to apply the right preference.
     * @return The state of the corresponding deletion preference.
     */
    public boolean getBrowsingDataDeletionPreference(int dataType, int clearBrowsingDataTab) {
        return nativeGetBrowsingDataDeletionPreference(dataType, clearBrowsingDataTab);
    }

    /**
     * Sets the state of deletion preference for a certain browsing data type.
     * @param dataType The requested browsing data type (from the shared enum
     *      {@link org.chromium.chrome.browser.browsing_data.BrowsingDataType}).
     * @param clearBrowsingDataTab Indicates if this is a checkbox on the default, basic or advanced
     *      tab to apply the right preference.
     * @param value The state to be set.
     */
    public void setBrowsingDataDeletionPreference(
            int dataType, int clearBrowsingDataTab, boolean value) {
        nativeSetBrowsingDataDeletionPreference(dataType, clearBrowsingDataTab, value);
    }

    /**
     * Gets the time period for which browsing data will be deleted.
     * @param clearBrowsingDataTab Indicates if this is a timeperiod on the default, basic or
     *      advanced tab to apply the right preference.
     * @return The currently selected browsing data deletion time period (from the shared enum
     *      {@link org.chromium.chrome.browser.browsing_data.TimePeriod}).
     */
    public int getBrowsingDataDeletionTimePeriod(int clearBrowsingDataTab) {
        return nativeGetBrowsingDataDeletionTimePeriod(clearBrowsingDataTab);
    }

    /**
     * Sets the time period for which browsing data will be deleted.
     * @param clearBrowsingDataTab Indicates if this is a timeperiod on the default, basic or
     *      advanced tab to apply the right preference.
     * @param timePeriod The selected browsing data deletion time period (from the shared enum
     *      {@link org.chromium.chrome.browser.browsing_data.TimePeriod}).
     */
    public void setBrowsingDataDeletionTimePeriod(int clearBrowsingDataTab, int timePeriod) {
        nativeSetBrowsingDataDeletionTimePeriod(clearBrowsingDataTab, timePeriod);
    }

    /**
     * @return The index of the tab last visited by the user in the CBD dialog.
     *         Index 0 is for the basic tab, 1 is the advanced tab.
     */
    public int getLastSelectedClearBrowsingDataTab() {
        return nativeGetLastClearBrowsingDataTab();
    }

    /**
     * Set the index of the tab last visited by the user.
     * @param tabIndex The last visited tab index, 0 for basic, 1 for advanced.
     */
    public void setLastSelectedClearBrowsingDataTab(int tabIndex) {
        nativeSetLastClearBrowsingDataTab(tabIndex);
    }

    public void setBlockThirdPartyCookiesEnabled(boolean enabled) {
        nativeSetBlockThirdPartyCookiesEnabled(enabled);
    }

    public void setDoNotTrackEnabled(boolean enabled) {
        nativeSetDoNotTrackEnabled(enabled);
    }

    public void setRememberPasswordsEnabled(boolean allow) {
        nativeSetRememberPasswordsEnabled(allow);
    }

    public void setPasswordManagerAutoSigninEnabled(boolean enabled) {
        nativeSetPasswordManagerAutoSigninEnabled(enabled);
    }

    public void setNotificationsVibrateEnabled(boolean enabled) {
        nativeSetNotificationsVibrateEnabled(enabled);
    }

    public void setPasswordEchoEnabled(boolean enabled) {
        nativeSetPasswordEchoEnabled(enabled);
    }

    /**
     * @return Whether the setting to allow popups is configured by policy
     */
    public boolean isPopupsManaged() {
        return isContentSettingManaged(ContentSettingsType.CONTENT_SETTINGS_TYPE_POPUPS);
    }

    /**
     * Whether the setting type requires tri-state (Allowed/Ask/Blocked) setting.
     */
    public boolean requiresTriStateContentSetting(int contentSettingsType) {
        switch (contentSettingsType) {
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
                return true;
            default:
                return false;
        }
    }

    /**
     * Sets the preferences on whether to enable/disable given setting.
     */
    public void setCategoryEnabled(int contentSettingsType, boolean allow) {
        assert !requiresTriStateContentSetting(contentSettingsType);

        switch (contentSettingsType) {
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_ADS:
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_JAVASCRIPT:
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_POPUPS:
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_USB_GUARD:
                setContentSettingEnabled(contentSettingsType, allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS:
                nativeSetAutomaticDownloadsEnabled(allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_AUTOPLAY:
                nativeSetAutoplayEnabled(allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC:
                nativeSetBackgroundSyncEnabled(allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_CLIPBOARD_READ:
                nativeSetClipboardEnabled(allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_COOKIES:
                nativeSetAllowCookiesEnabled(allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_GEOLOCATION:
                nativeSetAllowLocationEnabled(allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
                nativeSetCameraEnabled(allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
                nativeSetMicEnabled(allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
                nativeSetNotificationsEnabled(allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_SENSORS:
                nativeSetSensorsEnabled(allow);
                break;
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_SOUND:
                nativeSetSoundEnabled(allow);
                break;
            default:
                assert false;
        }
    }

    public boolean isCategoryEnabled(int contentSettingsType) {
        assert !requiresTriStateContentSetting(contentSettingsType);

        switch (contentSettingsType) {
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_ADS:
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_CLIPBOARD_READ:
            // Returns true if JavaScript is enabled. It may return the temporary value set by
            // {@link #setJavaScriptEnabled}. The default is true.
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_JAVASCRIPT:
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_POPUPS:
            // Returns true if websites are allowed to request permission to access USB devices.
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_USB_GUARD:
                return isContentSettingEnabled(contentSettingsType);
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS:
                return nativeGetAutomaticDownloadsEnabled();
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_AUTOPLAY:
                return nativeGetAutoplayEnabled();
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC:
                return nativeGetBackgroundSyncEnabled();
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_COOKIES:
                return nativeGetAcceptCookiesEnabled();
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
                return nativeGetCameraEnabled();
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
                return nativeGetMicEnabled();
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
                return nativeGetNotificationsEnabled();
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_SENSORS:
                return nativeGetSensorsEnabled();
            case ContentSettingsType.CONTENT_SETTINGS_TYPE_SOUND:
                return nativeGetSoundEnabled();
            default:
                assert false;
                return false;
        }
    }

    /**
     * Gets the ContentSetting for a settings type. Should only be used for more
     * complex settings where a binary on/off value is not sufficient.
     * Otherwise, use isCategoryEnabled() above.
     * @param contentSettingsType The settings type to get setting for.
     * @return The ContentSetting for |contentSettingsType|.
     */
    public int getContentSetting(int contentSettingsType) {
        return nativeGetContentSetting(contentSettingsType);
    }

    /**
     * @param setting New ContentSetting to set for |contentSettingsType|.
     */
    public void setContentSetting(int contentSettingsType, int setting) {
        nativeSetContentSetting(contentSettingsType, setting);
    }

    /**
     * @return Whether the camera/microphone permission is managed
     * by the custodian of the supervised account.
     */
    public boolean isCameraManagedByCustodian() {
        return nativeGetCameraManagedByCustodian();
    }

    /**
     * @return Whether the camera permission is editable by the user.
     */
    public boolean isCameraUserModifiable() {
        return nativeGetCameraUserModifiable();
    }

    /**
     * Sets the preferences on whether to enable/disable microphone.
     */
    public void setMicEnabled(boolean enabled) {
        nativeSetMicEnabled(enabled);
    }

    /**
     * @return Whether the microphone permission is managed by the custodian of
     * the supervised account.
     */
    public boolean isMicManagedByCustodian() {
        return nativeGetMicManagedByCustodian();
    }

    /**
     * @return Whether the microphone permission is editable by the user.
     */
    public boolean isMicUserModifiable() {
        return nativeGetMicUserModifiable();
    }

    /**
     * @return true if incognito mode is enabled.
     */
    public boolean isIncognitoModeEnabled() {
        return nativeGetIncognitoModeEnabled();
    }

    /**
     * @return true if incognito mode is managed by policy.
     */
    public boolean isIncognitoModeManaged() {
        return nativeGetIncognitoModeManaged();
    }

    /**
     * @return Whether printing is enabled.
     */
    public boolean isPrintingEnabled() {
        return nativeGetPrintingEnabled();
    }

    /**
     * @return Whether printing is managed by policy.
     */
    public boolean isPrintingManaged() {
        return nativeGetPrintingManaged();
    }

    /**
     * Get all the version strings from native.
     * @return AboutVersionStrings about version strings.
     */
    public AboutVersionStrings getAboutVersionStrings() {
        return nativeGetAboutVersionStrings();
    }

    /**
     * Reset accept-languages to its default value.
     *
     * @param defaultLocale A fall-back value such as en_US, de_DE, zh_CN, etc.
     */
    public void resetAcceptLanguages(String defaultLocale) {
        nativeResetAcceptLanguages(defaultLocale);
    }

    /**
     * @return Whether SafeSites for supervised users is enabled.
     */
    public boolean isSupervisedUserSafeSitesEnabled() {
        return nativeGetSupervisedUserSafeSitesEnabled();
    }

    /**
     * @return the default supervised user filtering behavior
     */
    public int getDefaultSupervisedUserFilteringBehavior() {
        return nativeGetDefaultSupervisedUserFilteringBehavior();
    }

    public String getSupervisedUserCustodianName() {
        return nativeGetSupervisedUserCustodianName();
    }

    public String getSupervisedUserCustodianEmail() {
        return nativeGetSupervisedUserCustodianEmail();
    }

    public String getSupervisedUserCustodianProfileImageURL() {
        return nativeGetSupervisedUserCustodianProfileImageURL();
    }

    public String getSupervisedUserSecondCustodianName() {
        return nativeGetSupervisedUserSecondCustodianName();
    }

    public String getSupervisedUserSecondCustodianEmail() {
        return nativeGetSupervisedUserSecondCustodianEmail();
    }

    public String getSupervisedUserSecondCustodianProfileImageURL() {
        return nativeGetSupervisedUserSecondCustodianProfileImageURL();
    }

    /**
     * @return A sorted list of LanguageItems representing the Chrome accept languages with details.
     *         Languages that are not supported on Android have been filtered out.
     */
    public List<LanguageItem> getChromeLanguageList() {
        List<LanguageItem> list = new ArrayList<>();
        nativeGetChromeAcceptLanguages(list);
        return list;
    }

    /**
     * @return A sorted list of accept language codes for the current user.
     *         Note that for the signed-in user, the list might contain some language codes from
     *         other platforms but not supported on Android.
     */
    public List<String> getUserLanguageCodes() {
        List<String> list = new ArrayList<>();
        nativeGetUserAcceptLanguages(list);
        return list;
    }

    /**
     * Update accept language for the current user.
     *
     * @param languageCode A valid language code to update.
     * @param add Whether this is an "add" operation or "delete" operation.
     */
    public void updateUserAcceptLanguages(String languageCode, boolean add) {
        nativeUpdateUserAcceptLanguages(languageCode, add);
    }

    /**
     * Move a language to the given postion of the user's accept language.
     *
     * @param languageCode A valid language code to set.
     * @param offset The offset from the original position of the language.
     */
    public void moveAcceptLanguage(String languageCode, int offset) {
        nativeMoveAcceptLanguage(languageCode, offset);
    }

    /**
     * @param languageCode A valid language code to check.
     * @return Whether the given language is blocked by the user.
     */
    public boolean isBlockedLanguage(String languageCode) {
        return nativeIsBlockedLanguage(languageCode);
    }

    /**
     * Sets the blocked state of a given language.
     *
     * @param languageCode A valid language code to change.
     * @param blocked Whether to set language blocked.
     */
    public void setLanguageBlockedState(String languageCode, boolean blocked) {
        nativeSetLanguageBlockedState(languageCode, blocked);
    }

    private native boolean nativeIsContentSettingEnabled(int contentSettingType);
    private native boolean nativeIsContentSettingManaged(int contentSettingType);
    private native void nativeSetContentSettingEnabled(int contentSettingType, boolean allow);
    private native void nativeGetContentSettingsExceptions(
            int contentSettingsType, List<ContentSettingException> list);
    public native void nativeSetContentSettingForPattern(
            int contentSettingType, String pattern, int setting);
    public native int nativeGetContentSetting(int contentSettingType);
    public native void nativeSetContentSetting(int contentSettingType, int setting);

    /**
      * @return Whether usage and crash reporting pref is enabled.
      */
    public boolean isMetricsReportingEnabled() {
        return nativeIsMetricsReportingEnabled();
    }

    /**
     * Sets whether the usage and crash reporting pref should be enabled.
     */
    public void setMetricsReportingEnabled(boolean enabled) {
        nativeSetMetricsReportingEnabled(enabled);
    }

    /**
     * @return Whether usage and crash report pref is managed.
     */
    public boolean isMetricsReportingManaged() {
        return nativeIsMetricsReportingManaged();
    }

    /**
     * @param clicked Whether the update menu item was clicked. The preference is stored to
     *                facilitate logging whether Chrome was updated after a click on the menu item.
     */
    public void setClickedUpdateMenuItem(boolean clicked) {
        nativeSetClickedUpdateMenuItem(clicked);
    }

    /**
     * @return Whether the update menu item was clicked.
     */
    public boolean getClickedUpdateMenuItem() {
        return nativeGetClickedUpdateMenuItem();
    }

    /**
     * @param version The latest version of Chrome available when the update menu item
     *                was clicked.
     */
    public void setLatestVersionWhenClickedUpdateMenuItem(String version) {
        nativeSetLatestVersionWhenClickedUpdateMenuItem(version);
    }

    /**
     * @return The latest version of Chrome available when the update menu item was clicked.
     */
    public String getLatestVersionWhenClickedUpdateMenuItem() {
        return nativeGetLatestVersionWhenClickedUpdateMenuItem();
    }

    @VisibleForTesting
    public void setSupervisedUserId(String supervisedUserId) {
        nativeSetSupervisedUserId(supervisedUserId);
    }

    /**
     * @return The stored download default directory.
     */
    public String getDownloadDefaultDirectory() {
        return nativeGetDownloadDefaultDirectory();
    }

    /**
     * @param directory New directory to set as the download default directory.
     */
    public void setDownloadAndSaveFileDefaultDirectory(String directory) {
        nativeSetDownloadAndSaveFileDefaultDirectory(directory);
    }

    /**
     * @return The status of prompt for download pref, defined by {@link DownloadPromptStatus}.
     */
    @DownloadPromptStatus
    public int getPromptForDownloadAndroid() {
        return nativeGetPromptForDownloadAndroid();
    }

    /**
     * @param status New status to update the prompt for download preference.
     */
    public void setPromptForDownloadAndroid(@DownloadPromptStatus int status) {
        nativeSetPromptForDownloadAndroid(status);
    }

    /**
     * @return Whether the explicit language prompt was shown at least once.
     */
    public boolean getExplicitLanguageAskPromptShown() {
        return nativeGetExplicitLanguageAskPromptShown();
    }

    /**
     * @param shown The value to set the underlying pref to: whether the prompt
     * was shown to the user at least once.
     */
    public void setExplicitLanguageAskPromptShown(boolean shown) {
        nativeSetExplicitLanguageAskPromptShown(shown);
    }

    @VisibleForTesting
    public static void setInstanceForTesting(@Nullable PrefServiceBridge instanceForTesting) {
        sInstance = instanceForTesting;
    }

    private native boolean nativeGetBoolean(int preference);
    private native void nativeSetBoolean(int preference, boolean value);
    private native boolean nativeGetAcceptCookiesEnabled();
    private native boolean nativeGetAcceptCookiesUserModifiable();
    private native boolean nativeGetAcceptCookiesManagedByCustodian();
    private native boolean nativeGetAutomaticDownloadsEnabled();
    private native boolean nativeGetAutoplayEnabled();
    private native boolean nativeGetBackgroundSyncEnabled();
    private native boolean nativeGetBlockThirdPartyCookiesEnabled();
    private native boolean nativeGetBlockThirdPartyCookiesManaged();
    private native boolean nativeGetRememberPasswordsEnabled();
    private native boolean nativeGetPasswordManagerAutoSigninEnabled();
    private native boolean nativeGetRememberPasswordsManaged();
    private native boolean nativeGetPasswordManagerAutoSigninManaged();
    private native boolean nativeGetAllowLocationUserModifiable();
    private native boolean nativeGetLocationAllowedByPolicy();
    private native boolean nativeGetAllowLocationManagedByCustodian();
    private native boolean nativeGetDoNotTrackEnabled();
    private native boolean nativeGetPasswordEchoEnabled();
    private native boolean nativeGetFirstRunEulaAccepted();
    private native boolean nativeGetCameraEnabled();
    private native void nativeSetCameraEnabled(boolean enabled);
    private native boolean nativeGetCameraUserModifiable();
    private native boolean nativeGetCameraManagedByCustodian();
    private native boolean nativeGetMicEnabled();
    private native void nativeSetMicEnabled(boolean enabled);
    private native boolean nativeGetMicUserModifiable();
    private native boolean nativeGetMicManagedByCustodian();
    private native boolean nativeGetTranslateEnabled();
    private native boolean nativeGetTranslateManaged();
    private native boolean nativeGetResolveNavigationErrorEnabled();
    private native boolean nativeGetResolveNavigationErrorManaged();
    private native boolean nativeGetIncognitoModeEnabled();
    private native boolean nativeGetIncognitoModeManaged();
    private native boolean nativeGetPrintingEnabled();
    private native boolean nativeGetPrintingManaged();
    private native boolean nativeGetSensorsEnabled();
    private native boolean nativeGetSoundEnabled();
    private native boolean nativeGetSupervisedUserSafeSitesEnabled();
    private native void nativeSetTranslateEnabled(boolean enabled);
    private native void nativeResetTranslateDefaults();
    private native void nativeMigrateJavascriptPreference();
    private native boolean nativeGetBrowsingDataDeletionPreference(
            int dataType, int clearBrowsingDataTab);
    private native void nativeSetBrowsingDataDeletionPreference(
            int dataType, int clearBrowsingDataTab, boolean value);
    private native int nativeGetBrowsingDataDeletionTimePeriod(int clearBrowsingDataTab);
    private native void nativeSetBrowsingDataDeletionTimePeriod(
            int clearBrowsingDataTab, int timePeriod);
    private native int nativeGetLastClearBrowsingDataTab();
    private native void nativeSetLastClearBrowsingDataTab(int lastTab);
    private native void nativeSetAutomaticDownloadsEnabled(boolean enabled);
    private native void nativeSetAutoplayEnabled(boolean enabled);
    private native void nativeSetAllowCookiesEnabled(boolean enabled);
    private native void nativeSetBackgroundSyncEnabled(boolean enabled);
    private native void nativeSetBlockThirdPartyCookiesEnabled(boolean enabled);
    private native void nativeSetClipboardEnabled(boolean enabled);
    private native void nativeSetDoNotTrackEnabled(boolean enabled);
    private native void nativeSetRememberPasswordsEnabled(boolean allow);
    private native void nativeSetPasswordManagerAutoSigninEnabled(boolean enabled);
    private native boolean nativeGetAllowLocationEnabled();
    private native boolean nativeGetNotificationsEnabled();
    private native boolean nativeGetNotificationsVibrateEnabled();
    private native void nativeSetAllowLocationEnabled(boolean enabled);
    private native void nativeSetNotificationsEnabled(boolean enabled);
    private native void nativeSetNotificationsVibrateEnabled(boolean enabled);
    private native void nativeSetPasswordEchoEnabled(boolean enabled);
    private native void nativeSetSensorsEnabled(boolean enabled);
    private native void nativeSetSoundEnabled(boolean enabled);
    private native boolean nativeCanPrefetchAndPrerender();
    private native AboutVersionStrings nativeGetAboutVersionStrings();
    private native void nativeSetContextualSearchPreference(String preference);
    private native String nativeGetContextualSearchPreference();
    private native boolean nativeGetContextualSearchPreferenceIsManaged();
    private native boolean nativeGetSearchSuggestEnabled();
    private native void nativeSetSearchSuggestEnabled(boolean enabled);
    private native boolean nativeGetSearchSuggestManaged();
    private native boolean nativeGetSafeBrowsingExtendedReportingEnabled();
    private native void nativeSetSafeBrowsingExtendedReportingEnabled(boolean enabled);
    private native boolean nativeGetSafeBrowsingExtendedReportingManaged();
    private native boolean nativeGetSafeBrowsingEnabled();
    private native void nativeSetSafeBrowsingEnabled(boolean enabled);
    private native boolean nativeGetSafeBrowsingManaged();
    private native boolean nativeGetNetworkPredictionManaged();
    private native boolean nativeObsoleteNetworkPredictionOptionsHasUserSetting();
    private native boolean nativeGetNetworkPredictionEnabled();
    private native void nativeSetNetworkPredictionEnabled(boolean enabled);
    private native void nativeSetResolveNavigationErrorEnabled(boolean enabled);
    private native void nativeSetEulaAccepted();
    private native void nativeResetAcceptLanguages(String defaultLocale);
    private native String nativeGetSyncLastAccountId();
    private native String nativeGetSyncLastAccountName();
    private native String nativeGetSupervisedUserCustodianName();
    private native String nativeGetSupervisedUserCustodianEmail();
    private native String nativeGetSupervisedUserCustodianProfileImageURL();
    private native int nativeGetDefaultSupervisedUserFilteringBehavior();
    private native String nativeGetSupervisedUserSecondCustodianName();
    private native String nativeGetSupervisedUserSecondCustodianEmail();
    private native String nativeGetSupervisedUserSecondCustodianProfileImageURL();
    private native boolean nativeIsMetricsReportingEnabled();
    private native void nativeSetMetricsReportingEnabled(boolean enabled);
    private native boolean nativeIsMetricsReportingManaged();
    private native void nativeSetClickedUpdateMenuItem(boolean clicked);
    private native boolean nativeGetClickedUpdateMenuItem();
    private native void nativeSetLatestVersionWhenClickedUpdateMenuItem(String version);
    private native String nativeGetLatestVersionWhenClickedUpdateMenuItem();
    private native void nativeSetSupervisedUserId(String supervisedUserId);
    private native void nativeGetChromeAcceptLanguages(List<LanguageItem> list);
    private native void nativeGetUserAcceptLanguages(List<String> list);
    private native void nativeUpdateUserAcceptLanguages(String language, boolean add);
    private native void nativeMoveAcceptLanguage(String language, int offset);
    private native boolean nativeIsBlockedLanguage(String language);
    private native void nativeSetLanguageBlockedState(String language, boolean blocked);
    private native String nativeGetDownloadDefaultDirectory();
    private native void nativeSetDownloadAndSaveFileDefaultDirectory(String directory);
    private native int nativeGetPromptForDownloadAndroid();
    private native void nativeSetPromptForDownloadAndroid(int status);
    private native boolean nativeGetExplicitLanguageAskPromptShown();
    private native void nativeSetExplicitLanguageAskPromptShown(boolean shown);
}
