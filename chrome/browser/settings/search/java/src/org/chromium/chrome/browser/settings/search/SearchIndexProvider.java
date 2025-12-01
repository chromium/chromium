// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.Map;
import java.util.Set;

/** Interface for classes whose instances can provide data for indexing. */
@NullMarked
public interface SearchIndexProvider {
    /** Returns the name of the associated {@link PreferenceFragment}. */
    String getPrefFragmentName();

    /**
     * Registers the fragment headers of the indexed search prefs by setting headers for ones that
     * should be displayed.
     *
     * @param context The application context.
     * @param indexData The data object to be populated.
     * @param providerMap The map of all known providers, keyed by fragment name.
     * @param processedFragments A set of fragment names that have already been processed.
     */
    void registerFragmentHeaders(
            Context context,
            SettingsIndexData indexData,
            Map<String, SearchIndexProvider> providerMap,
            Set<String> processedFragments);

    /**
     * Populates the search index with the static preferences associated with this provider.
     *
     * <p>Complex fragments with dynamic UI must override this method to mirror their UI logic,
     * ensuring only visible preferences are indexed.
     *
     * @param context The {@link Context} used to access application resources.
     * @param indexData The central {@link SettingsIndexData} object to be populated.
     */
    void initPreferenceXml(Context context, SettingsIndexData indexData);

    /**
     * Update the search index with the preference information provided dynamically or
     * programmatically. This is for fragments not relying on the current profile.
     *
     * @param context The {@link Context} used to access application resources.
     * @param indexData The central {@link SettingsIndexData} object to be populated.
     */
    default void updateDynamicPreferences(Context context, SettingsIndexData indexData) {}

    /**
     * Similar to {@link #updateDynamicPreferences(Context, SettingsIndexData)} but takes in the
     * profile. This is for fragments that rely on the profile.
     *
     * @param context The {@link Context} used to access application resources.
     * @param indexData The central {@link SettingsIndexData} object to be populated.
     * @param profile The current {@link Profile}.
     */
    default void updateDynamicPreferences(
            Context context, SettingsIndexData indexData, Profile profile) {
        updateDynamicPreferences(context, indexData);
    }
}
