// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appearance.settings;

import android.content.Context;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.os.Bundle;

import androidx.preference.Preference;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarConstants;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceUtil;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefChangeRegistrar.PrefObserver;

/** Fragment to manage bookmarks bar settings. */
@NullMarked
public class BookmarkBarSettingsFragment extends ChromeBaseSettingsFragment
        implements CustomDividerFragment {

    public static final String PREF_BOOKMARK_BAR = "bookmark_bar";

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();
    private boolean mUseProfileUserPrefs;

    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;
    private @Nullable PrefObserver mPrefObserver;
    private @Nullable OnSharedPreferenceChangeListener mDevicePrefsListener;

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        assert shouldShowBookmarkPref(getContext())
                : "Bookmark bar subpage should not be visible without flag enabled on certain"
                        + " devices";
        mPageTitle.set(getString(R.string.bookmark_bar_settings_title));
        SettingsUtils.addPreferencesFromResource(this, R.xml.bookmark_bar_preferences);

        mUseProfileUserPrefs = BookmarkBarUtils.shouldUseProfileUserPrefs();
        initBookmarkBarPref();
    }

    @Override
    @SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
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
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public boolean hasDivider() {
        return false;
    }

    private void initBookmarkBarPref() {
        if (!shouldShowBookmarkPref(getContext())) {
            removePreference(PREF_BOOKMARK_BAR);
            return;
        }

        ChromeSwitchPreference bookmarkBarSwitch = findPreference(PREF_BOOKMARK_BAR);
        assert bookmarkBarSwitch != null;
        bookmarkBarSwitch.setManagedPreferenceDelegate(
                new ChromeManagedPreferenceDelegate(getProfile()) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return BookmarkBarUtils.isBookmarkBarManagedByPolicy(getProfile());
                    }

                    @Override
                    public @Nullable Boolean isPreferenceRecommendation(Preference preference) {
                        if (!BookmarkBarUtils.isBookmarkBarRecommended(getProfile())) {
                            return null;
                        }
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
        mPrefObserver = this::updateBookmarkBarPref;

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

    @SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
    private void initBookmarkBarPrefForDevicePreference() {
        mDevicePrefsListener =
                (sharedPreferences, key) -> {
                    if (key != null
                            && key.equals(BookmarkBarConstants.BOOKMARK_BAR_SHOW_BOOKMARK_BAR)) {
                        updateBookmarkBarPref();
                        Preference bookmarkBarSwitch = findPreference(PREF_BOOKMARK_BAR);
                        if (bookmarkBarSwitch != null) {
                            bookmarkBarSwitch.setSummary(bookmarkBarSwitch.getSummary());
                        }
                    }
                };
        ContextUtils.getAppSharedPreferences()
                .registerOnSharedPreferenceChangeListener(mDevicePrefsListener);

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

    private void updateBookmarkBarPref() {
        if (!shouldShowBookmarkPref(getContext())) {
            return;
        }

        ChromeSwitchPreference bookmarkBarSwitch = findPreference(PREF_BOOKMARK_BAR);
        if (bookmarkBarSwitch == null) return;

        if (mUseProfileUserPrefs) {
            bookmarkBarSwitch.setChecked(
                    BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(getProfile()));
        } else {
            bookmarkBarSwitch.setChecked(
                    BookmarkBarUtils.isDevicePrefShowBookmarksBarEnabled(getProfile()));
        }
    }

    private void removePreference(String prefKey) {
        getPreferenceScreen().removePreference(findPreference(prefKey));
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }

    private static boolean shouldShowBookmarkPref(Context context) {
        return AppearanceSettingsFragment.shouldShowBookmarkPref(context)
                && AppearanceSettingsFragment.shouldShowSubpage();
    }

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(
                    BookmarkBarSettingsFragment.class.getName(), R.xml.bookmark_bar_preferences) {
                @Override
                public void updateDynamicPreferences(Context context, SettingsIndexData indexData) {
                    if (!shouldShowBookmarkPref(context)) {
                        indexData.removeEntryForKey(
                                BookmarkBarSettingsFragment.class.getName(), PREF_BOOKMARK_BAR);
                        return;
                    }

                    indexData.updateEntrySummaryForKey(
                            BookmarkBarSettingsFragment.class.getName(),
                            PREF_BOOKMARK_BAR,
                            R.string.bookmark_bar_setting_subtitle);
                }
            };

    @Nullable PrefObserver getPrefObserverForTesting() {
        return mPrefObserver;
    }
}
