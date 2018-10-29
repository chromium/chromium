// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.os.Build;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceFragment;
import android.widget.ListView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.LocationSettings;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferenceUtils;
import org.chromium.chrome.browser.preferences.website.SiteSettingsCategory.Type;

import java.util.ArrayList;
import java.util.List;

/**
 * The main Site Settings screen, which shows all the site settings categories: All sites, Location,
 * Microphone, etc. By clicking into one of these categories, the user can see or and modify
 * permissions that have been granted to websites, as well as enable or disable permissions
 * browser-wide.
 *
 * Depending on version and which experiment is running, this class also handles showing the Media
 * sub-menu, which contains Autoplay and Protected Content. To avoid the Media sub-menu having only
 * one sub-item, when either Autoplay or Protected Content should not be visible the other is shown
 * in the main setting instead (as opposed to under Media).
 */
public class SiteSettingsPreferences extends PreferenceFragment
        implements OnPreferenceClickListener {
    // The keys for each category shown on the Site Settings page
    // are defined in the SiteSettingsCategory, additional keys
    // are listed here.
    static final String MEDIA_KEY = "media";
    static final String TRANSLATE_KEY = "translate";

    // Whether the Protected Content menu is available for display.
    boolean mProtectedContentMenuAvailable;

    // Whether this class is handling showing the Media sub-menu (and not the main menu).
    boolean mMediaSubMenu;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        PreferenceUtils.addPreferencesFromResource(this, R.xml.site_settings_preferences);
        getActivity().setTitle(R.string.prefs_site_settings);

        mProtectedContentMenuAvailable = Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT;

        if (getArguments() != null) {
            String category =
                    getArguments().getString(SingleCategoryPreferences.EXTRA_CATEGORY, "");
            if (MEDIA_KEY.equals(category)) {
                mMediaSubMenu = true;
                getActivity().setTitle(findPreference(MEDIA_KEY).getTitle().toString());
            }
        }

        configurePreferences();
        updatePreferenceStates();
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        ((ListView) getView().findViewById(android.R.id.list)).setDivider(null);
    }

    private Preference findPreference(@Type int type) {
        return findPreference(SiteSettingsCategory.preferenceKey(type));
    }

    private void configurePreferences() {
        if (mMediaSubMenu) {
            // The Media sub-menu only contains Protected Content and Autoplay, so remove all other
            // menus.
            for (@Type int i = 0; i < Type.NUM_ENTRIES; i++) {
                if (i == Type.AUTOPLAY || i == Type.PROTECTED_MEDIA) continue;
                getPreferenceScreen().removePreference(findPreference(i));
            }
            getPreferenceScreen().removePreference(findPreference(MEDIA_KEY));
            getPreferenceScreen().removePreference(findPreference(TRANSLATE_KEY));
        } else {
            // If both Autoplay and Protected Content menus are available, they'll be tucked under
            // the Media key. Otherwise, we can remove the Media menu entry.
            if (!mProtectedContentMenuAvailable) {
                getPreferenceScreen().removePreference(findPreference(MEDIA_KEY));
            } else {
                // This will be tucked under the Media subkey, so no reason to show them now.
                getPreferenceScreen().removePreference(findPreference(Type.AUTOPLAY));
            }
            getPreferenceScreen().removePreference(findPreference(Type.PROTECTED_MEDIA));
            // TODO(csharrison): Remove this condition once the experimental UI lands. It is not
            // great to dynamically remove the preference in this way.
            if (!SiteSettingsCategory.adsCategoryEnabled()) {
                getPreferenceScreen().removePreference(findPreference(Type.ADS));
            }
            if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SOUND_CONTENT_SETTING)) {
                getPreferenceScreen().removePreference(findPreference(Type.SOUND));
            }
            if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CLIPBOARD_CONTENT_SETTING)) {
                getPreferenceScreen().removePreference(findPreference(Type.CLIPBOARD));
            }
            // The new Languages Preference *feature* is an advanced version of this translate
            // preference. Once Languages Preference is enabled, remove this setting.
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.LANGUAGES_PREFERENCE)) {
                getPreferenceScreen().removePreference(findPreference(TRANSLATE_KEY));
            }
            if (!ChromeFeatureList.isEnabled(ChromeFeatureList.GENERIC_SENSOR_EXTRA_CLASSES)) {
                getPreferenceScreen().removePreference(findPreference(Type.SENSORS));
            }
        }
    }

    private void updatePreferenceStates() {
        PrefServiceBridge prefServiceBridge = PrefServiceBridge.getInstance();

        // Translate preference.
        Preference translatePref = findPreference(TRANSLATE_KEY);
        if (translatePref != null) setTranslateStateSummary(translatePref);

        // Preferences that navigate to Website Settings.
        List<Integer> websitePrefs = new ArrayList<Integer>();
        if (mMediaSubMenu) {
            websitePrefs.add(Type.PROTECTED_MEDIA);
            websitePrefs.add(Type.AUTOPLAY);
        } else {
            if (SiteSettingsCategory.adsCategoryEnabled()) {
                websitePrefs.add(Type.ADS);
            }
            websitePrefs.add(Type.AUTOMATIC_DOWNLOADS);

            // When showing the main menu, if Protected Content is not available, only Autoplay
            // will be visible.
            if (!mProtectedContentMenuAvailable) {
                websitePrefs.add(Type.AUTOPLAY);
            }
            websitePrefs.add(Type.BACKGROUND_SYNC);
            websitePrefs.add(Type.CAMERA);
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.CLIPBOARD_CONTENT_SETTING)) {
                websitePrefs.add(Type.CLIPBOARD);
            }
            websitePrefs.add(Type.COOKIES);
            websitePrefs.add(Type.JAVASCRIPT);
            websitePrefs.add(Type.DEVICE_LOCATION);
            websitePrefs.add(Type.MICROPHONE);
            websitePrefs.add(Type.NOTIFICATIONS);
            websitePrefs.add(Type.POPUPS);
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.GENERIC_SENSOR_EXTRA_CLASSES)) {
                websitePrefs.add(Type.SENSORS);
            }
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.SOUND_CONTENT_SETTING)) {
                websitePrefs.add(Type.SOUND);
            }
            websitePrefs.add(Type.USB);
        }

        // Initialize the summary and icon for all preferences that have an
        // associated content settings entry.
        for (@Type int prefCategory : websitePrefs) {
            Preference p = findPreference(prefCategory);
            int contentType = SiteSettingsCategory.contentSettingsType(prefCategory);
            boolean requiresTriStateSetting =
                    prefServiceBridge.requiresTriStateContentSetting(contentType);

            boolean checked = false;
            ContentSetting setting = ContentSetting.DEFAULT;

            if (prefCategory == Type.DEVICE_LOCATION) {
                checked = LocationSettings.getInstance().areAllLocationSettingsEnabled();
            } else if (requiresTriStateSetting) {
                setting = ContentSetting.fromInt(prefServiceBridge.getContentSetting(contentType));
            } else {
                checked = prefServiceBridge.isCategoryEnabled(contentType);
            }

            p.setTitle(ContentSettingsResources.getTitle(contentType));
            p.setOnPreferenceClickListener(this);

            if ((Type.CAMERA == prefCategory || Type.MICROPHONE == prefCategory)
                    && SiteSettingsCategory.createFromType(prefCategory)
                               .showPermissionBlockedMessage(getActivity())) {
                // Show 'disabled' message when permission is not granted in Android.
                p.setSummary(ContentSettingsResources.getCategorySummary(contentType, false));
            } else if (Type.AUTOPLAY == prefCategory
                    && DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()) {
                // Disable autoplay preference if Data Saver is ON.
                p.setSummary(ContentSettingsResources.getAutoplayDisabledByDataSaverSummary());
                p.setEnabled(false);
            } else if (Type.COOKIES == prefCategory && checked
                    && prefServiceBridge.isBlockThirdPartyCookiesEnabled()) {
                p.setSummary(ContentSettingsResources.getCookieAllowedExceptThirdPartySummary());
            } else if (Type.DEVICE_LOCATION == prefCategory && checked
                    && prefServiceBridge.isLocationAllowedByPolicy()) {
                p.setSummary(ContentSettingsResources.getGeolocationAllowedSummary());
            } else if (Type.CLIPBOARD == prefCategory && !checked) {
                p.setSummary(ContentSettingsResources.getClipboardBlockedListSummary());
            } else if (Type.ADS == prefCategory && !checked) {
                p.setSummary(ContentSettingsResources.getAdsBlockedListSummary());
            } else if (Type.SOUND == prefCategory && !checked) {
                p.setSummary(ContentSettingsResources.getSoundBlockedListSummary());
            } else if (requiresTriStateSetting) {
                p.setSummary(ContentSettingsResources.getCategorySummary(setting));
            } else {
                p.setSummary(ContentSettingsResources.getCategorySummary(contentType, checked));
            }

            if (p.isEnabled()) {
                p.setIcon(PreferenceUtils.getTintedIcon(
                        getActivity(), ContentSettingsResources.getIcon(contentType)));
            } else {
                p.setIcon(ContentSettingsResources.getDisabledIcon(contentType, getResources()));
            }
        }

        Preference p = findPreference(Type.ALL_SITES);
        if (p != null) p.setOnPreferenceClickListener(this);
        p = findPreference(MEDIA_KEY);
        if (p != null) p.setOnPreferenceClickListener(this);
        // TODO(finnur): Re-move this for Storage once it can be moved to the 'Usage' menu.
        p = findPreference(Type.USE_STORAGE);
        if (p != null) p.setOnPreferenceClickListener(this);
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferenceStates();
    }

    // OnPreferenceClickListener:

    @Override
    public boolean onPreferenceClick(Preference preference) {
        preference.getExtras().putString(
                SingleCategoryPreferences.EXTRA_CATEGORY, preference.getKey());
        preference.getExtras().putString(SingleCategoryPreferences.EXTRA_TITLE,
                preference.getTitle().toString());
        return false;
    }

    private void setTranslateStateSummary(Preference translatePref) {
        boolean translateEnabled = PrefServiceBridge.getInstance().isTranslateEnabled();
        translatePref.setSummary(translateEnabled
                ? R.string.website_settings_category_ask
                : R.string.website_settings_category_blocked);
    }
}
