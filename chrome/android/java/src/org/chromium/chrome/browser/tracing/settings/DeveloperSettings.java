// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tracing.settings;

import android.content.Context;
import android.os.Bundle;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.version_info.Channel;
import org.chromium.base.version_info.VersionConstants;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

/** Settings fragment containing preferences aimed at Chrome and web developers. */
@NullMarked
public class DeveloperSettings extends ChromeBaseSettingsFragment
        implements EmbeddableSettingsPage {
    private static final String UI_PREF_BETA_STABLE_HINT = "beta_stable_hint";

    // Non-translated strings:
    private static final String MSG_DEVELOPER_OPTIONS_TITLE = "Developer options";
    private final NonNullObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createNonNull(MSG_DEVELOPER_OPTIONS_TITLE);

    private static @Nullable Boolean sIsEnabledForTests;

    public static boolean shouldShowDeveloperSettings() {
        // Always enabled on canary, dev and local builds, otherwise can be enabled by tapping the
        // Chrome version in Settings>About multiple times.
        if (sIsEnabledForTests != null) return sIsEnabledForTests;

        if (VersionConstants.CHANNEL <= Channel.DEV) return true;
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.SETTINGS_DEVELOPER_ENABLED, false);
    }

    public static void setDeveloperSettingsEnabled() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.SETTINGS_DEVELOPER_ENABLED, true);
    }

    public static void setIsEnabledForTests(Boolean isEnabled) {
        sIsEnabledForTests = isEnabled;
        ResettersForTesting.register(() -> sIsEnabledForTests = null);
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.developer_preferences);

        if (shouldRemoveBetaStableHint()) {
            getPreferenceScreen().removePreference(findPreference(UI_PREF_BETA_STABLE_HINT));
        }
    }

    private static boolean shouldRemoveBetaStableHint() {
        return VersionInfo.isBetaBuild() || VersionInfo.isStableBuild();
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }

    @Override
    public @Nullable String getMainMenuKey() {
        return "developer";
    }

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(
                    DeveloperSettings.class.getName(), R.xml.developer_preferences) {

                @Override
                public void updateDynamicPreferences(Context context, SettingsIndexData indexData) {
                    if (DeveloperSettings.shouldRemoveBetaStableHint()) {
                        indexData.removeEntry(getUniqueId(UI_PREF_BETA_STABLE_HINT));
                    }
                }
            };
}
