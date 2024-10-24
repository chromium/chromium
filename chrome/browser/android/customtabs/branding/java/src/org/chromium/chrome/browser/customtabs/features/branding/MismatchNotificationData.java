// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import java.util.HashMap;

/** Collection of app data storing the information on sign-in prompt UI for a specific profile. */
public class MismatchNotificationData {
    /** How the sign-in prompt UI was closed. */
    @IntDef({
        UserAction.INVALID,
        UserAction.ACCEPTED,
        UserAction.DISMISSED,
        UserAction.TIMED_OUT,
    })
    public @interface UserAction {
        int INVALID = 0;
        int ACCEPTED = 1;
        int DISMISSED = 2;
        int TIMED_OUT = 3;
        int MAX_VALUE = 3;
    }

    /** Per-app prompt UI data */
    public static class AppUiData {
        /** How many times the UI was shown to user. */
        public int showCount;

        /** How the UI was closed. */
        public @UserAction int closeType;

        @Override
        public boolean equals(Object o) {
            if (o instanceof AppUiData data) {
                return data.showCount == showCount && data.closeType == closeType;
            }
            return false;
        }

        public boolean isEmpty() {
            return showCount == 0 && closeType == UserAction.INVALID;
        }
    }

    /** App ID -> App prompt UI data */
    private final HashMap<String, AppUiData> mUiData = new HashMap<>();

    /**
     * @param appId ID for the app in interest.
     * @return {@link AppUiData} for a given app. If not present, A new empty object is created and
     *     returned.
     */
    public @NonNull AppUiData getForApp(String appId) {
        AppUiData res = mUiData.get(appId);
        return res != null ? res : new AppUiData();
    }

    /**
     * @param appId ID for the app in interest.
     * @param data {@link AppUiData} for a given app.
     */
    public void setForApp(String appId, @NonNull AppUiData data) {
        mUiData.put(appId, data);
    }
}
