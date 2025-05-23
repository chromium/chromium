// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.app.Activity;

import androidx.annotation.IntDef;
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

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Random;

/** Class that controls and manages when and if surveys should be shown. */
public class PrivacySandboxSurveyController {
    // LINT.IfChange(PrivacySandboxSurveyTypes)
    /** List of all the survey types that this controller manages. */
    @IntDef({
        PrivacySandboxSurveyType.UNKNOWN,
        PrivacySandboxSurveyType.SENTIMENT_SURVEY,
        PrivacySandboxSurveyType.CCT_EEA_ACCEPTED,
        PrivacySandboxSurveyType.CCT_EEA_DECLINED,
        PrivacySandboxSurveyType.CCT_EEA_CONTROL,
        PrivacySandboxSurveyType.CCT_ROW_ACKNOWLEDGED,
        PrivacySandboxSurveyType.CCT_ROW_CONTROL,
        PrivacySandboxSurveyType.MAX_VALUE,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PrivacySandboxSurveyType {
        // Default survey type if we don't surey type not explicitly defined.
        int UNKNOWN = 0;
        // Represents the always on sentiment survey.
        int SENTIMENT_SURVEY = 1;
        // Represents the surveys for the Ads CCT notice.
        int CCT_EEA_ACCEPTED = 2;
        int CCT_EEA_DECLINED = 3;
        int CCT_EEA_CONTROL = 4;
        int CCT_ROW_ACKNOWLEDGED = 5;
        int CCT_ROW_CONTROL = 6;

        int MAX_VALUE = 7;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/privacy/enums.xml:PrivacySandboxSurveyTypesEnums)

    // LINT.IfChange(PrivacySandboxCctAdsNoticeSurveyFailures)
    /** Represents the possible failures when attempting to surface a CCT ads notice survey. */
    @IntDef({
        CctAdsNoticeSurveyFailures.FEATURE_NOT_ENABLED,
        CctAdsNoticeSurveyFailures.APP_ID_MISMATCH,
        CctAdsNoticeSurveyFailures.USER_INTERACTION_NOT_FOUND,
        CctAdsNoticeSurveyFailures.INVALID_PROMPT_TYPE,
        CctAdsNoticeSurveyFailures.INVALID_EEA_ACCEPTED_SURVEY_CONFIG,
        CctAdsNoticeSurveyFailures.INVALID_EEA_DECLINED_SURVEY_CONFIG,
        CctAdsNoticeSurveyFailures.INVALID_EEA_CONTROL_SURVEY_CONFIG,
        CctAdsNoticeSurveyFailures.INVALID_ROW_ACKNOWLEDGED_SURVEY_CONFIG,
        CctAdsNoticeSurveyFailures.INVALID_ROW_CONTROL_SURVEY_CONFIG,
        CctAdsNoticeSurveyFailures.MAX_VALUE,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface CctAdsNoticeSurveyFailures {
        // Feature was disabled.
        int FEATURE_NOT_ENABLED = 0;
        // App-id value did not match.
        int APP_ID_MISMATCH = 1;
        // No interaction was found for a client in the treatment group.
        // Those in the treatment group should have seen and interacted
        // with a consent/notice before we attempt to surface a survey.
        int USER_INTERACTION_NOT_FOUND = 2;
        // We received an invalid prompt type for a client in the control group.
        int INVALID_PROMPT_TYPE = 3;
        // Invalid survey config.
        int INVALID_EEA_ACCEPTED_SURVEY_CONFIG = 4;
        int INVALID_EEA_DECLINED_SURVEY_CONFIG = 5;
        int INVALID_EEA_CONTROL_SURVEY_CONFIG = 6;
        int INVALID_ROW_ACKNOWLEDGED_SURVEY_CONFIG = 7;
        int INVALID_ROW_CONTROL_SURVEY_CONFIG = 8;
        int MAX_VALUE = INVALID_ROW_CONTROL_SURVEY_CONFIG;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/privacy/enums.xml:PrivacySandboxCctAdsNoticeSurveyFailures)

    // Maps {@link PrivacySandboxSurveyType} to their survey triggerid.
    private static final Map<Integer, String> sSurveyTriggers =
            ImmutableMap.<Integer, String>builder()
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
    private final Activity mActivity;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private PropertyModel mMessage;
    private final TabModelSelector mTabModelSelector;
    private final MessageDispatcher mMessageDispatcher;
    private final Profile mProfile;
    private boolean mHasSeenNtp;
    private boolean mOverrideChannelForTesting;
    private int mChannelForTesting;
    private static final int DEFAULT_ADS_CCT_DELAY_MS = 20_000;

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
            recordCctAdsNoticeSurveyFailures(CctAdsNoticeSurveyFailures.FEATURE_NOT_ENABLED);
            return false;
        }
        String paramAdsNoticeAppId =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY, "survey-app-id");
        if (!paramAdsNoticeAppId.isEmpty() && !paramAdsNoticeAppId.equals(appId)) {
            recordCctAdsNoticeSurveyFailures(CctAdsNoticeSurveyFailures.APP_ID_MISMATCH);
            return false;
        }
        return true;
    }

    @VisibleForTesting
    public long getAdsCctDelayMilliseconds() {
        // Use the 20 second default if the conversion of the parameter fails.
        return Long.valueOf(
                ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY,
                        "survey-delay-ms",
                        DEFAULT_ADS_CCT_DELAY_MS));
    }

