// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import android.content.Context;
import android.os.UserHandle;
import android.os.UserManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;

import java.util.List;

/** Stores high-level state about a session for metrics logging. */
public class ChromeSessionState {
    /**
     * Records whether the activity is in multi-window mode with native-side feature utilities.
     * @param isInMultiWindowMode Whether the activity is in Android N multi-window mode.
     */
    public static void setIsInMultiWindowMode(boolean isInMultiWindowMode) {
        ChromeSessionStateJni.get().setIsInMultiWindowMode(isInMultiWindowMode);
    }

    /**
     * Records the type of the currently visible Activity for metrics.
     * @param activityType The type of the Activity.
     */
    public static void setActivityType(@ActivityType int activityType) {
        ChromeSessionStateJni.get().setActivityType(activityType);
    }

    /**
     * Records the dark mode settings for the current Activity and the system.
     * @param activityIsInDarkMode Whether the current Activity is in dark mode.
     * @param systemIsInDarkMode Whether the phone/tablet is in dark mode.
     */
    public static void setDarkModeState(boolean activityIsInDarkMode, boolean systemIsInDarkMode) {
        boolean activityMatchesSystem = activityIsInDarkMode == systemIsInDarkMode;

        @DarkModeState int darkModeState;
        if (activityIsInDarkMode) {
            if (activityMatchesSystem) {
                darkModeState = DarkModeState.DARK_MODE_SYSTEM;
            } else {
                darkModeState = DarkModeState.DARK_MODE_APP;
            }
        } else {
            if (activityMatchesSystem) {
                darkModeState = DarkModeState.LIGHT_MODE_SYSTEM;
            } else {
                darkModeState = DarkModeState.LIGHT_MODE_APP;
            }
        }
        ChromeSessionStateJni.get().setDarkModeState(darkModeState);
    }

    /** Returns whether Android has multiple user profiles. */
    @CalledByNative
    public static @MultipleUserProfilesState int getMultipleUserProfilesState() {
        UserManager userManager =
                (UserManager)
                        ContextUtils.getApplicationContext().getSystemService(Context.USER_SERVICE);
        List<UserHandle> userHandles = userManager.getUserProfiles();
        assert !userHandles.isEmpty();
        return userHandles.size() > 1
                ? MultipleUserProfilesState.MULTIPLE_PROFILES
                : MultipleUserProfilesState.SINGLE_PROFILE;
    }

    @NativeMethods
    interface Natives {
        void setActivityType(@ActivityType int type);

        void setIsInMultiWindowMode(boolean isInMultiWindowMode);

        void setDarkModeState(@DarkModeState int state);
    }
}
