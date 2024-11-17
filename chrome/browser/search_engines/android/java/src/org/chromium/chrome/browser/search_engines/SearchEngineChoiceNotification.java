// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.version_info.VersionInfo;
import org.chromium.chrome.browser.omaha.VersionNumber;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineSettings;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;

import java.util.concurrent.TimeUnit;

/**
 * Class that prompts the user to change their search engine at the browser startup.
 * User is only meant to be prompted once, hence the fact of prompting is saved to preferences.
 */
public final class SearchEngineChoiceNotification {
    /** Notification snackbar duration (in seconds). */
    private static final int NOTIFICATION_SNACKBAR_DURATION_SECONDS = 10;

    /**
     * Snackbar controller for search engine choice notification. It takes the user to the settings
     * page responsible for search engine choice, when button is clicked.
     */
    private static class NotificationSnackbarController implements SnackbarController {
        @NonNull private final Context mContext;

        private NotificationSnackbarController(@NonNull Context context) {
            mContext = context;
        }

        @Override
        public void onAction(Object actionData) {
            SettingsNavigationFactory.createSettingsNavigation()
                    .startSettings(mContext, SearchEngineSettings.class);
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
    public static void handleSearchEngineChoice(
            @NonNull Context context, @NonNull SnackbarManager snackbarManager) {
        boolean searchEngineChoiceRequested = wasSearchEngineChoiceRequested();
        boolean searchEngineChoicePresented = wasSearchEngineChoicePresented();
        boolean searchEngineChoiceAvailable =
                !TemplateUrlServiceFactory.getForProfile(ProfileManager.getLastUsedRegularProfile())
                        .isDefaultSearchManaged();

        if (searchEngineChoiceRequested
                && searchEngineChoiceAvailable
                && !searchEngineChoicePresented) {
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

    private static Snackbar buildSnackbarNotification(@NonNull Context context) {
        return Snackbar.make(
                        context.getString(R.string.search_engine_choice_prompt),
                        new NotificationSnackbarController(context),
                        Snackbar.TYPE_NOTIFICATION,
                        Snackbar.UMA_SEARCH_ENGINE_CHOICE_NOTIFICATION)
                .setAction(context.getString(R.string.settings), null)
                .setDuration(
                        (int) TimeUnit.SECONDS.toMillis(NOTIFICATION_SNACKBAR_DURATION_SECONDS))
                .setSingleLine(false)
                .setTheme(Snackbar.Theme.GOOGLE);
    }

    private static void updateSearchEngineChoiceRequested() {
        long now = System.currentTimeMillis();
        ChromeSharedPreferences.getInstance()
                .writeLong(ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_REQUESTED_TIMESTAMP, now);
    }

    private static boolean wasSearchEngineChoiceRequested() {
        return ChromeSharedPreferences.getInstance()
                .contains(ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_REQUESTED_TIMESTAMP);
    }

    private static void updateSearchEngineChoicePresented() {
        String productVersion = VersionInfo.getProductVersion();
        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_PRESENTED_VERSION,
                        productVersion);
    }

    private static boolean wasSearchEngineChoicePresented() {
        return getLastPresentedVersionNumber() != null;
    }

    private static @Nullable VersionNumber getLastPresentedVersionNumber() {
        return VersionNumber.fromString(
                ChromeSharedPreferences.getInstance()
                        .readString(
                                ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_PRESENTED_VERSION, null));
    }
}
