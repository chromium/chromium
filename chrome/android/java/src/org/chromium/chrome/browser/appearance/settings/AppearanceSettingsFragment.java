// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appearance.settings;

import android.content.Context;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.os.Bundle;

import androidx.preference.Preference;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarConstants;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.night_mode.NightModeMetrics.ThemeSettingsEntry;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceUtil;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefChangeRegistrar.PrefObserver;

/** Fragment to manage appearance settings. */
@NullMarked
public class AppearanceSettingsFragment extends ChromeBaseSettingsFragment
        implements CustomDividerFragment {

    public static final String PREF_BOOKMARK_BAR = "bookmark_bar";
    public static final String PREF_TOOLBAR_SHORTCUT = "toolbar_shortcut";
    public static final String PREF_UI_THEME = "ui_theme";

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();
    private boolean mUseProfileUserPrefs;

    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;
    private @Nullable PrefObserver mPrefObserver;
    private @Nullable OnSharedPreferenceChangeListener mDevicePrefsListener;

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mPageTitle.set(getTitle(getContext()));
        SettingsUtils.addPreferencesFromResource(this, R.xml.appearance_preferences);

        // This fragment may be used on Desktop or tablets. For Desktop we use the current Profile's
        // UserPrefs. For tablets, we use the local device preference.
        mUseProfileUserPrefs = DeviceInfo.isDesktop();
        initBookmarkBarPref();
        initToolbarShortcutPref();
        initUiThemePref();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        if (mPrefChangeRegistrar != null) {
            mPrefChangeRegistrar.removeObserver(Pref.SHOW_BOOKMARK_BAR);
            mPrefChangeRegistrar.destroy();
            mPrefChangeRegistrar = null;
        }
        if (mDevicePrefsListener != null) {
            ContextUtils.getAppSharedPreferences()
                    .unregisterOnSharedPreferenceChangeListener(mDevicePrefsListener);
            mDevicePrefsListener = null;
        }
    }

    @Override
    public void onStart() {
        super.onStart();
        updateBookmarkBarPref();
        updateUiThemePref();

        TrackerFactory.getTrackerForProfile(getProfile())
                .notifyEvent(EventConstants.SETTINGS_APPEARANCE_OPENED);
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
        // isDeviceBookmarkBarCompatible already checks the flag sAndroidBookmarkBar.
        if (!BookmarkBarUtils.isDeviceBookmarkBarCompatible(getContext())) {
            removePreference(PREF_BOOKMARK_BAR);
            return;
        }

        // Find the switch preference and attach our policy logic to it.
        ChromeSwitchPreference bookmarkBarSwitch = findPreference(PREF_BOOKMARK_BAR);
        assert bookmarkBarSwitch != null;
        bookmarkBarSwitch.setManagedPreferenceDelegate(
                new ChromeManagedPreferenceDelegate(getProfile()) {
                    // If true, the helper methods in ManagedPreferencesUtils will disable the
                    // switch and show the "managed by your organization"
                    // text with the business icon.
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return BookmarkBarUtils.isBookmarkBarManagedByPolicy(getProfile());
                    }

                    @Override
                    public @Nullable Boolean isPreferenceRecommendation(Preference preference) {
                        if (!BookmarkBarUtils.isBookmarkBarRecommended(getProfile())) {
                            // No recommendation exists.
                            return null;
                        }

                        // Return true if the user's setting matches the recommendation, which
                        // shows the icon & text. Return false if it doesn't match, which hides
                        // the icon & text.
                        return BookmarkBarUtils.isFollowingBookmarkBarRecommendation(getProfile());
                    }
                });

        if (mUseProfileUserPrefs) {
            initBookmarkBarPrefForUserPrefs();
        } else {
            initBookmarkBarPrefForDevicePreference();
        }
    }

    private void initBookmarkBarPrefForUserPrefs() {
        mPrefChangeRegistrar = PrefServiceUtil.createFor(getProfile());
        mPrefObserver =
                () -> {
                    updateBookmarkBarPref();
                    Preference bookmarkBarSwitch = findPreference(PREF_BOOKMARK_BAR);
                    if (bookmarkBarSwitch != null) {
                        // This is the trigger to showing/hiding the
                        // "recommended" icon & text.
                        // Flow: switch is toggled ->
                        // OnPreferenceChangeListener fired ->
                        // #setDevicePrefShowBookmarksBar ->
                        // pref updated in pref service & shared pref ->
                        // OnSharedPreferenceChangeListener fired ->
                        // this #onPreferenceChange is called ->
                        // ManagedPreferencesUtils#onBindViewPreference
                        // -> methods in our setManagedPreferenceDelegate are called.
                        bookmarkBarSwitch.setSummary(bookmarkBarSwitch.getSummary());
                    }
                };

        // We register a pref change listener for a pref that would be changed on this page so that
        // we can account for users changing the pref using a different window in desktop mode.
        mPrefChangeRegistrar.addObserver(Pref.SHOW_BOOKMARK_BAR, mPrefObserver);
        ((ChromeSwitchPreference) findPreference(PREF_BOOKMARK_BAR))
                .setOnPreferenceChangeListener(
                        (pref, newValue) -> {
                            BookmarkBarUtils.setUserPrefsShowBookmarksBar(
                                    getProfile(),
                                    (boolean) newValue,
                                    /* fromKeyboardShortcut= */ false);
                            return true;
                        });
    }

    private void initBookmarkBarPrefForDevicePreference() {
        // Similar to UserPrefs above, we must have both an observer of changes to the device prefs,
        // as well as the ability to set the device prefs via the toggle, since the value can be
        // toggled by another window.
        mDevicePrefsListener =
                (sharedPreferences, key) -> {
                    if (key != null
                            && key.equals(BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR)) {
                        updateBookmarkBarPref();
                        Preference bookmarkBarSwitch = findPreference(PREF_BOOKMARK_BAR);
                        if (bookmarkBarSwitch != null) {
                            // Forces a redraw, and methods in our setManagedPreferenceDelegate are
                            // called.
                            bookmarkBarSwitch.setSummary(bookmarkBarSwitch.getSummary());
                        }
                    }
                };
        ContextUtils.getAppSharedPreferences()
                .registerOnSharedPreferenceChangeListener(mDevicePrefsListener);

        // setOnPreferenceChangeListener is the listener for the preference widget itself. It fires
        // immediately when the user taps the toggle.
        ((ChromeSwitchPreference) findPreference(PREF_BOOKMARK_BAR))
                .setOnPreferenceChangeListener(
                        (pref, newValue) -> {
                            BookmarkBarUtils.setDevicePrefShowBookmarksBar(
                                    getProfile(),
                                    (boolean) newValue,
                                    /* fromKeyboardShortcut= */ false);
                            return true;
                        });
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

    private void updateBookmarkBarPref() {
        // isDeviceBookmarkBarCompatible already checks the flag sAndroidBookmarkBar.
        if (!BookmarkBarUtils.isDeviceBookmarkBarCompatible(getContext())) {
            return;
        }

        if (mUseProfileUserPrefs) {
            ((ChromeSwitchPreference) findPreference(PREF_BOOKMARK_BAR))
                    .setChecked(BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(getProfile()));
        } else {
            ((ChromeSwitchPreference) findPreference(PREF_BOOKMARK_BAR))
                    .setChecked(BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(getProfile()));
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

    @Override
    public @Nullable String getMainMenuKey() {
        return "appearance";
    }

    @Nullable PrefObserver getPrefObserverForTesting() {
        return mPrefObserver;
    }
}
