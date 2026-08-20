// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appearance.settings;

import android.content.Context;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
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
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils.BookmarkBarSettingChangeOrigin;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceUtil;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.bookmarks.BookmarkBarVisibilityState;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefChangeRegistrar.PrefObserver;

/** Fragment to manage bookmarks bar settings. */
@NullMarked
public class BookmarkBarSettingsFragment extends ChromeBaseSettingsFragment
        implements CustomDividerFragment {

    public static final String PREF_BOOKMARK_BAR = "bookmark_bar";
    @VisibleForTesting static final String PREF_MANAGED_DISCLAIMER_TEXT = "managed_disclaimer_text";

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();
    private boolean mUseProfileUserPrefs;
    private @Nullable RadioButtonGroupBookmarkBarPreference mBookmarkBarPref;

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
            mPrefChangeRegistrar.removeObserver(Pref.BOOKMARK_BAR_VISIBILITY_STATE);
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
        mBookmarkBarPref = findPreference(PREF_BOOKMARK_BAR);
        if (mBookmarkBarPref == null) return;

        ManagedPreferenceDelegate managedPrefDelegate = createManagedPreferenceDelegate();
        mBookmarkBarPref.setManagedPreferenceDelegate(managedPrefDelegate);

        Preference disclaimerPref = findPreference(PREF_MANAGED_DISCLAIMER_TEXT);
        if (disclaimerPref != null) {
            disclaimerPref.setVisible(
                    managedPrefDelegate.isPreferenceControlledByPolicy(mBookmarkBarPref));
        }

        if (mUseProfileUserPrefs) {
            initBookmarkBarPrefForUserPrefs();
        } else {
            initBookmarkBarPrefForDevicePreference();
        }
        updateBookmarkBarPref();
    }

    private ChromeManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return new ChromeManagedPreferenceDelegate(getProfile()) {
            // If true, helper methods in ManagedPreferencesUtils will disable the preference
            // and display the text "Managed by your organization" with the business icon.
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                return BookmarkBarUtils.isUserPrefsBookmarkBarVisibilityStateManagedByPolicy(
                        getProfile());
            }

            // If true, helper methods in ManagedPreferencesUtils will display the text
            // "Recommended by your organization" with the business icon.
            @Override
            public @Nullable Boolean isPreferenceRecommendation(Preference preference) {
                if (!BookmarkBarUtils.isUserPrefsBookmarkBarVisibilityStateRecommended(
                        getProfile())) {
                    // No recommendation exists.
                    return null;
                }

                // On tablets, we use device-specific SharedPreferences. This requires
                // special treatment for enterprise policies; which are UserPrefs bound. If
                // a user has set a SharedPreference value, we must compare that value to
                // the UserPrefs policy's recommended value directly.
                if (BookmarkBarUtils.hasUserSetDevicePrefBookmarkBarVisibilityState()) {
                    return BookmarkBarUtils.getDevicePrefBookmarkBarVisibilityState(getProfile())
                            == BookmarkBarUtils
                                    .getUserPrefsBookmarkBarVisibilityStateRecommendedValue(
                                            getProfile());
                }

                // In the user has not set a SharedPreferences value (which is always the
                // case on Desktop), we can simply follow the standard UserPrefs flow.
                return BookmarkBarUtils
                        .isUserPrefsBookmarkBarVisibilityStateFollowingRecommendation(getProfile());
            }
        };
    }

    private void initBookmarkBarPrefForUserPrefs() {
        mPrefChangeRegistrar = PrefServiceUtil.createFor(getProfile());
        mPrefObserver = this::updateBookmarkBarPref;

        mPrefChangeRegistrar.addObserver(Pref.BOOKMARK_BAR_VISIBILITY_STATE, mPrefObserver);
        if (mBookmarkBarPref != null) {
            mBookmarkBarPref.setOnPreferenceChangeListener(
                    (pref, newValue) -> {
                        @BookmarkBarVisibilityState int state = (int) newValue;
                        BookmarkBarUtils.setUserPrefsBookmarkBarVisibilityState(
                                getProfile(),
                                state,
                                BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS);
                        return true;
                    });
        }
    }

    @SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
    private void initBookmarkBarPrefForDevicePreference() {
        mDevicePrefsListener =
                (sharedPreferences, key) -> {
                    if (key != null
                            && key.equals(
                                    BookmarkBarConstants
                                            .BOOKMARK_BAR_BOOKMARK_BAR_VISIBILITY_STATE)) {
                        updateBookmarkBarPref();
                    }
                };
        ContextUtils.getAppSharedPreferences()
                .registerOnSharedPreferenceChangeListener(mDevicePrefsListener);

        if (mBookmarkBarPref != null) {
            mBookmarkBarPref.setOnPreferenceChangeListener(
                    (pref, newValue) -> {
                        @BookmarkBarVisibilityState int state = (int) newValue;
                        BookmarkBarUtils.setDevicePrefBookmarkBarVisibilityState(
                                state, BookmarkBarSettingChangeOrigin.APPEARANCE_SETTINGS);
                        return true;
                    });
        }
    }

    private void updateBookmarkBarPref() {
        if (mBookmarkBarPref == null) {
            return;
        }

        @BookmarkBarVisibilityState
        int state =
                mUseProfileUserPrefs
                        ? BookmarkBarUtils.getUserPrefsBookmarkBarVisibilityState(getProfile())
                        : BookmarkBarUtils.getDevicePrefBookmarkBarVisibilityState(getProfile());
        mBookmarkBarPref.setCheckedState(state);
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
