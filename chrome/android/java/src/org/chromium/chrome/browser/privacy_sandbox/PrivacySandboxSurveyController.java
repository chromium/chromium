// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.hats.MessageSurveyUiDelegate;
import org.chromium.chrome.browser.ui.hats.SurveyClient;
import org.chromium.chrome.browser.ui.hats.SurveyClientFactory;
import org.chromium.chrome.browser.ui.hats.SurveyConfig;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Collections;

/** Class that controls and manages when and if surveys should be shown. */
public class PrivacySandboxSurveyController {
    private static final String SENTIMENT_SURVEY_TRIGGER = "privacy-sandbox-sentiment-survey";
    private ActivityTabTabObserver mActivityTabTabObserver;
    private Activity mActivity;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private PropertyModel mMessage;
    private TabModelSelector mTabModelSelector;
    private MessageDispatcher mMessageDispatcher;
    private Profile mProfile;
    private PrivacySandboxSurveyBridge mPrivacySandboxSurveyBridge;
    private boolean mHasSeenNtp;
    private static boolean sEnableForTesting;

    PrivacySandboxSurveyController(
            TabModelSelector tabModelSelector,
            @Nullable ActivityLifecycleDispatcher lifecycleDispatcher,
            Activity activity,
            MessageDispatcher messageDispatcher,
            ActivityTabProvider activityTabProvider,
            Profile profile) {
        mHasSeenNtp = false;
        mActivity = activity;
        mActivityLifecycleDispatcher = lifecycleDispatcher;
        mTabModelSelector = tabModelSelector;
        mMessageDispatcher = messageDispatcher;
        mProfile = profile;
        mPrivacySandboxSurveyBridge = new PrivacySandboxSurveyBridge(mProfile);

        setSurveyMessageToDefault();
        createTabObserver(activityTabProvider);
    }

    public void destroy() {
        if (mActivityTabTabObserver != null) {
            mActivityTabTabObserver.destroy();
            mActivityTabTabObserver = null;
        }
    }

    public static PrivacySandboxSurveyController initialize(
            TabModelSelector tabModelSelector,
            @Nullable ActivityLifecycleDispatcher lifecycleDispatcher,
            Activity activity,
            MessageDispatcher messageDispatcher,
            ActivityTabProvider activityTabProvider,
            Profile profile) {
        // Do not create the client for testing unless explicitly enabled.
        if (BuildConfig.IS_FOR_TEST && !sEnableForTesting) {
            return null;
        }
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_SENTIMENT_SURVEY)) {
            return null;
        }
        return new PrivacySandboxSurveyController(
                tabModelSelector,
                lifecycleDispatcher,
                activity,
                messageDispatcher,
                activityTabProvider,
                profile);
    }

    private SurveyClient constructSurveyClient(String triggerId) {
        SurveyConfig config = SurveyConfig.get(triggerId);
        if (config == null) {
            return null;
        }
        assert SurveyClientFactory.getInstance() != null;

        MessageSurveyUiDelegate messageDelegate =
                new MessageSurveyUiDelegate(
                        mMessage,
                        mMessageDispatcher,
                        mTabModelSelector,
                        SurveyClientFactory.getInstance().getCrashUploadPermissionSupplier());

        SurveyClient surveyClient =
                SurveyClientFactory.getInstance().createClient(config, messageDelegate, mProfile);
        return surveyClient;
    }

    private void setSurveyMessageToDefault() {
        mMessage =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.CHROME_SURVEY)
                        .build();
        MessageSurveyUiDelegate.populateDefaultValuesForSurveyMessage(
                mActivity.getResources(), mMessage);
    }

    private void maybeLaunchSurvey() {
        SurveyClient sentimentSurveyClient = constructSurveyClient(SENTIMENT_SURVEY_TRIGGER);
        if (sentimentSurveyClient == null) {
            return;
        }

        sentimentSurveyClient.showSurvey(
                mActivity,
                mActivityLifecycleDispatcher,
                mPrivacySandboxSurveyBridge.getPrivacySandboxSentimentSurveyPsb(),
                Collections.emptyMap());
    }

    private void createTabObserver(ActivityTabProvider activityTabProvider) {
        mActivityTabTabObserver =
                new ActivityTabTabObserver(activityTabProvider) {
                    @Override
                    protected void onObservingDifferentTab(Tab tab) {
                        if (tab == null) {
                            return;
                        }
                        if (UrlUtilities.isNtpUrl(tab.getUrl())) {
                            if (mHasSeenNtp) {
                                maybeLaunchSurvey();
                            }
                            mHasSeenNtp = true;
                        }
                    }
                };
    }

    /** Set whether to trigger the start up survey in tests. */
    public static void setEnableForTesting() {
        sEnableForTesting = true;
        ResettersForTesting.register(() -> sEnableForTesting = false);
    }
}
