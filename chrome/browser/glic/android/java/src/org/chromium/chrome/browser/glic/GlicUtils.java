// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.side_panel.AndroidSidePanelEnabledFn;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.user_prefs.UserPrefs;

/** Utility class for Glic related functions. */
@NullMarked
public class GlicUtils {
    /**
     * Returns whether the Glic button is supported on the tab strip for the given profile.
     *
     * @param profile The {@link Profile} to check.
     * @return True if the button is supported on the tab strip.
     */
    public static boolean isTabStripGlicSupported(@Nullable Profile profile) {
        return profile != null
                && GlicEnabling.isEnabledForProfile(profile)
                && AndroidSidePanelEnabledFn.isEnabled();
    }

    /**
     * Returns whether the Glic button is pinned to the tab strip.
     *
     * @param profile The current {@link Profile}.
     * @return True if the button is pinned.
     */
    public static boolean isButtonPinnedToTabStrip(Profile profile) {
        return UserPrefs.get(profile).getBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP);
    }

    /**
     * Sets whether the Glic button is pinned to the tab strip.
     *
     * @param profile The current {@link Profile}.
     * @param isPinned Whether to pin the button.
     */
    public static void setButtonPinnedToTabStrip(Profile profile, boolean isPinned) {
        UserPrefs.get(profile).setBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP, isPinned);
    }

    /**
     * Determines if a tab is contextually eligible to show the Glic IPH.
     *
     * @param tab The current {@link Tab}.
     * @return True if the tab is eligible for showing Glic IPH.
     */
    public static boolean isTabEligibleForGlicIph(@Nullable Tab tab) {
        return tab != null
                && GlicEnabling.isEnabledForProfile(tab.getProfile())
                && !tab.isOffTheRecord()
                && UrlUtilities.isHttpOrHttps(tab.getUrl());
    }
}
