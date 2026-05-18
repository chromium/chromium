// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.WeakHashMap;

/** Utility class for Glic-related UI actions. */
@NullMarked
public class GlicHelper {
    @IntDef({Caller.SETTINGS_ACTIVITY, Caller.SNACKBAR_ACTIVITY, Caller.NEW_TAB_PAGE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Caller {
        int SETTINGS_ACTIVITY = 0;
        int SNACKBAR_ACTIVITY = 1;
        int NEW_TAB_PAGE = 2;
    }

    private static final WeakHashMap<SnackbarManageable, Boolean> sShownSnackbars =
            new WeakHashMap<>();

    /**
     * Shows a snackbar if there are any active Glic tasks for the given profile.
     *
     * @param snackbarManageable The activity or component that can manage snackbars.
     * @param profile The current profile.
     * @param context The Android context.
     * @param caller The caller context invoking this method.
     */
    public static void maybeShowGlicTaskInProgressSnackbar(
            SnackbarManageable snackbarManageable,
            Profile profile,
            Context context,
            @Caller int caller) {
        boolean limitFrequency = caller != Caller.NEW_TAB_PAGE;
        if (limitFrequency && sShownSnackbars.containsKey(snackbarManageable)) {
            return;
        }

        if (maybeShowGlicTaskInProgressSnackbarInternal(snackbarManageable, profile, context)) {
            if (limitFrequency) {
                sShownSnackbars.put(snackbarManageable, true);
            }
        }
    }

    private static boolean maybeShowGlicTaskInProgressSnackbarInternal(
            SnackbarManageable snackbarManageable, Profile profile, Context context) {
        if (profile.isOffTheRecord()) return false;

        ActorKeyedService actorService = ActorKeyedServiceFactory.getForProfile(profile);
        if (actorService != null && actorService.getActiveTasksCount() > 0) {
            snackbarManageable
                    .getSnackbarManager()
                    .showSnackbar(
                            Snackbar.make(
                                            context.getString(R.string.glic_actor_task_in_progress),
                                            null,
                                            Snackbar.TYPE_NOTIFICATION,
                                            Snackbar.UMA_UNKNOWN)
                                    .setDuration(SnackbarManager.DEFAULT_SNACKBAR_DURATION_MS));
            return true;
        }
        return false;
    }
}
