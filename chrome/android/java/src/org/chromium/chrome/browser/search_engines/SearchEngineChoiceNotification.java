// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.omaha.VersionNumber;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.SearchEnginePreference;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;

import java.util.concurrent.TimeUnit;

/**
 * Class that prompts the user to change their search engine at the browser startup.
 * User is only meant to be propmpted once, hence the fact of prompting is saved to preferences.
 */
public final class SearchEngineChoiceNotification {
    /** Key used to store the date of when search engine choice was requested. */
    static final String PREF_SEARCH_ENGINE_CHOICE_REQUESTED_TIMESTAMP =
            "search_engine_choice_requested_timestamp";

    /** Key used to store the version of Chrome in which the choice was presented. */
    static final String PREF_SEARCH_ENGINE_CHOICE_PRESENTED_VERSION =
            "search_engine_choice_presented_version";

    /** Variations parameter name for notification snackbar duration (in seconds). */
    private static final String PARAM_NOTIFICATION_SNACKBAR_DURATION_SECONDS =
            "notification-snackbar-duration-seconds";

    /** Default value for notification snackbar duration (in seconds). */
    private static final int PARAM_NOTIFICATION_SNACKBAR_DURATION_SECONDS_DEFAULT = 10;

    /** Variations parameter name for invalidating version number. */
    private static final String PARAM_NOTIFICATION_INVALIDATING_VERSION_NUMBER =
            "notification-invalidating-version-number";

    /**
     * Snackbar controller for search engine choice notification. It takes the user to the settings
     * page responsible for search engine choice, when button is clicked.
     */
    public static class NotificationSnackbarController implements SnackbarController {
        private Context mContext;

        private NotificationSnackbarController(Context context) {
            mContext = context;
        }

        @Override
        public void onAction(Object actionData) {
            PreferencesLauncher.launchSettingsPage(mContext, SearchEnginePreference.class);
            SearchEngineChoiceMetrics.recordEvent(SearchEngineChoiceMetrics.Events.PROMPT_FOLLOWED);
            SearchEngineChoiceMetrics.recordSearchEngineTypeBeforeChoice();
        }
    }

    private SearchEngineChoiceNotification() {}

    /**
     * When called for the first time, it will save a preference that search engine choice was
     * requested.
     */
    public static void receiveSearchEngineChoiceRequest() {
        if (wasSearchEngineChoiceRequested()) return;

        updateSearchEngineChoiceRequested();
    }

    /**
     * Shows a search engine change notification, in form of a Snackbar. When run for the first time
     * after showing a prompt, it reports metrics about Search Engine change.
     *
     * @param context Context in which to show the Snackbar.
     * @param snackbarManager Snackbar manager which will shown and manage the Snackbar.
     */
    public static void handleSearchEngineChoice(Context context, SnackbarManager snackbarManager) {
        boolean searchEngineChoiceRequested = wasSearchEngineChoiceRequested();
        boolean searchEngineChoicePresented = wasSearchEngineChoicePresented();

        if (searchEngineChoiceRequested && !searchEngineChoicePresented) {
            snackbarManager.showSnackbar(buildSnackbarNotification(context));
            updateSearchEngineChoicePresented();
            SearchEngineChoiceMetrics.recordEvent(SearchEngineChoiceMetrics.Events.SNACKBAR_SHOWN);
        } else {
            if (SearchEngineChoiceMetrics.recordSearchEngineTypeAfterChoice()) {
                SearchEngineChoiceMetrics.recordEvent(
                        SearchEngineChoiceMetrics.Events.SEARCH_ENGINE_CHANGED);
            }
        }
    }

    private static Snackbar buildSnackbarNotification(Context context) {
        int durationSeconds = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.ANDROID_SEARCH_ENGINE_CHOICE_NOTIFICATION,
                PARAM_NOTIFICATION_SNACKBAR_DURATION_SECONDS,
                PARAM_NOTIFICATION_SNACKBAR_DURATION_SECONDS_DEFAULT);

        return Snackbar
                .make(context.getString(R.string.search_engine_choice_prompt),
                        new NotificationSnackbarController(context), Snackbar.TYPE_NOTIFICATION,
                        Snackbar.UMA_SEARCH_ENGINE_CHOICE_NOTIFICATION)
                .setAction(context.getString(R.string.preferences), null)
                .setDuration((int) TimeUnit.SECONDS.toMillis(durationSeconds))
                .setSingleLine(false)
                .setTheme(Snackbar.Theme.GOOGLE);
    }

    private static void updateSearchEngineChoiceRequested() {
        long now = System.currentTimeMillis();
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putLong(PREF_SEARCH_ENGINE_CHOICE_REQUESTED_TIMESTAMP, now)
                .apply();
    }

    private static boolean wasSearchEngineChoiceRequested() {
        return ContextUtils.getAppSharedPreferences().contains(
                PREF_SEARCH_ENGINE_CHOICE_REQUESTED_TIMESTAMP);
    }

    private static void updateSearchEngineChoicePresented() {
        String productVersion = ChromeVersionInfo.getProductVersion();
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putString(PREF_SEARCH_ENGINE_CHOICE_PRESENTED_VERSION, productVersion)
                .apply();
    }

    private static boolean wasSearchEngineChoicePresented() {
        VersionNumber lastPresentedVersionNumber = getLastPresentedVersionNumber();
        if (lastPresentedVersionNumber == null) return false;

        VersionNumber lowestAcceptedVersionNumber = getLowestAcceptedVersionNumber();
        if (lowestAcceptedVersionNumber == null) return true;

        return !lastPresentedVersionNumber.isSmallerThan(lowestAcceptedVersionNumber);
    }

    @Nullable
    private static VersionNumber getLastPresentedVersionNumber() {
        return VersionNumber.fromString(ContextUtils.getAppSharedPreferences().getString(
                PREF_SEARCH_ENGINE_CHOICE_PRESENTED_VERSION, null));
    }

    @Nullable
    private static VersionNumber getLowestAcceptedVersionNumber() {
        return VersionNumber.fromString(ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.ANDROID_SEARCH_ENGINE_CHOICE_NOTIFICATION,
                PARAM_NOTIFICATION_INVALIDATING_VERSION_NUMBER));
    }

    private static int getNotificationSnackbarDuration() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.ANDROID_SEARCH_ENGINE_CHOICE_NOTIFICATION,
                PARAM_NOTIFICATION_SNACKBAR_DURATION_SECONDS,
                PARAM_NOTIFICATION_SNACKBAR_DURATION_SECONDS_DEFAULT);
    }
}
