// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;

/** Interface for classes whose instances can provide data for indexing. */
@NullMarked
public interface SearchIndexProvider {
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
     * programmatically.
     *
     * @param context The {@link Context} used to access application resources.
     * @param indexData The central {@link SettingsIndexData} object to be populated.
     */
    default void updateDynamicPreferences(Context context, SettingsIndexData indexData) {}
}
