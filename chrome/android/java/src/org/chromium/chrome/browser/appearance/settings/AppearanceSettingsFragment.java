// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appearance.settings;

import android.content.Context;
import android.os.Bundle;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarSettingProvider;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils;
import org.chromium.chrome.browser.night_mode.NightModeMetrics.ThemeSettingsEntry;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/** Fragment to manage appearance settings. */
@NullMarked
public class AppearanceSettingsFragment extends ChromeBaseSettingsFragment
        implements CustomDividerFragment {

    public static final String PREF_BOOKMARK_BAR = "bookmark_bar";
    public static final String PREF_TOOLBAR_SHORTCUT = "toolbar_shortcut";
    public static final String PREF_UI_THEME = "ui_theme";

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    private @Nullable BookmarkBarSettingProvider mBookmarkBarSettingProvider;

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mPageTitle.set(getTitle(getContext()));
        SettingsUtils.addPreferencesFromResource(this, R.xml.appearance_preferences);

        initBookmarkBarPref();
        initToolbarShortcutPref();
        initUiThemePref();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        if (mBookmarkBarSettingProvider != null) {
            mBookmarkBarSettingProvider.destroy();
            mBookmarkBarSettingProvider = null;
        }
    }

    @Override
    public void onStart() {
        super.onStart();
        updatePreferences();
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    public static String getTitle(Context context) {
        return context.getString(R.string.appearance_settings);
    }

    // CustomDividerFragment implementation.

    @Override
    public boolean hasDivider() {
        return false;
    }

    // Private methods.

    private void initBookmarkBarPref() {
        if (!BookmarkBarUtils.isFeatureEnabled(getContext())) {
            removePreference(PREF_BOOKMARK_BAR);
            return;
        }

        ((ChromeSwitchPreference) findPreference(PREF_BOOKMARK_BAR))
                .setOnPreferenceChangeListener(
                        (pref, newValue) -> {
                            BookmarkBarUtils.setSettingEnabled(getProfile(), (boolean) newValue);
                            return true;
                        });

        mBookmarkBarSettingProvider =
                new BookmarkBarSettingProvider(
                        getProfile(), /* callback= */ this::updateBookmarkBarPref);
    }

    private void initToolbarShortcutPref() {
        // LINT.IfChange(InitPrefToolbarShortcut)
        new AdaptiveToolbarStatePredictor(
                        getContext(),
                        getProfile(),
                        /* androidPermissionDelegate= */ null,
                        /* behavior= */ null)
                .recomputeUiState(
                        uiState -> {
                            // Don't show toolbar shortcut settings if disabled from finch.
                            if (!uiState.canShowUi) removePreference(PREF_TOOLBAR_SHORTCUT);
                        });
        // LINT.ThenChange(//chrome/android/java/src/org/chromium/chrome/browser/settings/MainSettings.java:InitPrefToolbarShortcut)
    }

    private void initUiThemePref() {
        // LINT.IfChange(InitPrefUiTheme)
        findPreference(PREF_UI_THEME)
                .getExtras()
                .putInt(
                        ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY,
                        ThemeSettingsEntry.SETTINGS);
        // LINT.ThenChange(//chrome/android/java/src/org/chromium/chrome/browser/settings/MainSettings.java:InitPrefUiTheme)
    }

    private void removePreference(String prefKey) {
        getPreferenceScreen().removePreference(findPreference(prefKey));
    }

    private void updatePreferences() {
        updateBookmarkBarPref(BookmarkBarUtils.isSettingEnabled(getProfile()));
        updateUiThemePref();
    }

    private void updateBookmarkBarPref(boolean isSettingEnabled) {
        if (BookmarkBarUtils.isFeatureEnabled(getContext())) {
            ((ChromeSwitchPreference) findPreference(PREF_BOOKMARK_BAR))
                    .setChecked(isSettingEnabled);
        }
    }

    private void updateUiThemePref() {
        findPreference(PREF_UI_THEME)
                .setSummary(
                        NightModeUtils.getThemeSettingTitle(
                                getContext(), NightModeUtils.getThemeSetting()));
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }
}
