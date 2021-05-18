// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;

import org.chromium.base.IntentUtils;
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
public class PrivacySandboxSettingsFragment
        extends PreferenceFragmentCompat implements Preference.OnPreferenceChangeListener {
    public static final String PRIVACY_SANDBOX_DEFAULT_URL = "https://www.privacysandbox.com";
    public static final String EXPERIMENT_URL_PARAM = "website-url";
    // Key for the argument with which the PrivacySandbox fragment will be launched. The value for
    // this argument should be part of the PrivacySandboxReferrer enum, which contains all points of
    // entry to the Privacy Sandbox UI.
    public static final String PRIVACY_SANDBOX_REFERRER = "privacy-sandbox-referrer";

    public static final String EXPERIMENT_DESCRIPTION_PREFERENCE = "privacy_sandbox_description";
    public static final String TOGGLE_DESCRIPTION_PREFERENCE = "privacy_sandbox_toggle_description";
    public static final String TOGGLE_PREFERENCE = "privacy_sandbox_toggle";

    private @PrivacySandboxReferrer int mPrivacySandboxReferrer;
    private PrivacySandboxHelpers.CustomTabIntentHelper mCustomTabHelper;
    private PrivacySandboxHelpers.TrustedIntentHelper mTrustedIntentHelper;

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
        // Format the Privacy Sandbox description, which has a link.
        findPreference(EXPERIMENT_DESCRIPTION_PREFERENCE)
                .setSummary(SpanApplier.applySpans(
                        getContext().getString(R.string.privacy_sandbox_description),
                        new SpanInfo("<link>", "</link>",
                                new NoUnderlineClickableSpan(getContext().getResources(),
                                        (widget) -> openUrlInCct(getPrivacySandboxUrl())))));
        // Format the toggle description, which has bullet points.
        findPreference(TOGGLE_DESCRIPTION_PREFERENCE)
                .setSummary(SpanApplier.applySpans(
                        getContext().getString(R.string.privacy_sandbox_toggle_description),
                        new SpanInfo("<li1>", "</li1>", new ChromeBulletSpan(getContext())),
                        new SpanInfo("<li2>", "</li2>", new ChromeBulletSpan(getContext()))));

        ChromeSwitchPreference privacySandboxToggle =
                (ChromeSwitchPreference) findPreference(TOGGLE_PREFERENCE);
        privacySandboxToggle.setOnPreferenceChangeListener(this);
        privacySandboxToggle.setManagedPreferenceDelegate(createManagedPreferenceDelegate());
        privacySandboxToggle.setChecked(PrivacySandboxBridge.isPrivacySandboxEnabled());

        parseAndRecordReferrer(bundle);

        // Enable the options menu to be able to use a custom question mark button.
        setHasOptionsMenu(true);
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
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putInt(PRIVACY_SANDBOX_REFERRER, mPrivacySandboxReferrer);
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        // Add the custom question mark button.
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(VectorDrawableCompat.create(
                getResources(), R.drawable.ic_help_and_feedback, getActivity().getTheme()));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            // Action for the question mark button.
            openUrlInCct(getPrivacySandboxUrl());
            return true;
        }
        return false;
    }

    /**
     * Set the necessary CCT helpers to be able to natively open links. This is needed because the
     * helpers are not modularized.
     */
    public void setCctHelpers(PrivacySandboxHelpers.CustomTabIntentHelper tabHelper,
            PrivacySandboxHelpers.TrustedIntentHelper intentHelper) {
        mCustomTabHelper = tabHelper;
        mTrustedIntentHelper = intentHelper;
    }

    private ChromeManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return preference -> {
            if (!TOGGLE_PREFERENCE.equals(preference.getKey())) return false;
            return PrivacySandboxBridge.isPrivacySandboxManaged();
        };
    }

    private String getPrivacySandboxUrl() {
        // Get the URL from Finch, if defined.
        String url = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS, EXPERIMENT_URL_PARAM);
        if (url == null || url.isEmpty()) return PRIVACY_SANDBOX_DEFAULT_URL;
        return url;
    }

    private void openUrlInCct(String url) {
        assert (mCustomTabHelper != null)
                && (mTrustedIntentHelper != null)
            : "CCT helpers must be set on PrivacySandboxSettingsFragment before opening a link.";
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder().setShowTitle(true).build();
        customTabIntent.intent.setData(Uri.parse(url));
        Intent intent = mCustomTabHelper.createCustomTabActivityIntent(
                getContext(), customTabIntent.intent);
        intent.setPackage(getContext().getPackageName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, getContext().getPackageName());
        mTrustedIntentHelper.addTrustedIntentExtras(intent);
        IntentUtils.safeStartActivity(getContext(), intent);
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
}
