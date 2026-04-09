// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.chromium.build.NullUtil.assertNonNull;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.text.method.LinkMovementMethod;
import android.view.View;
import android.widget.TextView;

import androidx.core.content.ContextCompat;
import androidx.preference.Preference;
import androidx.preference.Preference.OnPreferenceChangeListener;

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
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** Fragment for Glic configurations in Chrome. */
@NullMarked
public class GlicSettings extends ChromeBaseSettingsFragment {
    private static final String PREFERENCE_BUTTON = "glic_button";
    private static final String PERMISSION_LOCATION = "permissions_location";
    private static final String PERMISSION_DEFAULT_TAB_ACCESS =
            "glic_permissions_default_tab_access";
    private static final String PERMISSION_AUTO_BROWSE = "glic_permissions_auto_browse";
    // TODO(b/498717684): Replace answer number urls with a p= identifier instead.
    private static final String LEARN_MORE_AI_URL = "https://support.google.com/a/answer/15706919";
    private static final String LEARN_MORE_MANAGED_AI_URL =
            "https://support.google.com/chrome/a/answer/14443058";
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

    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.glic_settings);
        mPageTitle.set(getString(R.string.glic_setting_label));
        SettingsCustomTabLauncher customTabLauncher = getCustomTabLauncher();

        PrefService prefService = UserPrefs.get(getProfile());
        mPrefChangeRegistrar = new PrefChangeRegistrar(prefService);

        setupSwitchPreference(
                PREFERENCE_BUTTON,
                ChromePreferenceKeys.GLIC_BUTTON_PINNED,
                GlicPrefNames.GLIC_PINNED_TO_TABSTRIP,
                /* extraListener= */ null);

        ChromeSwitchPreference locationPref =
                setupSwitchPreference(
                        PERMISSION_LOCATION,
                        ChromePreferenceKeys.GLIC_PRECISE_LOCATION_SETTING_ENABLED,
                        GlicPrefNames.GLIC_GEOLOCATION_ENABLED,
                        (preference, newValue) -> {
                            boolean enabled = (boolean) newValue;
                            if (enabled) {
                                ensureFineLocationPermissionGranted();
                            }
                            return true;
                        });

        if (locationPref.isChecked()) {
            ensureFineLocationPermissionGranted();
        }

        ChromeExpandableSwitchPreference tabAccessPref =
                setupSwitchPreference(
                        PERMISSION_DEFAULT_TAB_ACCESS,
                        ChromePreferenceKeys.GLIC_SHARE_CURRENT_TAB_DEFAULT_ACCESS_ENABLED,
                        GlicPrefNames.GLIC_TAB_CONTEXT_ENABLED,
                        /* extraListener= */ null);

        String summary =
                getString(
                        R.string
                                .settings_glic_permissions_default_tab_access_toggle_sublabel_data_protected);
        tabAccessPref.setSummary(
                SpanApplier.applySpans(summary, getLearnMoreSpanInfo(LEARN_MORE_AI_URL)));

        ChromeExpandableSwitchPreference autoBrowsePref =
                setupSwitchPreference(
                        PERMISSION_AUTO_BROWSE,
                        ChromePreferenceKeys.GLIC_AUTO_BROWSE_SETTING_ENABLED,
                        GlicPrefNames.GLIC_USER_ENABLED_ACTUATION_ON_WEB,
                        /* extraListener= */ null);

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

        GlicExtraInfoPreference aiInfoPref = findPreference("glic_custom_box_preference");
        if (aiInfoPref != null) {
            aiInfoPref.setOnLearnMoreClicked(
                    () -> customTabLauncher.openUrlInCct(getActivity(), LEARN_MORE_MANAGED_AI_URL));
        }
    }

    @Override
    public void onDestroy() {
        if (mPrefChangeRegistrar != null) {
            mPrefChangeRegistrar.destroy();
            mPrefChangeRegistrar = null;
        }
        super.onDestroy();
    }

    private void ensureFineLocationPermissionGranted() {
        if (ContextCompat.checkSelfPermission(
                        getContext(), Manifest.permission.ACCESS_FINE_LOCATION)
                != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[] {Manifest.permission.ACCESS_FINE_LOCATION}, 1);
        }
    }

    /**
     * Sets up a switch preference by syncing its state between the UI, Android SharedPreferences,
     * and the Profile PrefService.
     *
     * <p>This helper:
     *
     * <ol>
     *   <li>Finds the preference in the hierarchy.
     *   <li>Initializes its state from the Profile preference (native).
     *   <li>Syncs that value to the Android SharedPreferences (Java).
     *   <li>Updates both preference locations when the UI value changes.
     *   <li>Listens for changes in the Profile preference to update the UI and SharedPreferences.
     * </ol>
     *
     * @param preferenceKey The key used to find the preference in the XML layout.
     * @param sharedPreferenceKey The key for local Android SharedPreferences.
     * @param profilePreferenceKey The key for the native Profile PrefService.
     * @param extraListener Optional extra listener to handle preference changes.
     * @return The configured preference.
     */
    private <T extends ChromeSwitchPreference> T setupSwitchPreference(
            String preferenceKey,
            String sharedPreferenceKey,
            String profilePreferenceKey,
            @Nullable OnPreferenceChangeListener extraListener) {

        T preference = assertNonNull(findPreference(preferenceKey));
        // Note: We are always using the profile preference over the java shared preference manager.
        // This could be changed if the conflict handling is decided later.
        PrefService prefService = UserPrefs.get(getProfile());
        boolean value = prefService.getBoolean(profilePreferenceKey);
        mSharedPreferencesManager.writeBoolean(sharedPreferenceKey, value);

        preference.setChecked(value);
        preference.setOnPreferenceChangeListener(
                (pref, newValue) -> {
                    boolean boolValue = (boolean) newValue;
                    mSharedPreferencesManager.writeBoolean(sharedPreferenceKey, boolValue);
                    prefService.setBoolean(profilePreferenceKey, boolValue);
                    if (extraListener != null) {
                        return extraListener.onPreferenceChange(pref, newValue);
                    }

                    return true;
                });

        if (mPrefChangeRegistrar != null) {
            mPrefChangeRegistrar.addObserver(
                    profilePreferenceKey,
                    () -> {
                        boolean newValue = prefService.getBoolean(profilePreferenceKey);
                        if (preference.isChecked() != newValue) {
                            preference.setChecked(newValue);
                            mSharedPreferencesManager.writeBoolean(sharedPreferenceKey, newValue);
                        }
                    });
        }

        return preference;
    }

    @Override
    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        if (requestCode != 1) return;

        boolean granted =
                grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED;
        if (granted) return;

        ChromeSwitchPreference locationPref = findPreference(PERMISSION_LOCATION);
        if (locationPref != null) {
            locationPref.setChecked(false);
        }
        UserPrefs.get(getProfile()).setBoolean(GlicPrefNames.GLIC_GEOLOCATION_ENABLED, false);
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
