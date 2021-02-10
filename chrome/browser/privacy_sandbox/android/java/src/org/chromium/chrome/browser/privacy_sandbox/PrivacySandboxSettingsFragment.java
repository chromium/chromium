// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.ChromeBulletSpan;

/**
 * Settings fragment for privacy sandbox settings. This class represents a View in the MVC paradigm.
 */
public class PrivacySandboxSettingsFragment
        extends PreferenceFragmentCompat implements Preference.OnPreferenceChangeListener {
    public static final String TRIAL_DESCRIPTION_PREFERENCE = "privacy_sandbox_trial_description";
    public static final String TOGGLE_PREFERENCE = "privacy_sandbox_toggle";

    public static CharSequence getStatusString(Context context) {
        return context.getString(PrivacySandboxBridge.isPrivacySandboxEnabled()
                        ? R.string.privacy_sandbox_status_enabled
                        : R.string.privacy_sandbox_status_disabled);
    }

    /**
     * Initializes all the objects related to the preferences page.
     */
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        // Add all preferences and set the title.
        getActivity().setTitle(R.string.prefs_privacy_sandbox);
        SettingsUtils.addPreferencesFromResource(this, R.xml.privacy_sandbox_preferences);
        // Format the trial description, which has bullet points.
        findPreference(TRIAL_DESCRIPTION_PREFERENCE)
                .setSummary(SpanApplier.applySpans(
                        getContext().getString(R.string.privacy_sandbox_trial_description),
                        new SpanInfo("<li1>", "</li1>", new ChromeBulletSpan(getContext())),
                        new SpanInfo("<li2>", "</li2>", new ChromeBulletSpan(getContext()))));

        ChromeSwitchPreference privacySandboxToggle =
                (ChromeSwitchPreference) findPreference(TOGGLE_PREFERENCE);
        privacySandboxToggle.setOnPreferenceChangeListener(this);
        privacySandboxToggle.setManagedPreferenceDelegate(createManagedPreferenceDelegate());
        privacySandboxToggle.setChecked(PrivacySandboxBridge.isPrivacySandboxEnabled());
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        if (!TOGGLE_PREFERENCE.equals(key)) return true;
        boolean enabled = (boolean) newValue;
        RecordUserAction.record(enabled ? "Settings.PrivacySandbox.ApisEnabled"
                                        : "Settings.PrivacySandbox.ApisDisabled");
        PrivacySandboxBridge.setPrivacySandboxEnabled(enabled);
        return true;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        LinearLayout view =
                (LinearLayout) super.onCreateView(inflater, container, savedInstanceState);
        LinearLayout headerView =
                (LinearLayout) inflater.inflate(R.layout.privacy_sandbox_header, view, false);
        // Add the header view to the top.
        view.addView(headerView, 0);
        return view;
    }

    private ChromeManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return preference -> {
            if (!TOGGLE_PREFERENCE.equals(preference.getKey())) return false;
            return PrivacySandboxBridge.isPrivacySandboxManaged();
        };
    }
}
