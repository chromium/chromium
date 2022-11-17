// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.os.Bundle;

import androidx.preference.Preference;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.ChromeBulletSpan;

/**
 * Settings fragment for privacy sandbox settings. This class represents a View in the MVC paradigm.
 */
public class PrivacySandboxSettingsFragment extends PrivacySandboxSettingsBaseFragment
        implements Preference.OnPreferenceChangeListener {
    public static final String PRIVACY_SANDBOX_URL = "https://www.privacysandbox.com";
    // Key for the argument with which the PrivacySandbox fragment will be launched. The value for
    // this argument should be part of the PrivacySandboxReferrer enum, which contains all points of
    // entry to the Privacy Sandbox UI.
    public static final String PRIVACY_SANDBOX_REFERRER = "privacy-sandbox-referrer";

    public static final String EXPERIMENT_DESCRIPTION_TITLE = "privacy_sandbox_title";
    public static final String EXPERIMENT_DESCRIPTION_PREFERENCE = "privacy_sandbox_description";
    public static final String TOGGLE_DESCRIPTION_PREFERENCE = "privacy_sandbox_toggle_description";
    public static final String TOGGLE_PREFERENCE = "privacy_sandbox_toggle";
    public static final String FLOC_PREFERENCE = "floc_page";

    private @PrivacySandboxReferrer int mPrivacySandboxReferrer;

    /**
     * Initializes all the objects related to the preferences page.
     */
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        assert !ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_3);
        // Add all preferences and set the title.
        getActivity().setTitle(R.string.prefs_privacy_sandbox);
        SettingsUtils.addPreferencesFromResource(this, R.xml.privacy_sandbox_preferences);

        // Modify the Privacy Sandbox elements.
        getPreferenceScreen().removePreference(findPreference(EXPERIMENT_DESCRIPTION_TITLE));
        updateFlocPreference();

        // Format the Privacy Sandbox description, which has a link.
        findPreference(EXPERIMENT_DESCRIPTION_PREFERENCE)
                .setSummary(SpanApplier.applySpans(
                        getContext().getString(R.string.privacy_sandbox_description_two),
                        new SpanInfo("<link>", "</link>",
                                new NoUnderlineClickableSpan(getContext(),
                                        (widget) -> openUrlInCct(PRIVACY_SANDBOX_URL)))));
        // Format the toggle description, which has bullet points.
        findPreference(TOGGLE_DESCRIPTION_PREFERENCE)
                .setSummary(SpanApplier.applySpans(
                        getContext().getString(R.string.privacy_sandbox_toggle_description_two),
                        new SpanInfo("<li1>", "</li1>", new ChromeBulletSpan(getContext())),
                        new SpanInfo("<li2>", "</li2>", new ChromeBulletSpan(getContext()))));

        ChromeSwitchPreference privacySandboxToggle =
                (ChromeSwitchPreference) findPreference(TOGGLE_PREFERENCE);
        privacySandboxToggle.setOnPreferenceChangeListener(this);
        privacySandboxToggle.setManagedPreferenceDelegate(createManagedPreferenceDelegate());
        privacySandboxToggle.setChecked(PrivacySandboxBridge.isPrivacySandboxEnabled());

        parseAndRecordReferrer(bundle);
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        if (!TOGGLE_PREFERENCE.equals(key)) return true;
        boolean enabled = (boolean) newValue;
        RecordUserAction.record(enabled ? "Settings.PrivacySandbox.ApisEnabled"
                                        : "Settings.PrivacySandbox.ApisDisabled");
        PrivacySandboxBridge.setPrivacySandboxEnabled(enabled);
        updateFlocPreference();
        return true;
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putInt(PRIVACY_SANDBOX_REFERRER, mPrivacySandboxReferrer);
    }

    @Override
    public void onResume() {
        super.onResume();
        updateFlocPreference();
    }

    private ChromeManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return preference -> {
            if (!TOGGLE_PREFERENCE.equals(preference.getKey())) return false;
            return PrivacySandboxBridge.isPrivacySandboxManaged();
        };
    }

    private void parseAndRecordReferrer(Bundle savedInstanceState) {
        if (savedInstanceState != null
                && savedInstanceState.containsKey(PRIVACY_SANDBOX_REFERRER)) {
            mPrivacySandboxReferrer = savedInstanceState.getInt(PRIVACY_SANDBOX_REFERRER);
        } else {
            Bundle extras = getArguments();
            assert (extras != null)
                    && extras.containsKey(PRIVACY_SANDBOX_REFERRER)
                : "PrivacySandboxSettingsFragment must be launched with a privacy-sandbox-referrer "
                            + "fragment argument, but none was provided.";
            mPrivacySandboxReferrer = extras.getInt(PRIVACY_SANDBOX_REFERRER);
        }
        // Record all the referrer metrics.
        RecordHistogram.recordEnumeratedHistogram("Settings.PrivacySandbox.PrivacySandboxReferrer",
                mPrivacySandboxReferrer, PrivacySandboxReferrer.COUNT);
        if (mPrivacySandboxReferrer == PrivacySandboxReferrer.PRIVACY_SETTINGS) {
            RecordUserAction.record("Settings.PrivacySandbox.OpenedFromSettingsParent");
        } else if (mPrivacySandboxReferrer == PrivacySandboxReferrer.COOKIES_SNACKBAR) {
            RecordUserAction.record("Settings.PrivacySandbox.OpenedFromCookiesPageToast");
        }
    }

    private void updateFlocPreference() {
        // Update the Preference linking to the FLoC page if shown.
        Preference flocPreference = findPreference(FLOC_PREFERENCE);
        if (flocPreference != null) {
            flocPreference.setEnabled(PrivacySandboxBridge.isPrivacySandboxEnabled());
            flocPreference.setSummary(PrivacySandboxBridge.getFlocStatusString());
        }
    }
}
