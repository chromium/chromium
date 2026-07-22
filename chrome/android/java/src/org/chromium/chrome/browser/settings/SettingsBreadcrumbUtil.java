// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;
import android.os.Build;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.search.SettingsSearchCoordinator;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

import java.util.ArrayList;
import java.util.List;

/** Utility class for managing breadcrumb path in settings intents and saved instance state. */
@NullMarked
public class SettingsBreadcrumbUtil {
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static final String KEY_INITIAL_BREADCRUMB_PATH = "initial_breadcrumb_path";

    private SettingsBreadcrumbUtil() {}

    /**
     * Extracts the initial breadcrumb path for a given fragment and its arguments, if present.
     *
     * @param context The context used to build the search index.
     * @param profile The profile used to build the search index.
     * @param fragmentName The class name of the fragment to get breadcrumbs for.
     * @param fragmentArgs The arguments bundle passed to the fragment.
     * @return The list of breadcrumb entries if the path has more than 1 entry, or null otherwise.
     */
    public static @Nullable List<SettingsIndexData.Entry> getInitialBreadcrumbPath(
            Context context,
            Profile profile,
            @Nullable String fragmentName,
            @Nullable Bundle fragmentArgs) {
        if (fragmentName == null) return null;

        SettingsIndexData indexData = SettingsSearchCoordinator.ensureIndexBuilt(context, profile);
        List<SettingsIndexData.Entry> path =
                indexData.getBreadcrumbEntries(fragmentName, fragmentArgs);
        if (path != null && path.size() > 1) {
            return path;
        }
        return null;
    }

    /**
     * Extracts the initial breadcrumb path from the saved instance state, if present.
     *
     * @param savedInstanceState The saved instance state bundle.
     * @return The list of breadcrumb entries, or null if not found.
     */
    public static @Nullable List<SettingsIndexData.Entry> getInitialBreadcrumbPath(
            @Nullable Bundle savedInstanceState) {
        if (savedInstanceState == null) return null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            return savedInstanceState.getParcelableArrayList(
                    KEY_INITIAL_BREADCRUMB_PATH, SettingsIndexData.Entry.class);
        } else {
            @SuppressWarnings("deprecation")
            ArrayList<SettingsIndexData.Entry> legacyList =
                    savedInstanceState.getParcelableArrayList(KEY_INITIAL_BREADCRUMB_PATH);
            return legacyList;
        }
    }

    /**
     * Saves the initial breadcrumb path to the saved instance state bundle, if present.
     *
     * @param outState The bundle in which to place the saved state.
     * @param breadcrumbPath The list of breadcrumb entries to save.
     */
    public static void saveInitialBreadcrumbPath(
            Bundle outState, @Nullable List<SettingsIndexData.Entry> breadcrumbPath) {
        if (breadcrumbPath != null) {
            outState.putParcelableArrayList(
                    KEY_INITIAL_BREADCRUMB_PATH, new ArrayList<>(breadcrumbPath));
        }
    }
}
