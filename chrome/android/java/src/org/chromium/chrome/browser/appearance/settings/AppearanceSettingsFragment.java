// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appearance.settings;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.night_mode.NightModeMetrics.ThemeSettingsEntry;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/** Fragment to manage appearance settings. */
public class AppearanceSettingsFragment extends ChromeBaseSettingsFragment
        implements CustomDividerFragment {

    public static final String PREF_TOOLBAR_SHORTCUT = "toolbar_shortcut";
    public static final String PREF_UI_THEME = "ui_theme";

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mPageTitle.set(getTitle(getContext()));
        SettingsUtils.addPreferencesFromResource(this, R.xml.appearance_preferences);

        // LINT.IfChange(InitPrefToolbarShortcut)
        new AdaptiveToolbarStatePredictor(
                        getContext(),
                        getProfile(),
                        /* androidPermissionDelegate= */ null,
                        /* behavior= */ null)
                .recomputeUiState(
                        uiState -> {
                            // Don't show toolbar shortcut settings if disabled from finch.
                            if (!uiState.canShowUi) {
                                getPreferenceScreen()
                                        .removePreference(findPreference(PREF_TOOLBAR_SHORTCUT));
                            }
                        });
        // LINT.ThenChange(//chrome/android/java/src/org/chromium/chrome/browser/settings/MainSettings.java:InitPrefToolbarShortcut)

        // LINT.IfChange(InitPrefUiTheme)
        findPreference(PREF_UI_THEME)
                .getExtras()
                .putInt(
                        ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY,
                        ThemeSettingsEntry.SETTINGS);
        // LINT.ThenChange(//chrome/android/java/src/org/chromium/chrome/browser/settings/MainSettings.java:InitPrefUiTheme)
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferences();
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    public static @NonNull String getTitle(@NonNull Context context) {
        return context.getString(R.string.appearance_settings);
    }

    // CustomDividerFragment implementation.

    @Override
    public boolean hasDivider() {
        return false;
    }

    // Private methods.

    private void updatePreferences() {
        final var context = getContext();
        final var theme = NightModeUtils.getThemeSetting();
        findPreference(PREF_UI_THEME)
                .setSummary(NightModeUtils.getThemeSettingTitle(context, theme));
    }
}
