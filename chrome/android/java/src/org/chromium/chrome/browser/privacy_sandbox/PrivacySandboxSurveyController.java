// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.app.Activity;

import androidx.annotation.Nullable;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.collect.ImmutableMap;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.version_info.Channel;
import org.chromium.base.version_info.VersionConstants;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
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
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/** Class that controls and manages when and if surveys should be shown. */
public class PrivacySandboxSurveyController {
    private enum PrivacySandboxSurveyType {
        UNKNOWN,
        CCT_EEA_ACCEPTED,
        CCT_EEA_DECLINED,
        CCT_EEA_CONTROL,
        CCT_ROW_ACKNOWLEDGED,
        CCT_ROW_CONTROL,
        SENTIMENT_SURVEY,
    }

    private static final Map<PrivacySandboxSurveyType, String> sSurveyTriggers =
            ImmutableMap.<PrivacySandboxSurveyType, String>builder()
                    .put(
                            PrivacySandboxSurveyType.CCT_EEA_ACCEPTED,
                            "privacy-sandbox-cct-ads-notice-eea-accepted")
                    .put(
                            PrivacySandboxSurveyType.CCT_EEA_DECLINED,
                            "privacy-sandbox-cct-ads-notice-eea-declined")
                    .put(
                            PrivacySandboxSurveyType.CCT_EEA_CONTROL,
                            "privacy-sandbox-cct-ads-notice-eea-control")
                    .put(
                            PrivacySandboxSurveyType.CCT_ROW_ACKNOWLEDGED,
                            "privacy-sandbox-cct-ads-notice-row-acknowledged")
                    .put(
                            PrivacySandboxSurveyType.CCT_ROW_CONTROL,
                            "privacy-sandbox-cct-ads-notice-row-control")
                    .put(
                            PrivacySandboxSurveyType.SENTIMENT_SURVEY,
                            "privacy-sandbox-sentiment-survey")
                    .build();

