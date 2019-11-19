// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceFragmentCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.accessibility.FontSizePrefs;
import org.chromium.chrome.browser.accessibility.FontSizePrefs.FontSizePrefsObserver;
import org.chromium.chrome.browser.util.AccessibilityUtil;

import java.text.NumberFormat;

/**
 * Fragment to keep track of all the accessibility related preferences.
 */
public class AccessibilityPreferences
        extends PreferenceFragmentCompat implements Preference.OnPreferenceChangeListener {
    static final String PREF_TEXT_SCALE = "text_scale";
    static final String PREF_FORCE_ENABLE_ZOOM = "force_enable_zoom";
    static final String PREF_READER_FOR_ACCESSIBILITY = "reader_for_accessibility";
    static final String PREF_CAPTIONS = "captions";

    private NumberFormat mFormat;
    private FontSizePrefs mFontSizePrefs;

    private TextScalePreference mTextScalePref;
    private ChromeBaseCheckBoxPreference mForceEnableZoomPref;

    private FontSizePrefsObserver mFontSizePrefsObserver = new FontSizePrefsObserver() {
        @Override
        public void onFontScaleFactorChanged(float fontScaleFactor, float userFontScaleFactor) {
            updateTextScaleSummary(userFontScaleFactor);
        }

        @Override
        public void onForceEnableZoomChanged(boolean enabled) {
            mForceEnableZoomPref.setChecked(enabled);
        }
    };

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(R.string.prefs_accessibility);
        PreferenceUtils.addPreferencesFromResource(this, R.xml.accessibility_preferences);

        mFormat = NumberFormat.getPercentInstance();
        mFontSizePrefs = FontSizePrefs.getInstance();

        mTextScalePref = (TextScalePreference) findPreference(PREF_TEXT_SCALE);
        mTextScalePref.setOnPreferenceChangeListener(this);

        mForceEnableZoomPref =
                (ChromeBaseCheckBoxPreference) findPreference(PREF_FORCE_ENABLE_ZOOM);
        mForceEnableZoomPref.setOnPreferenceChangeListener(this);

        ChromeBaseCheckBoxPreference readerForAccessibilityPref =
                (ChromeBaseCheckBoxPreference) findPreference(PREF_READER_FOR_ACCESSIBILITY);
        readerForAccessibilityPref.setChecked(
                PrefServiceBridge.getInstance().getBoolean(Pref.READER_FOR_ACCESSIBILITY_ENABLED));
        readerForAccessibilityPref.setOnPreferenceChangeListener(this);

        ChromeBaseCheckBoxPreference mAccessibilityTabSwitcherPref =
                (ChromeBaseCheckBoxPreference) findPreference(
                        ChromePreferenceKeys.ACCESSIBILITY_TAB_SWITCHER);
        if (AccessibilityUtil.isAccessibilityEnabled()) {
            mAccessibilityTabSwitcherPref.setChecked(
                    SharedPreferencesManager.getInstance().readBoolean(
                            ChromePreferenceKeys.ACCESSIBILITY_TAB_SWITCHER, true));
        } else {
            getPreferenceScreen().removePreference(mAccessibilityTabSwitcherPref);
        }

        Preference captions = findPreference(PREF_CAPTIONS);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CAPTION_SETTINGS)) {
            captions.setOnPreferenceClickListener(preference -> {
                Intent intent = new Intent(Settings.ACTION_CAPTIONING_SETTINGS);

                // Open the activity in a new task because the back button on the caption
                // settings page navigates to the previous settings page instead of Chrome.
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                startActivity(intent);

                return true;
            });
        } else {
            getPreferenceScreen().removePreference(captions);
        }
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        setDivider(null);
    }

    @Override
    public void onStart() {
        super.onStart();
        updateValues();
        // TODO(crbug.com/977206): Move this call to make TextScalePreference self-contained
        mTextScalePref.startObservingFontPrefs();
        mFontSizePrefs.addObserver(mFontSizePrefsObserver);
    }

    @Override
    public void onStop() {
        // TODO(crbug.com/977206): Move this call to make TextScalePreference self-contained
        mTextScalePref.stopObservingFontPrefs();
        mFontSizePrefs.removeObserver(mFontSizePrefsObserver);
        super.onStop();
    }

    private void updateValues() {
        float userFontScaleFactor = mFontSizePrefs.getUserFontScaleFactor();
        mTextScalePref.setValue(userFontScaleFactor);
        updateTextScaleSummary(userFontScaleFactor);

        mForceEnableZoomPref.setChecked(mFontSizePrefs.getForceEnableZoom());
    }

    // TODO(crbug.com/977206): Move this to within TextScalePreference
    private void updateTextScaleSummary(float userFontScaleFactor) {
        mTextScalePref.setSummary(mFormat.format(userFontScaleFactor));
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (PREF_TEXT_SCALE.equals(preference.getKey())) {
            mFontSizePrefs.setUserFontScaleFactor((Float) newValue);
        } else if (PREF_FORCE_ENABLE_ZOOM.equals(preference.getKey())) {
            mFontSizePrefs.setForceEnableZoomFromUser((Boolean) newValue);
        } else if (PREF_READER_FOR_ACCESSIBILITY.equals(preference.getKey())) {
            PrefServiceBridge.getInstance().setBoolean(
                    Pref.READER_FOR_ACCESSIBILITY_ENABLED, (Boolean) newValue);
        }
        return true;
    }
}
