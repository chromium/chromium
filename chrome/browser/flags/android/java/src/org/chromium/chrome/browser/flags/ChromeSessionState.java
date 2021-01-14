// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import org.chromium.base.annotations.NativeMethods;

/**
 * Stores high-level state about a session for metrics logging.
 */
public class ChromeSessionState {
    /**
     * Records the current custom tab visibility state with native-side feature utilities.
     * @param visible Whether a custom tab is visible.
     */
    public static void setCustomTabVisible(boolean visible) {
        ChromeSessionStateJni.get().setCustomTabVisible(visible);
    }

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

    @NativeMethods
    interface Natives {
        void setCustomTabVisible(boolean visible);
        void setActivityType(@ActivityType int type);
        void setIsInMultiWindowMode(boolean isInMultiWindowMode);
    }
}
