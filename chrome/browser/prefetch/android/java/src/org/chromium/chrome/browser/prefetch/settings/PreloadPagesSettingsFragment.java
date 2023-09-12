// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prefetch.settings;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.FragmentSettingsLauncher;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

/**
 * Fragment containing Preload Pages settings.
 */
public class PreloadPagesSettingsFragment extends PreloadPagesSettingsFragmentBase
        implements FragmentSettingsLauncher,
                   RadioButtonGroupPreloadPagesSettings.OnPreloadPagesStateDetailsRequested,
                   Preference.OnPreferenceChangeListener {
    @VisibleForTesting
    static final String PREF_MANAGED_DISCLAIMER_TEXT = "managed_disclaimer_text";
    @VisibleForTesting
    static final String PREF_PRELOAD_PAGES = "preload_pages_radio_button_group";

    // An instance of SettingsLauncher that is used to launch Preload Pages subsections.
    private SettingsLauncher mSettingsLauncher;
    private RadioButtonGroupPreloadPagesSettings mPreloadPagesPreference;

    /**
     * @return A summary that describes the current Preload Pages state.
     */
    public static String getPreloadPagesSummaryString(Context context) {
        @PreloadPagesState
        int preloadPagesState = PreloadPagesSettingsBridge.getState();
        if (preloadPagesState == PreloadPagesState.EXTENDED_PRELOADING) {
            return context.getString(R.string.preload_pages_extended_preloading_title);
        }
        if (preloadPagesState == PreloadPagesState.STANDARD_PRELOADING) {
            return context.getString(R.string.preload_pages_standard_preloading_title);
        }
        if (preloadPagesState == PreloadPagesState.NO_PRELOADING) {
            return context.getString(R.string.preload_pages_no_preloading_title);
        }
        assert false : "Should not be reached";
        return "";
    }

    @Override
    protected void onCreatePreferencesInternal(Bundle bundle, String s) {
        ManagedPreferenceDelegate managedPreferenceDelegate = createManagedPreferenceDelegate();

        mPreloadPagesPreference = findPreference(PREF_PRELOAD_PAGES);
        mPreloadPagesPreference.init(PreloadPagesSettingsBridge.getState());
        mPreloadPagesPreference.setPreloadPagesStateDetailsRequestedListener(this);
        mPreloadPagesPreference.setManagedPreferenceDelegate(managedPreferenceDelegate);
        mPreloadPagesPreference.setOnPreferenceChangeListener(this);

        findPreference(PREF_MANAGED_DISCLAIMER_TEXT).setVisible(
               managedPreferenceDelegate.isPreferenceClickDisabled(mPreloadPagesPreference));
    }

    @Override
    protected int getPreferenceResource() {
        return R.xml.preload_pages_preferences;
    }

    @Override
    public void onPreloadPagesStateDetailsRequested(@PreloadPagesState int preloadPagesState) {
        if (preloadPagesState == PreloadPagesState.EXTENDED_PRELOADING) {
            mSettingsLauncher.launchSettingsActivity(
                    getActivity(), ExtendedPreloadingSettingsFragment.class);
        } else if (preloadPagesState == PreloadPagesState.STANDARD_PRELOADING) {
            mSettingsLauncher.launchSettingsActivity(
                    getActivity(), StandardPreloadingSettingsFragment.class);
        } else {
            assert false : "Should not be reached";
        }
    }

    @Override
    public void setSettingsLauncher(SettingsLauncher settingsLauncher) {
        mSettingsLauncher = settingsLauncher;
    }

    private ChromeManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return new ChromeManagedPreferenceDelegate(getProfile()) {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                String key = preference.getKey();
                assert PREF_MANAGED_DISCLAIMER_TEXT.equals(key)
                        || PREF_PRELOAD_PAGES.equals(key) : "Wrong preference key: " + key;
                return PreloadPagesSettingsBridge.isNetworkPredictionManaged();
            }
        };
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        assert PREF_PRELOAD_PAGES.equals(key) : "Unexpected preference key.";
        @PreloadPagesState
        int newState = (int) newValue;
        @PreloadPagesState
        int currentState = PreloadPagesSettingsBridge.getState();
        if (newState == currentState) {
            return true;
        }
        PreloadPagesSettingsBridge.setState(newState);
        return true;
    }
}
