// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.hats.MessageSurveyUiDelegate;
import org.chromium.chrome.browser.ui.hats.SurveyClient;
import org.chromium.chrome.browser.ui.hats.SurveyClientFactory;
import org.chromium.chrome.browser.ui.hats.SurveyConfig;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Class that controls if and when to show surveys. One instance of this class is associated with
 * one trigger ID, which is used to fetch a survey, at the time it is created.
 */
public class ChromeSurveyController {
    private static final String TRIGGER_STARTUP_SURVEY = "startup_survey";
    private static boolean sForceUmaEnabledForTesting;
    private static boolean sEnableForTesting;

    private final SurveyClient mSurveyClient;

    ChromeSurveyController(SurveyClient client) {
        mSurveyClient = client;
    }

    private void showSurvey(Activity activity, ActivityLifecycleDispatcher lifecycleDispatcher) {
        mSurveyClient.showSurvey(activity, lifecycleDispatcher);
    }

    /**
     * Checks if the conditions to show the survey are met and starts the process if they are.
     *
     * @param tabModelSelector The tab model selector to access the tab on which the survey will be
     *     shown.
     * @param lifecycleDispatcher Dispatcher for activity lifecycle events.
     * @param activity The {@link Activity} on which the survey will be shown.
     * @param messageDispatcher The {@link MessageDispatcher} for displaying messages.
     */
    public static ChromeSurveyController initialize(
            TabModelSelector tabModelSelector,
            @Nullable ActivityLifecycleDispatcher lifecycleDispatcher,
            Activity activity,
            MessageDispatcher messageDispatcher,
            Profile profile) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SURVEY_NEXT_ANDROID)) return null;

        // Do not create the client for testing unless explicitly enabled.
        if (BuildConfig.IS_FOR_TEST && !sEnableForTesting) {
            return null;
        }

        SurveyConfig config = SurveyConfig.get(TRIGGER_STARTUP_SURVEY);
        if (config == null) return null;

        assert SurveyClientFactory.getInstance() != null;

        PropertyModel message =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.CHROME_SURVEY)
                        .build();
        MessageSurveyUiDelegate.populateDefaultValuesForSurveyMessage(
                activity.getResources(), message);
        MessageSurveyUiDelegate messageDelegate =
                new MessageSurveyUiDelegate(
                        message,
                        messageDispatcher,
                        tabModelSelector,
                        ChromeSurveyController::isUMAEnabled);
        SurveyClient client =
                SurveyClientFactory.getInstance().createClient(config, messageDelegate, profile);
        if (client == null) return null;

        ChromeSurveyController chromeSurveyController = new ChromeSurveyController(client);
        chromeSurveyController.showSurvey(activity, lifecycleDispatcher);
        return chromeSurveyController;
    }

    /** @return Whether metrics and crash dumps are enabled. */
    private static boolean isUMAEnabled() {
        return sForceUmaEnabledForTesting
                || PrivacyPreferencesManagerImpl.getInstance().isUsageAndCrashReportingPermitted();
    }

    /** Set whether UMA consent is granted during tests. Reset to "false" after tests. */
    public static void forceIsUMAEnabledForTesting(boolean forcedUMAStatus) {
        sForceUmaEnabledForTesting = forcedUMAStatus;
        ResettersForTesting.register(() -> sForceUmaEnabledForTesting = false);
    }

    /** Set whether to trigger the start up survey in tests. */
    public static void setEnableForTesting() {
        sEnableForTesting = true;
        ResettersForTesting.register(() -> sEnableForTesting = false);
    }
}
