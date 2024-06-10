// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.flags.ActivityType;

/**
 * Utility class for mapping browser activity types to privacy sandbox storage activity types.
 *
 * <p>This class provides a bridge between the general {@link ActivityType} used to describe browser
 * activities (tabs, WebAPKs, etc.) and the more specific {@link PrivacySandboxStorageActivityType}
 * used for tracking privacy-related data.
 */
public class ActivityTypeMapper {
    /** Represents an invalid or unsupported activity type. */
    public static final int INVALID_ACTIVITY_TYPE = -1;

    /**
     * Maps a browser activity type to a corresponding privacy sandbox storage activity type.
     *
     * <p>Takes into account special cases like pre-first-tab activities and custom tabs from the
     * Google Search App (AGSA). If no direct mapping is found, returns -1.
     *
     * @param activityType The browser activity type to map from {@link ActivityType}.
     * @param intentDataProvider Provides data about the intent that started the activity.
     * @return The corresponding privacy sandbox storage activity type, or -1 if no mapping is
     *     found.
     */
    public static int toPrivacySandboxStorageActivityType(
            @ActivityType int activityType, BrowserServicesIntentDataProvider intentDataProvider) {
        if (activityType == ActivityType.PRE_FIRST_TAB) {
            return INVALID_ACTIVITY_TYPE;
        }

        if (activityType == ActivityType.CUSTOM_TAB) {
            if (intentDataProvider != null
                    && intentDataProvider.getClientPackageName() != null
                    && intentDataProvider
                            .getClientPackageName()
                            .equals("com.google.android.googlequicksearchbox")
                    && !intentDataProvider.isPartialCustomTab()) {
                return PrivacySandboxStorageActivityType.AGSA_CUSTOM_TAB;
            } else if (intentDataProvider != null && intentDataProvider.isPartialCustomTab()) {
                return INVALID_ACTIVITY_TYPE;
            } else {
                return PrivacySandboxStorageActivityType.NON_AGSA_CUSTOM_TAB;
            }
        }

        return toPrivacySandboxStorageActivityType(activityType);
    }

    /**
     * Maps a browser activity type to a corresponding privacy sandbox storage activity type.
     *
     * @param activityType The browser activity type to map from {@link ActivityType}.
     * @return The corresponding privacy sandbox storage activity type, or -1 if no mapping is
     *     found.
     */
    public static int toPrivacySandboxStorageActivityType(@ActivityType int activityType) {
        switch (activityType) {
            case ActivityType.TRUSTED_WEB_ACTIVITY -> {
                return PrivacySandboxStorageActivityType.TRUSTED_WEB_ACTIVITY;
            }
            case ActivityType.WEBAPP -> {
                return PrivacySandboxStorageActivityType.WEBAPP;
            }
            case ActivityType.WEB_APK -> {
                return PrivacySandboxStorageActivityType.WEB_APK;
            }
            case ActivityType.TABBED -> {
                return PrivacySandboxStorageActivityType.TABBED;
            }
            default -> {
                return INVALID_ACTIVITY_TYPE;
            }
        }
    }
}
