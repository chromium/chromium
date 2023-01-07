// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.settings;

import android.os.Bundle;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.R;
import org.chromium.components.browser_ui.settings.FragmentSettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/**
 * Fragment containing Safe Browsing settings.
 */
public class FeedAutoplaySettingsFragment extends PreferenceFragmentCompat
        implements FragmentSettingsLauncher, Preference.OnPreferenceChangeListener {
    public static final String VIDEO_PREVIEWS_PREF = "video_previews_pref";

    private SettingsLauncher mSettingsLauncher;
    private RadioButtonGroupVideoPreviewsPreference mVideoPreviewsPreference;

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.feed_autoplay_preferences);
        getActivity().setTitle(R.string.feed_autoplay_title);

        mVideoPreviewsPreference =
                (RadioButtonGroupVideoPreviewsPreference) findPreference(VIDEO_PREVIEWS_PREF);
        mVideoPreviewsPreference.setOnPreferenceChangeListener(this);
        mVideoPreviewsPreference.initialize(FeedServiceBridge.getVideoPreviewsTypePreference());
    }

    @Override
    public void setSettingsLauncher(SettingsLauncher settingsLauncher) {}

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        FeedServiceBridge.setVideoPreviewsTypePreference((int) newValue);
        return true;
    }
}
