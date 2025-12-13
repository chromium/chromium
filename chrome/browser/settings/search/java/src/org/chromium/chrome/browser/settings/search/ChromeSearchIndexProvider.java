// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.search.SearchIndexProvider;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

/** Interface for Chrome preferences that have data for indexing. */
@NullMarked
public interface ChromeSearchIndexProvider extends SearchIndexProvider {
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
