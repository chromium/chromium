// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.user_prefs.UserPrefs;

/** Utility class for Glic related functions. */
@NullMarked
public class GlicUtils {
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
}
