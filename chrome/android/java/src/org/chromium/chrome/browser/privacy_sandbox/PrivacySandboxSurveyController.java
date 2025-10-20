// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;

import androidx.annotation.IntDef;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.collect.ImmutableMap;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.version_info.Channel;
import org.chromium.base.version_info.VersionConstants;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.function.Supplier;

/** Class that controls and manages when and if surveys should be shown. */
@NullMarked
public class PrivacySandboxSurveyController {
    // LINT.IfChange(PrivacySandboxSurveyTypes)
    /** List of all the survey types that this controller manages. */
    @IntDef({
        PrivacySandboxSurveyType.UNKNOWN,
        PrivacySandboxSurveyType.SENTIMENT_SURVEY,
        PrivacySandboxSurveyType.MAX_VALUE,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PrivacySandboxSurveyType {
        // Default survey type if we don't survey type not explicitly defined.
        int UNKNOWN = 0;
        // Represents the always on sentiment survey.
        int SENTIMENT_SURVEY = 1;
        int MAX_VALUE = 2;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/privacy/enums.xml:PrivacySandboxSurveyTypesEnums)

    // Maps {@link PrivacySandboxSurveyType} to their survey triggerid.
    private static final Map<Integer, String> sSurveyTriggers =
            ImmutableMap.<Integer, String>builder()
                    .put(
                            PrivacySandboxSurveyType.SENTIMENT_SURVEY,
                            "privacy-sandbox-sentiment-survey")
                    .build();

    private @Nullable ActivityTabTabObserver mActivityTabTabObserver;
    private final Activity mActivity;
    private final @Nullable ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private PropertyModel mMessage;
    private final TabModelSelector mTabModelSelector;
    private final MessageDispatcher mMessageDispatcher;
    private final Profile mProfile;
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

    public static @Nullable PrivacySandboxSurveyController initialize(
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

    private @Nullable SurveyClient constructSurveyClient(@PrivacySandboxSurveyType int survey) {
        String triggerId = sSurveyTriggers.get(survey);
        assert triggerId != null;
        SurveyConfig surveyConfig = SurveyConfig.get(mProfile, triggerId);
        if (surveyConfig == null) {
            emitInvalidSurveyConfigHistogram(survey);
            return null;
        }

        // TODO(crbug.com/453007852): When ObservableSupplier<E> extends Supplier<@Nullable E>,
        // remove cast to Supplier<@Nullable Boolean>,
        MessageSurveyUiDelegate messageDelegate =
                new MessageSurveyUiDelegate(
                        mMessage,
                        mMessageDispatcher,
                        mTabModelSelector,
                        (Supplier<@Nullable Boolean>)
                                SurveyClientFactory.getInstance()
                                        .getCrashUploadPermissionSupplier());
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
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
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
        IdentityManager identityManager =
                assumeNonNull(IdentityServicesProvider.get().getIdentityManager(mProfile));
        psb.put("Signed in", identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN));
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

    private static void emitInvalidSurveyConfigHistogram(@PrivacySandboxSurveyType int surveyType) {
        switch (surveyType) {
            case PrivacySandboxSurveyType.SENTIMENT_SURVEY:
                recordSentimentSurveyStatus(
                        PrivacySandboxSentimentSurveyStatus.INVALID_SURVEY_CONFIG);
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
