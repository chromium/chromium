// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.preference.PreferenceScreen;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

/** A Fragment to display recently searched (and chosen) settings. */
@NullMarked
public class RecentSearchesFragment extends SearchResultsPreferenceFragment {
    private @Nullable Runnable mDeleteCallback;

    void setDeleteCallback(Runnable r) {
        mDeleteCallback = r;
    }

    @Override
    protected void buildPreferences(PreferenceScreen screen) {
        var context = requireContext();
        var prefGroup = new RecentSearchesPreferenceCategory(context);
        prefGroup.setIconSpaceReserved(false);
        prefGroup.setTitle(context.getString(R.string.search_in_settings_recent_title));
        screen.addPreference(prefGroup);
        prefGroup.setOnActionClickListener(
                v -> {
                    if (mDeleteCallback != null) mDeleteCallback.run();
                });
        for (SettingsIndexData.Entry info : assumeNonNull(mPreferenceData)) {
            addPreference(screen, info);
        }
    }
}
