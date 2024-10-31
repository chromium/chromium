// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import java.util.HashMap;
import java.util.Map;

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

    /**
     * The entire notification data for apps. Nested map of {@link AppUiData} with the account/app
     * ID as a key.
     */
    private final Map<String, Map<String, AppUiData>> mDataMap = new HashMap<>();

    /**
     * Returns the notification data for a given account/app ID. An empty data is returned if the
     * corresponding entry has not been created yet.
     *
     * @param account Account ID
     * @param appId App ID
     */
    public @NonNull AppUiData getAppData(String accountId, String appId) {
        Map<String, AppUiData> accountData = mDataMap.get(accountId);
        AppUiData res = accountData != null ? accountData.get(appId) : null;
        return res != null ? res : new AppUiData();
    }

    /**
     * Stores the notification data for a given account/app ID.
     *
     * @param account Account ID
     * @param appId App ID
     */
    public void setAppData(String accountId, String appId, @NonNull AppUiData data) {
        Map<String, AppUiData> accountData = mDataMap.get(accountId);
        if (accountData != null) {
            accountData.put(appId, data);
        } else {
            // Create a new inner app -> data map if it doesn't exist.
            accountData = new HashMap<String, AppUiData>();
            accountData.put(appId, data);
            mDataMap.put(accountId, accountData);
        }
    }

    public Map<String, Map<String, AppUiData>> getAllForTesting() {
        return mDataMap;
    }
}
