// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.os.Build;
import android.os.Bundle;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceFragmentCompat;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.chrome.browser.offlinepages.prefetch.PrefetchConfiguration;
import org.chromium.chrome.browser.offlinepages.prefetch.PrefetchPrefs;
import org.chromium.chrome.browser.preferences.website.ContentSettingsResources;
import org.chromium.chrome.browser.preferences.website.SingleCategoryPreferences;
import org.chromium.chrome.browser.preferences.website.SiteSettingsCategory;
import org.chromium.chrome.browser.preferences.website.WebsitePreferenceBridge;

/**
 * Settings fragment that allows the user to configure notifications. It contains general
 * notification channels at the top level and links to website specific notifications. This is only
 * used on pre-O devices, devices on Android O+ will link to the Android notification settings.
 */
public class NotificationsPreferences extends PreferenceFragmentCompat {
    // These are package-private to be used in tests.
    static final String PREF_FROM_WEBSITES = "from_websites";
    static final String PREF_SUGGESTIONS = "content_suggestions";

    private Preference mFromWebsitesPref;

    // The following field is only set if Feed is disabled, and should be null checked before
    // being used.
    @Nullable
    private ChromeSwitchPreference mSuggestionsPref;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        assert Build.VERSION.SDK_INT < Build.VERSION_CODES.O
            : "NotificationsPreferences should only be used pre-O.";

        PreferenceUtils.addPreferencesFromResource(this, R.xml.notifications_preferences);
        getActivity().setTitle(R.string.prefs_notifications);

        mSuggestionsPref = (ChromeSwitchPreference) findPreference(PREF_SUGGESTIONS);
        mSuggestionsPref.setOnPreferenceChangeListener((Preference preference, Object newValue) -> {
            PrefetchPrefs.setNotificationEnabled((boolean) newValue);
            return true;
        });

        mFromWebsitesPref = findPreference(PREF_FROM_WEBSITES);
        mFromWebsitesPref.getExtras().putString(SingleCategoryPreferences.EXTRA_CATEGORY,
                SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.NOTIFICATIONS));
    }

    @Override
    public void onResume() {
        super.onResume();
        update();
    }

    /**
     * Updates the state of displayed preferences.
     */
    private void update() {
        if (mSuggestionsPref != null) {
            boolean prefetchingFeatureEnabled = PrefetchConfiguration.isPrefetchingFlagEnabled();
            boolean notificationsEnabled = PrefetchPrefs.getNotificationEnabled();
            mSuggestionsPref.setChecked(prefetchingFeatureEnabled && notificationsEnabled);
            mSuggestionsPref.setEnabled(prefetchingFeatureEnabled);
            mSuggestionsPref.setSummary(prefetchingFeatureEnabled
                            ? R.string.notifications_content_suggestions_summary
                            : R.string.notifications_content_suggestions_summary_disabled);
        }

        mFromWebsitesPref.setSummary(ContentSettingsResources.getCategorySummary(
                ContentSettingsType.NOTIFICATIONS,
                WebsitePreferenceBridge.isCategoryEnabled(ContentSettingsType.NOTIFICATIONS)));
    }
}
