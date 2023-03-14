// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omaha.VersionNumber;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineSettings;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.version_info.VersionInfo;

import java.util.concurrent.TimeUnit;

/**
 * Class that prompts the user to change their search engine at the browser startup.
 * User is only meant to be prompted once, hence the fact of prompting is saved to preferences.
 */
public final class SearchEngineChoiceNotification {
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
    private static class NotificationSnackbarController implements SnackbarController {
        @NonNull
        private final Context mContext;
        @NonNull
        private final SettingsLauncher mSettingsLauncher;

        private NotificationSnackbarController(
                @NonNull Context context, @NonNull SettingsLauncher settingsLauncher) {
            mContext = context;
            mSettingsLauncher = settingsLauncher;
        }

        @Override
        public void onAction(Object actionData) {
            mSettingsLauncher.launchSettingsActivity(mContext, SearchEngineSettings.class);
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
     * @param settingsLauncher Launcher of settings activity.
     */
    public static void handleSearchEngineChoice(@NonNull Context context,
            @NonNull SnackbarManager snackbarManager, @NonNull SettingsLauncher settingsLauncher) {
        boolean searchEngineChoiceRequested = wasSearchEngineChoiceRequested();
        boolean searchEngineChoicePresented = wasSearchEngineChoicePresented();
        boolean searchEngineChoiceAvailable =
                !TemplateUrlServiceFactory.getForProfile(Profile.getLastUsedRegularProfile())
                         .isDefaultSearchManaged();

        if (searchEngineChoiceRequested && searchEngineChoiceAvailable
                && !searchEngineChoicePresented) {
            snackbarManager.showSnackbar(buildSnackbarNotification(context, settingsLauncher));
            updateSearchEngineChoicePresented();
            SearchEngineChoiceMetrics.recordEvent(SearchEngineChoiceMetrics.Events.SNACKBAR_SHOWN);
        } else {
            if (SearchEngineChoiceMetrics.recordSearchEngineTypeAfterChoice()) {
                SearchEngineChoiceMetrics.recordEvent(
                        SearchEngineChoiceMetrics.Events.SEARCH_ENGINE_CHANGED);
            }
        }
    }

    private static Snackbar buildSnackbarNotification(
            @NonNull Context context, @NonNull SettingsLauncher settingsLauncher) {
        int durationSeconds = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.ANDROID_SEARCH_ENGINE_CHOICE_NOTIFICATION,
                PARAM_NOTIFICATION_SNACKBAR_DURATION_SECONDS,
                PARAM_NOTIFICATION_SNACKBAR_DURATION_SECONDS_DEFAULT);

        return Snackbar
                .make(context.getString(R.string.search_engine_choice_prompt),
                        new NotificationSnackbarController(context, settingsLauncher),
                        Snackbar.TYPE_NOTIFICATION, Snackbar.UMA_SEARCH_ENGINE_CHOICE_NOTIFICATION)
                .setAction(context.getString(R.string.settings), null)
                .setDuration((int) TimeUnit.SECONDS.toMillis(durationSeconds))
                .setSingleLine(false)
                .setTheme(Snackbar.Theme.GOOGLE);
    }

    private static void updateSearchEngineChoiceRequested() {
        long now = System.currentTimeMillis();
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_REQUESTED_TIMESTAMP, now);
    }

    private static boolean wasSearchEngineChoiceRequested() {
        return SharedPreferencesManager.getInstance().contains(
                ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_REQUESTED_TIMESTAMP);
    }

    private static void updateSearchEngineChoicePresented() {
        String productVersion = VersionInfo.getProductVersion();
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_PRESENTED_VERSION, productVersion);
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
        return VersionNumber.fromString(SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_PRESENTED_VERSION, null));
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
