// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
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
    public static @Nullable Activity getActivityFromWebContents(@Nullable WebContents webContents) {
        if (webContents == null || webContents.isDestroyed()) return null;

        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return null;

        Activity activity = window.getActivity().get();
        return activity;
    }

    /** @return the theme ID to use. */
    public static int getThemeId() {
        boolean useLowEndTheme = SysUtils.isLowEndDevice();
        return (useLowEndTheme
                ? R.style.Theme_Chromium_WithWindowAnimation_LowEnd
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

    /**
     * Specify the proper non-.Main-aliased Chrome Activity for the given component.
     *
     * @param intent The intent to set the component for.
     * @param component The client generated component to be validated.
     */
    public static void setNonAliasedComponentForMainBrowsingActivity(
            Intent intent, ComponentName component) {
        assert component != null;
        Context appContext = ContextUtils.getApplicationContext();
        if (!TextUtils.equals(component.getPackageName(), appContext.getPackageName())) {
            return;
        }
        if (component.getClassName() != null
                && TextUtils.equals(
                        component.getClassName(),
                        ChromeTabbedActivity.MAIN_LAUNCHER_ACTIVITY_NAME)) {
            // Keep in sync with the activities that the .Main alias points to in
            // AndroidManifest.xml.
            intent.setClass(appContext, ChromeTabbedActivity.class);
        } else {
            intent.setComponent(component);
        }
    }
}
