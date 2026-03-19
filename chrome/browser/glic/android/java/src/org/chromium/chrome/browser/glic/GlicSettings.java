// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.chromium.build.NullUtil.assertNonNull;

import android.os.Bundle;
import android.text.method.LinkMovementMethod;
import android.view.View;
import android.widget.TextView;

import androidx.preference.Preference;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.ChromeExpandableSwitchPreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** Fragment for Glic configurations in Chrome. */
@NullMarked
public class GlicSettings extends ChromeBaseSettingsFragment {
    private static final String PREFERENCE_BUTTON = "glic_button";
    private static final String PERMISSION_LOCATION = "permissions_location";
    private static final String PERMISSION_DEFAULT_TAB_ACCESS =
            "glic_permissions_default_tab_access";
    private static final String PERMISSION_AUTO_BROWSE= "glic_permissions_auto_browse";
    private static final String LEARN_MORE_AI_URL = "https://support.google.com/a/answer/15706919";
    private static final String AUTO_BROWSE_LEARN_MORE_URL =
            "https://support.google.com/gemini/answer/16821166";
    private static final String AUTO_BROWSE_CONSIDER_SAFELY_URL =
            "https://policies.google.com/terms/generative-ai/use-policy";
    private static final String AUTO_BROWSE_CONSIDER_UNEXPECTED_RESULTS_URL =
            "https://support.google.com/gemini/answer/16821166";

    public static final String PREF_KEY_GLIC_PERMISSIONS_ACTIVITY = "glic_permissions_activity";
    public static final String PREF_KEY_GLIC_EXTENSIONS = "glic_extensions";

    private final SharedPreferencesManager mSharedPreferencesManager =
            ChromeSharedPreferences.getInstance();
    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.glic_settings);
        mPageTitle.set(getString(R.string.settings_glic_button_toggle));
        SettingsCustomTabLauncher customTabLauncher = getCustomTabLauncher();

        setupSwitchPreference(PREFERENCE_BUTTON, ChromePreferenceKeys.GLIC_BUTTON_PINNED);

        setupSwitchPreference(
                PERMISSION_LOCATION, ChromePreferenceKeys.GLIC_PRECISE_LOCATION_SETTING_ENABLED);

        ChromeExpandableSwitchPreference tabAccessPref =
                setupSwitchPreference(
                        PERMISSION_DEFAULT_TAB_ACCESS,
                        ChromePreferenceKeys.GLIC_SHARE_CURRENT_TAB_DEFAULT_ACCESS_ENABLED);
        String summary =
                getString(
                        R.string
                                .settings_glic_permissions_default_tab_access_toggle_sublabel_data_protected);
        tabAccessPref.setSummary(
                SpanApplier.applySpans(summary, getLearnMoreSpanInfo(LEARN_MORE_AI_URL)));

        ChromeExpandableSwitchPreference autoBrowsePref =
                setupSwitchPreference(
                        PERMISSION_AUTO_BROWSE,
                        ChromePreferenceKeys.GLIC_AUTO_BROWSE_SETTING_ENABLED);
        String autoBrowseSummary =
                getString(R.string.settings_glic_permissions_chrome_web_actuation_toggle_sublabel);
        autoBrowsePref.setSummary(
                SpanApplier.applySpans(
                        autoBrowseSummary, getLearnMoreSpanInfo(AUTO_BROWSE_LEARN_MORE_URL)));
        autoBrowsePref.setOnBindExpandedAreaListener(this::setupAutoBrowseExpandedArea);

        Preference permissionActivityPref =
                assertNonNull(findPreference(PREF_KEY_GLIC_PERMISSIONS_ACTIVITY));
        permissionActivityPref.setOnPreferenceClickListener(
                preference -> {
                    customTabLauncher.openUrlInCct(
                            getActivity(),
                            getString(R.string.settings_glic_permissions_activity_button_url));
                    return true;
                });

        Preference permissionConnectedAppsPref = findPreference(PREF_KEY_GLIC_EXTENSIONS);
        if (permissionConnectedAppsPref != null) {
            permissionConnectedAppsPref.setOnPreferenceClickListener(
                    preference -> {
                        customTabLauncher.openUrlInCct(
                                getActivity(),
                                getString(R.string.settings_glic_extensions_button_url));
                        return true;
                    });
        }
    }

    private <T extends ChromeSwitchPreference> T setupSwitchPreference(
            String preferenceKey, String sharedPreferenceKey) {
        T preference = assertNonNull(findPreference(preferenceKey));
        preference.setChecked(mSharedPreferencesManager.readBoolean(sharedPreferenceKey, false));
        preference.setOnPreferenceChangeListener(
                (pref, newValue) -> {
                    mSharedPreferencesManager.writeBoolean(sharedPreferenceKey, (boolean) newValue);
                    return true;
                });
        return preference;
    }

    private void setupAutoBrowseExpandedArea(View expandedArea) {
        TextView consider2 =
                expandedArea.findViewById(
                        R.id.glic_auto_browse_consider_user_responsibility_description);
        if (consider2 != null && consider2.getMovementMethod() == null) {
            String text =
                    getString(R.string.settings_glic_permissions_web_actuation_toggle_consider_2);
            consider2.setText(
                    SpanApplier.applySpans(
                            text,
                            createLinkSpanInfo("$1", AUTO_BROWSE_CONSIDER_SAFELY_URL),
                            createLinkSpanInfo(
                                    "$2", AUTO_BROWSE_CONSIDER_UNEXPECTED_RESULTS_URL)));
            consider2.setMovementMethod(LinkMovementMethod.getInstance());
        }
    }

    private SpanApplier.SpanInfo createSpanInfo(String openTag, String url) {
        SettingsCustomTabLauncher customTabLauncher = getCustomTabLauncher();
        return new SpanApplier.SpanInfo(
                openTag,
                "</a>",
                new ChromeClickableSpan(
                        getContext(),
                        v -> {
                            customTabLauncher.openUrlInCct(getContext(), url);
                        }));
    }

    private SpanApplier.SpanInfo createLinkSpanInfo(String placeholderIndex, String url) {
        return createSpanInfo("<a href=\"" + placeholderIndex + "\" target=\"_blank\">", url);
    }

    private SpanApplier.SpanInfo getLearnMoreSpanInfo(String url) {
        return createSpanInfo("<a href=\"#\">", url);
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    // TODO(crbug.com/480218604): Override #updateDynamicPreferences once the preference is
    // implemented.

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(GlicSettings.class.getName(), R.xml.glic_settings);
}
