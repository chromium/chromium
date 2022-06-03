// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.base.SysUtils;
import org.chromium.chrome.R;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Provides a set of utilities to help with working with Activities. */
public final class ActivityUtils {
    /** Constant used to express a missing or null resource id. */
    public static final int NO_RESOURCE_ID = -1;

    /**
     * Looks up the Activity of the given web contents. This can be null. Should never be cached,
     * because web contents can change activities, e.g., when user selects "Open in Chrome" menu
     * item.
     *
     * @param webContents The web contents for which to lookup the Activity.
     * @return Activity currently related to webContents. Could be <c>null</c> and could change,
     *         therefore do not cache.
     */
    @Nullable
    public static Activity getActivityFromWebContents(@Nullable WebContents webContents) {
        if (webContents == null || webContents.isDestroyed()) return null;

        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return null;

        Activity activity = window.getActivity().get();
        return activity;
    }

    /** @return the theme ID to use. */
    public static int getThemeId() {
        boolean useLowEndTheme = SysUtils.isLowEndDevice();
        return (useLowEndTheme ? R.style.Theme_Chromium_WithWindowAnimation_LowEnd
                               : R.style.Theme_Chromium_WithWindowAnimation);
    }

    /**
     * Returns whether the activity is finishing or destroyed.
     * @param activity The activity to check.
     * @return Whether the activity is finishing or destroyed. Also returns true if the activity is
     *         null.
     */
    public static boolean isActivityFinishingOrDestroyed(Activity activity) {
        if (activity == null) return true;
        return activity.isDestroyed() || activity.isFinishing();
    }
}