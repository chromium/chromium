// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.search.SearchIndexProvider;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

import java.util.Map;

/** Interface for Chrome preferences that have data for indexing. */
@NullMarked
public interface ChromeSearchIndexProvider extends SearchIndexProvider {
    /**
     * Populates the search index with the static preferences associated with this provider, using
     * the current profile.
     *
     * @param context The {@link Context} used to access application resources.
     * @param profile The current {@link Profile}.
     * @param indexData The central {@link SettingsIndexData} object to be populated.
     * @param providerMap Map of all registered providers, keyed by Fragment Class Name. Used to
     *     look up default extras for child fragments.
     */
    default void initPreferenceXml(
            Context context,
            Profile profile,
            SettingsIndexData indexData,
            Map<String, SearchIndexProvider> providerMap) {
        initPreferenceXml(context, indexData, providerMap);
    }

    /**
     * Update preferences dynamically with Profile access.
     *
     * @param context The {@link Context} used to access application resources.
     * @param indexData The central {@link SettingsIndexData} object to be populated.
     * @param profile The current {@link Profile}.
     */
    default void updateDynamicPreferences(
            Context context, SettingsIndexData indexData, Profile profile) {
        // Default behavior: Fallback to the component-level method
        // (in case a Chrome fragment doesn't actually need the profile)
        updateDynamicPreferences(context, indexData);
    }
}