    private ActivityTabTabObserver mActivityTabTabObserver;
    private Activity mActivity;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private PropertyModel mMessage;
    private TabModelSelector mTabModelSelector;
    private MessageDispatcher mMessageDispatcher;
    private Profile mProfile;
    private boolean mHasSeenNtp;
    private boolean mOverrideChannelForTesting;
    private int mChannelForTesting;
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
        if (!shouldInitializeForActiveStudy()) {
            return null;
        }
        if (profile.isOffTheRecord()) {
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

    private boolean shouldLaunchAdsCctSurvey(String appId) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY)) {
            // TODO(crbug.com/379930582): Emit a histogram detailing that the feature was disabled.
            return false;
        }
        String paramAdsNoticeAppId =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY, "app-id");
        if (!paramAdsNoticeAppId.isEmpty() && !paramAdsNoticeAppId.equals(appId)) {
            // TODO(crbug.com/379930582): Emit a histogram detailing an app-id mismatch.
            return false;
        }
        return true;
    }

    // Schedules the launch of an Ads CCT Treatment survey.
    // Should only be invoked after the closure of either the EEA or ROW notice.
    public void scheduleAdsCctTreatmentSurveyLaunch(String appId) {
        if (!shouldLaunchAdsCctSurvey(appId)) {
            return;
        }
        PostTask.postDelayedTask(
                TaskTraits.UI_DEFAULT,
                () -> maybeLaunchAdsCctTreatmentSurvey(),
                /*20 seconds*/ 20000);
    }

    // Determines the appropriate survey to launch based on the user interaction with either the EEA
    // consent or the ROW notice and launches the survey.
    private void maybeLaunchAdsCctTreatmentSurvey() {
        PrivacySandboxSurveyType surveyType = PrivacySandboxSurveyType.UNKNOWN;
        PrefService prefs = UserPrefs.get(mProfile);
        // Check if the EEA consent was shown.
        if (prefs.getBoolean(Pref.PRIVACY_SANDBOX_M1_CONSENT_DECISION_MADE)) {
            if (prefs.getBoolean(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED)) {
                surveyType = PrivacySandboxSurveyType.CCT_EEA_ACCEPTED;
            } else {
                surveyType = PrivacySandboxSurveyType.CCT_EEA_DECLINED;
            }
            // Check if the ROW notice was acknowledged.
        } else if (prefs.getBoolean(Pref.PRIVACY_SANDBOX_M1_ROW_NOTICE_ACKNOWLEDGED)) {
            surveyType = PrivacySandboxSurveyType.CCT_ROW_ACKNOWLEDGED;
        }

        if (surveyType == PrivacySandboxSurveyType.UNKNOWN) {
            // TODO(crbug.com/379930582): Emit a histogram detailing that somehow the client did not
            // interact with a consent/notice.
            return;
        }
        showSurvey(surveyType);
    }

    private SurveyClient constructSurveyClient(PrivacySandboxSurveyType survey) {
        SurveyConfig surveyConfig = SurveyConfig.get(mProfile, sSurveyTriggers.get(survey));
        if (surveyConfig == null) {
            emitInvalidSurveyConfigHistogram(survey);
            return null;
        }
        MessageSurveyUiDelegate messageDelegate =
                new MessageSurveyUiDelegate(
                        mMessage,
                        mMessageDispatcher,
                        mTabModelSelector,
                        SurveyClientFactory.getInstance().getCrashUploadPermissionSupplier());
        SurveyClient surveyClient =
                SurveyClientFactory.getInstance()
                        .createClient(surveyConfig, messageDelegate, mProfile);
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

    private void showSurvey(PrivacySandboxSurveyType surveyType) {
        SurveyClient surveyClient = constructSurveyClient(surveyType);
        if (surveyClient == null) {
            return;
        }
        surveyClient.showSurvey(
                mActivity,
                mActivityLifecycleDispatcher,
                populateSurveyPsb(surveyType),
                populateSurveyPsd(surveyType));
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
                                showSurvey(PrivacySandboxSurveyType.SENTIMENT_SURVEY);
                            }
                            mHasSeenNtp = true;
                        }
                    }
                };
    }

    private Map<String, Boolean> populateSurveyPsb(PrivacySandboxSurveyType surveyType) {
        if (surveyType == PrivacySandboxSurveyType.SENTIMENT_SURVEY) {
            return getSentimentSurveyPsb();
        }
        return Collections.emptyMap();
    }

    private Map<String, String> populateSurveyPsd(PrivacySandboxSurveyType surveyType) {
        if (surveyType == PrivacySandboxSurveyType.SENTIMENT_SURVEY) {
            return getSentimentSurveyPsd();
        }
        return Collections.emptyMap();
    }

    @VisibleForTesting
    public Map<String, Boolean> getSentimentSurveyPsb() {
        Map<String, Boolean> psb = new HashMap<>();
        PrefService prefs = UserPrefs.get(mProfile);
        psb.put("Topics enabled", prefs.getBoolean(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED));
        psb.put(
                "Protected audience enabled",
                prefs.getBoolean(Pref.PRIVACY_SANDBOX_M1_FLEDGE_ENABLED));
        psb.put(
                "Measurement enabled",
                prefs.getBoolean(Pref.PRIVACY_SANDBOX_M1_AD_MEASUREMENT_ENABLED));
        psb.put(
                "Signed in",
                IdentityServicesProvider.get()
                        .getIdentityManager(mProfile)
                        .hasPrimaryAccount(ConsentLevel.SIGNIN));
        return psb;
    }

    @VisibleForTesting
    public Map<String, String> getSentimentSurveyPsd() {
        Map<String, String> psd = new HashMap<>();
        psd.put("Channel", getChannelName());
        return psd;
    }

    @VisibleForTesting
    public String getChannelName() {
        int channel = VersionConstants.CHANNEL;
        if (mOverrideChannelForTesting) {
            channel = mChannelForTesting;
        }
        switch (channel) {
            case Channel.STABLE:
                return "stable";
            case Channel.BETA:
                return "beta";
            case Channel.DEV:
                return "dev";
            case Channel.CANARY:
                return "canary";
            default:
                return "unknown";
        }
    }

    private static boolean shouldInitializeForActiveStudy() {
        // Sentiment survey should be checked last as it should always be on.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_SENTIMENT_SURVEY)) {
            emitFeatureDisabledHistogram(PrivacySandboxSurveyType.SENTIMENT_SURVEY);
            return false;
        }

        return true;
    }

    private static void recordSentimentSurveyStatus(
            @PrivacySandboxSentimentSurveyStatus int status) {
        RecordHistogram.recordEnumeratedHistogram(
                "PrivacySandbox.SentimentSurvey.Status",
                status,
                PrivacySandboxSentimentSurveyStatus.MAX_VALUE + 1);
    }

    private static void emitInvalidSurveyConfigHistogram(PrivacySandboxSurveyType survey) {
        switch (survey) {
            case SENTIMENT_SURVEY:
                recordSentimentSurveyStatus(
                        PrivacySandboxSentimentSurveyStatus.INVALID_SURVEY_CONFIG);
                return;
                // TODO(crbug.com/379930582): Add support for CCT survey histograms
            default:
                return;
        }
    }

    private static void emitFeatureDisabledHistogram(PrivacySandboxSurveyType survey) {
        switch (survey) {
            case SENTIMENT_SURVEY:
                recordSentimentSurveyStatus(PrivacySandboxSentimentSurveyStatus.FEATURE_DISABLED);
                return;
                // TODO(crbug.com/379930582): Add support for CCT survey histograms
            default:
                return;
        }
    }

    // Set whether to trigger the start up survey in tests.
    public static void setEnableForTesting() {
        sEnableForTesting = true;
        ResettersForTesting.register(() -> sEnableForTesting = false);
    }

    // Set whether we should override the channel for tests.
    public void overrideChannelForTesting() {
        mOverrideChannelForTesting = true;
        ResettersForTesting.register(() -> mOverrideChannelForTesting = false);
    }

    // Set the channel for testing.
    public void setChannelForTesting(int channel) {
        mChannelForTesting = channel;
        ResettersForTesting.register(() -> mChannelForTesting = Channel.DEFAULT);
    }
}
