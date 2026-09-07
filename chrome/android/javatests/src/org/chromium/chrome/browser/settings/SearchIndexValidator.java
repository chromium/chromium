// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;
import android.text.TextUtils;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceGroup;
import androidx.preference.PreferenceScreen;

import org.junit.Assert;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.search.SettingsSearchCoordinator;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Validates that a Settings screen matches the search index.
 *
 * <p>Callers should use the public API exposed via {@link SettingsSearchTestUtils}.
 */
@NullMarked
class SearchIndexValidator {

    private SearchIndexValidator() {}

    static void assertPreferenceScreenMatchesIndex(PreferenceFragmentCompat fragment) {
        assertPreferenceScreenMatchesIndex(fragment, Set.of());
    }

    static void assertPreferenceScreenMatchesIndex(
            PreferenceFragmentCompat fragment, Set<String> allowlistedKeys) {
        if (!ThreadUtils.runningOnUiThread()) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> assertPreferenceScreenMatchesIndex(fragment, allowlistedKeys));
            return;
        }

        PreferenceScreen preferenceScreen = fragment.getPreferenceScreen();
        Assert.assertNotNull(
                "PreferenceScreen is null on " + fragment.getClass().getName(), preferenceScreen);

        Map<String, Preference> visiblePrefs = new LinkedHashMap<>();
        collectVisiblePreferences(preferenceScreen, visiblePrefs);

        SettingsIndexData indexData = buildIndexData(fragment);
        String fragmentName = fragment.getClass().getName();
        Map<String, SettingsIndexData.Entry> fragmentEntries = new HashMap<>();
        for (SettingsIndexData.Entry entry : indexData.getEntriesForTesting().values()) {
            if (fragmentName.equals(entry.parentFragment)) {
                fragmentEntries.put(entry.key, entry);
            }
        }

        List<String> errors = new ArrayList<>();

        // 1. Forward check: Every visible preference must be present in the search index and marked
        // searchable.
        for (Map.Entry<String, Preference> mapEntry : visiblePrefs.entrySet()) {
            String prefKey = mapEntry.getKey();
            Preference pref = mapEntry.getValue();

            if (allowlistedKeys.contains(prefKey)) {
                continue;
            }

            SettingsIndexData.Entry indexEntry = fragmentEntries.get(prefKey);
            if (indexEntry == null) {
                errors.add(
                        String.format(
                                "Visible preference [key='%s', title='%s'] on %s is missing from"
                                        + " the search index!",
                                prefKey, pref.getTitle(), fragmentName));
            } else if (!indexEntry.isSearchable) {
                errors.add(
                        String.format(
                                "Visible preference [key='%s', title='%s'] on %s is marked"
                                        + " isSearchable=false in the search index!",
                                prefKey, pref.getTitle(), fragmentName));
            } else if (!TextUtils.equals(pref.getTitle(), indexEntry.title)) {
                errors.add(
                        String.format(
                                "Title mismatch for preference [key='%s'] on %s: UI shows"
                                        + " '%s', but search index has '%s'",
                                prefKey, fragmentName, pref.getTitle(), indexEntry.title));
            }
        }

        // 2. Reverse check: Every searchable index entry for this fragment must be visible on the
        // screen.
        for (SettingsIndexData.Entry indexEntry : fragmentEntries.values()) {
            if (allowlistedKeys.contains(indexEntry.key)) {
                continue;
            }
            if (indexEntry.isSearchable && !visiblePrefs.containsKey(indexEntry.key)) {
                errors.add(
                        String.format(
                                "Search index contains searchable entry [key='%s', title='%s']"
                                        + " that is NOT visible on %s!",
                                indexEntry.key, indexEntry.title, fragmentName));
            }
        }

        if (!errors.isEmpty()) {
            Assert.fail(
                    "Settings search index parity failure on "
                            + fragmentName
                            + ":\n  - "
                            + String.join("\n  - ", errors));
        }
    }

    private static SettingsIndexData buildIndexData(PreferenceFragmentCompat fragment) {
        Context context = ContextUtils.getApplicationContext();
        Profile profile =
                fragment instanceof ChromeBaseSettingsFragment chromeBaseSettingsFragment
                        ? chromeBaseSettingsFragment.getProfile()
                        : ProfileManager.getLastUsedRegularProfile();
        SettingsIndexData.reset();
        return SettingsSearchCoordinator.ensureIndexBuilt(context, profile);
    }

    private static void collectVisiblePreferences(
            PreferenceGroup group, Map<String, Preference> visiblePrefs) {
        for (int i = 0; i < group.getPreferenceCount(); i++) {
            Preference pref = group.getPreference(i);
            if (!pref.isVisible() || pref instanceof TextMessagePreference) {
                continue;
            }
            if (pref instanceof PreferenceGroup subGroup) {
                collectVisiblePreferences(subGroup, visiblePrefs);
            } else if (pref.getKey() != null) {
                visiblePrefs.put(pref.getKey(), pref);
            }
        }
    }
}
