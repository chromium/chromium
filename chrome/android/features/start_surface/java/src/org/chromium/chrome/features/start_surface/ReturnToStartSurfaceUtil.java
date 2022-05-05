// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;

/**
 * This is a utility class for managing the start surface when returning to Chrome.
 */
public final class ReturnToStartSurfaceUtil {
    /** Preference to indicate whether explore pane was visible last in the start surface.*/
    private static final String START_SURFACE_PREF_FILE_NAME = "start_surface";
    private static final String EXPLORE_SURFACE_VISIBLE_LAST = "explore_surface_visible_last";

    private ReturnToStartSurfaceUtil() {}

    /**
     * Determine if we should show the explore surface on returning to Chrome.
     * @return whether the last pane shown in StartSurface was explore
     */
    public static boolean shouldShowExploreSurface() {
        return getSharedPreferences().getBoolean(EXPLORE_SURFACE_VISIBLE_LAST, false);
    }

    /**
     * Preserve whether last active pane in the start surface was the explore surface.
     * @param isVisible whether the explore surface is visible
     */
    public static void setExploreSurfaceVisibleLast(boolean isVisible) {
        getSharedPreferences().edit().putBoolean(EXPLORE_SURFACE_VISIBLE_LAST, isVisible).apply();
    }

    private static SharedPreferences getSharedPreferences() {
        // On some versions of Android, creating the Preferences object involves a disk read (to
        // check if the Preferences directory exists, not even to read the actual Preferences).
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return ContextUtils.getApplicationContext().getSharedPreferences(
                    START_SURFACE_PREF_FILE_NAME, Context.MODE_PRIVATE);
        }
    }
}