    // Attempts to schedule the launch of an Ads CCT Treatment survey.
    // Should only be invoked after the closure of either the EEA or ROW notice.
    public void maybeScheduleAdsCctTreatmentSurveyLaunch(String appId) {
        if (!shouldLaunchAdsCctSurvey(appId)) {
            return;
        }
        PostTask.postDelayedTask(
                TaskTraits.UI_DEFAULT,
                () -> maybeLaunchAdsCctTreatmentSurvey(),
                getAdsCctDelayMilliseconds());
    }

    // Does a local random roll to determine if a EEA survey should be shown based on the trigger
    // rate
    private boolean isSelectedForEeaSurvey(@PrivacySandboxSurveyType int surveyType) {
        switch (surveyType) {
            case PrivacySandboxSurveyType.CCT_EEA_ACCEPTED:
                return new Random().nextFloat()
                        < ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                                ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY,
                                "accepted-trigger-rate",
                                0.0);
            case PrivacySandboxSurveyType.CCT_EEA_DECLINED:
                return new Random().nextFloat()
                        < ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                                ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY,
                                "declined-trigger-rate",
                                0.0);
            default:
                return false;
        }
    }

    // Determines the appropriate survey to launch based on the user interaction with either the EEA
    // consent or the ROW notice and launches the survey.
    private void maybeLaunchAdsCctTreatmentSurvey() {
        @PrivacySandboxSurveyType int surveyType = PrivacySandboxSurveyType.UNKNOWN;
        PrefService prefs = UserPrefs.get(mProfile);
        // Check if the EEA consent was shown.
        if (prefs.getBoolean(Pref.PRIVACY_SANDBOX_M1_CONSENT_DECISION_MADE)) {
            if (prefs.getBoolean(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED)) {
                surveyType = PrivacySandboxSurveyType.CCT_EEA_ACCEPTED;
            } else {
                surveyType = PrivacySandboxSurveyType.CCT_EEA_DECLINED;
            }
            if (!isSelectedForEeaSurvey(surveyType)) {
                return;
            }
            // Check if the ROW notice was acknowledged.
        } else if (prefs.getBoolean(Pref.PRIVACY_SANDBOX_M1_ROW_NOTICE_ACKNOWLEDGED)) {
            surveyType = PrivacySandboxSurveyType.CCT_ROW_ACKNOWLEDGED;
        }

        if (surveyType == PrivacySandboxSurveyType.UNKNOWN) {
            recordCctAdsNoticeSurveyFailures(CctAdsNoticeSurveyFailures.USER_INTERACTION_NOT_FOUND);
            return;
        }
        showSurvey(surveyType);
    }

    // Attempts to schedule the launch of an Ads CCT control survey.
    // Clients expected to see a control survey will not see any Ads CCT dialogs.
    public void maybeScheduleAdsCctControlSurveyLaunch(String appId, @PromptType int promptType) {
        if (!shouldLaunchAdsCctSurvey(appId)) {
            return;
        }
        PostTask.postDelayedTask(
                TaskTraits.UI_DEFAULT,
                () -> maybeLaunchAdsCctControlSurvey(promptType),
                getAdsCctDelayMilliseconds());
    }

    // Determines the appropriate survey to launch based on the prompt type.
    private void maybeLaunchAdsCctControlSurvey(@PromptType int promptType) {
        switch (promptType) {
            case PromptType.M1_CONSENT:
                // Case where we expected the client to see the EEA consent.
                showSurvey(PrivacySandboxSurveyType.CCT_EEA_CONTROL);
                break;
            case PromptType.M1_NOTICE_ROW:
                // Case where we expected the client to see the ROW notice.
                showSurvey(PrivacySandboxSurveyType.CCT_ROW_CONTROL);
                break;
            case PromptType.NONE:
            case PromptType.M1_NOTICE_EEA:
            case PromptType.M1_NOTICE_RESTRICTED:
                recordCctAdsNoticeSurveyFailures(CctAdsNoticeSurveyFailures.INVALID_PROMPT_TYPE);
                return;
        }
    }

    private SurveyClient constructSurveyClient(@PrivacySandboxSurveyType int survey) {
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
                        .createClient(surveyConfig, messageDelegate, mProfile, mTabModelSelector);
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

    private void showSurvey(@PrivacySandboxSurveyType int surveyType) {
        recordSurveySurfaceAttempted(surveyType);
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

    private Map<String, Boolean> populateSurveyPsb(@PrivacySandboxSurveyType int surveyType) {
        if (surveyType == PrivacySandboxSurveyType.SENTIMENT_SURVEY) {
            return getSentimentSurveyPsb();
        }
        return Collections.emptyMap();
    }

    private Map<String, String> populateSurveyPsd(@PrivacySandboxSurveyType int surveyType) {
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
        // Ads CCT notice survey.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY)) {
            return true;
        }
        // Sentiment survey should be checked last as it should always be on.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_SENTIMENT_SURVEY)) {
            recordSentimentSurveyStatus(PrivacySandboxSentimentSurveyStatus.FEATURE_DISABLED);
            return false;
        }

        return true;
    }

    private static void recordSentimentSurveyStatus(
            @PrivacySandboxSentimentSurveyStatus int status) {
        RecordHistogram.recordEnumeratedHistogram(
                "PrivacySandbox.SentimentSurvey.Status",
                status,
                PrivacySandboxSentimentSurveyStatus.MAX_VALUE);
    }

    private static void recordCctAdsNoticeSurveyFailures(@CctAdsNoticeSurveyFailures int failure) {
        RecordHistogram.recordEnumeratedHistogram(
                "PrivacySandbox.Surveys.CctAdsNoticeSurvey.Failures",
                failure,
                CctAdsNoticeSurveyFailures.MAX_VALUE);
    }

    private static void emitInvalidSurveyConfigHistogram(@PrivacySandboxSurveyType int surveyType) {
        switch (surveyType) {
            case PrivacySandboxSurveyType.SENTIMENT_SURVEY:
                recordSentimentSurveyStatus(
                        PrivacySandboxSentimentSurveyStatus.INVALID_SURVEY_CONFIG);
                return;
            case PrivacySandboxSurveyType.CCT_EEA_ACCEPTED:
                recordCctAdsNoticeSurveyFailures(
                        CctAdsNoticeSurveyFailures.INVALID_EEA_ACCEPTED_SURVEY_CONFIG);
                return;
            case PrivacySandboxSurveyType.CCT_EEA_DECLINED:
                recordCctAdsNoticeSurveyFailures(
                        CctAdsNoticeSurveyFailures.INVALID_EEA_DECLINED_SURVEY_CONFIG);
                return;
            case PrivacySandboxSurveyType.CCT_EEA_CONTROL:
                recordCctAdsNoticeSurveyFailures(
                        CctAdsNoticeSurveyFailures.INVALID_EEA_CONTROL_SURVEY_CONFIG);
                return;
            case PrivacySandboxSurveyType.CCT_ROW_ACKNOWLEDGED:
                recordCctAdsNoticeSurveyFailures(
                        CctAdsNoticeSurveyFailures.INVALID_ROW_ACKNOWLEDGED_SURVEY_CONFIG);
                return;
            case PrivacySandboxSurveyType.CCT_ROW_CONTROL:
                recordCctAdsNoticeSurveyFailures(
                        CctAdsNoticeSurveyFailures.INVALID_ROW_CONTROL_SURVEY_CONFIG);
                return;
            default:
                return;
        }
    }

    private static void recordSurveySurfaceAttempted(@PrivacySandboxSurveyType int surveyType) {
        RecordHistogram.recordEnumeratedHistogram(
                "PrivacySandbox.Surveys.SurfaceAttempts",
                surveyType,
                PrivacySandboxSurveyType.MAX_VALUE);
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
