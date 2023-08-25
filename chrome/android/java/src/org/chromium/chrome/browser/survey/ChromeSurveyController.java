// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.os.Handler;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.ui.hats.SurveyController;
import org.chromium.chrome.browser.ui.hats.SurveyControllerProvider;
import org.chromium.chrome.browser.ui.hats.SurveyThrottler;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * Class that controls if and when to show surveys. One instance of this class is associated with
 * one trigger ID, which is used to fetch a survey, at the time it is created.
 *
 * @see #getTriggerId()
 */
public class ChromeSurveyController {
    private static final String TAG = "ChromeSurveyCtrler";
    @VisibleForTesting
    public static final String COMMAND_LINE_PARAM_NAME = "survey_override_site_id";
    @VisibleForTesting
    static final String MAX_DOWNLOAD_ATTEMPTS = "max-download-attempts";
    @VisibleForTesting
    static final String MAX_NUMBER = "max-number";
    @VisibleForTesting
    static final String SITE_ID_PARAM_NAME = "site-id";

    private static @Nullable SurveyController sControllerForTesting;
    private static boolean sForceUmaEnabledForTesting;
    private static boolean sMessageShown;

    private TabModelSelector mTabModelSelector;
    private Handler mLoggingHandler;
    private Tab mSurveyPromptTab;
    private TabModelSelectorObserver mTabModelObserver;

    private final String mTriggerId;
    private final @Nullable ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final Activity mActivity;
    private final MessageDispatcher mMessageDispatcher;
    private SurveyController mSurveyController;
    private SurveyThrottler mSurveyThrottler;
    private @Nullable TabObserver mTabObserver;
    private @Nullable PauseResumeWithNativeObserver mLifecycleObserver;

    @VisibleForTesting
    ChromeSurveyController(String triggerId,
            @Nullable ActivityLifecycleDispatcher lifecycleDispatcher, Activity activity,
            MessageDispatcher messageDispatcher) {
        mTriggerId = triggerId;
        mLifecycleDispatcher = lifecycleDispatcher;
        mActivity = activity;
        mMessageDispatcher = messageDispatcher;
        mSurveyThrottler = new SurveyThrottler(triggerId, 1f / getMaxNumber(),
                ChromeSurveyController::isUMAEnabled, getMaxDownloadAttempt());
    }

    /**
     * Checks if the conditions to show the survey are met and starts the process if they are.
     * @param tabModelSelector The tab model selector to access the tab on which the survey will be
     *                         shown.
     * @param lifecycleDispatcher Dispatcher for activity lifecycle events.
     * @param activity The {@link Activity} on which the survey will be shown.
     * @param messageDispatcher The {@link MessageDispatcher} for displaying messages.
     */
    public static void initialize(TabModelSelector tabModelSelector,
            @Nullable ActivityLifecycleDispatcher lifecycleDispatcher, Activity activity,
            MessageDispatcher messageDispatcher) {
        assert tabModelSelector != null;
        if (!isSurveyEnabled() || TextUtils.isEmpty(getTriggerId())) return;
        new StartDownloadIfEligibleTask(new ChromeSurveyController(getTriggerId(),
                                                lifecycleDispatcher, activity, messageDispatcher),
                tabModelSelector)
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Downloads the survey if the user qualifies.
     * @param context The current Android {@link Context}.
     * @param tabModelSelector The tab model selector to access the tab on which the survey will be
     *                         shown.
     */
    private void startDownload(Context context, TabModelSelector tabModelSelector) {
        mSurveyThrottler.recordDownloadAttempted();
        mLoggingHandler = new Handler();
        mTabModelSelector = tabModelSelector;

        mSurveyController = sControllerForTesting != null ? sControllerForTesting
                                                          : SurveyControllerProvider.create();
        Runnable onSuccessRunnable = () -> onSurveyAvailable(mTriggerId);
        Runnable onFailureRunnable = () -> Log.w(TAG, "Survey does not exists or download failed.");
        mSurveyController.downloadSurvey(context, mTriggerId, onSuccessRunnable, onFailureRunnable);
    }

    /**
     * Called when the survey has finished downloading to show the survey prompt.
     * @param siteId The site id of the survey to display.
     */
    @VisibleForTesting
    void onSurveyAvailable(String siteId) {
        if (!isUMAEnabled() || attemptToShowOnTab(mTabModelSelector.getCurrentTab(), siteId)) {
            return;
        }

        mTabModelObserver = new TabModelSelectorObserver() {
            @Override
            public void onChange() {
                attemptToShowOnTab(mTabModelSelector.getCurrentTab(), siteId);
            }
        };

        // TODO(https://crbug.com/1192719): Remove the observer properly.
        mTabModelSelector.addObserver(mTabModelObserver);
    }

    /**
     * Show the survey prompt on the passed in tab if the tab is finished loading.
     * Else, it adds a listener to the tab to show the prompt once conditions are met.
     * @param tab The tab to attempt to attach the survey prompt.
     * @param siteId The site id of the survey to display.
     * @return If the survey prompt was successfully shown.
     */
    private boolean attemptToShowOnTab(Tab tab, String siteId) {
        if (!isUMAEnabled()) {
            if (mTabModelObserver != null) {
                mTabModelSelector.removeObserver(mTabModelObserver);
                mTabModelObserver = null;
            }
            return false;
        }
        if (!isValidTabForSurvey(tab)) return false;

        if (tab.isLoading() || !tab.isUserInteractable()) {
            tab.addObserver(createTabObserver(tab, siteId));
            return false;
        }

        showSurveyPrompt(tab, siteId);
        return true;
    }

    /**
     * Shows the survey prompt as a message.
     * @param tab The tab to attach the survey prompt.
     * @param siteId The site id of the survey to display.
     */
    @VisibleForTesting
    void showSurveyPrompt(@NonNull Tab tab, String siteId) {
        mSurveyPromptTab = tab;

        if (mMessageDispatcher == null) {
            mTabModelSelector.removeObserver(mTabModelObserver);
            return;
        }

        // Return early if the message is already shown once.
        if (sMessageShown) {
            return;
        }

        // Return early without displaying the message prompt if the survey has expired.
        if (mSurveyController.isSurveyExpired(siteId)) {
            return;
        }
        Resources resources = mActivity.getResources();

        PropertyModel message =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.CHROME_SURVEY)
                        .with(MessageBannerProperties.TITLE,
                                resources.getString(R.string.chrome_survey_message_title))
                        .with(MessageBannerProperties.ICON_RESOURCE_ID, R.drawable.chrome_sync_logo)
                        .with(MessageBannerProperties.ICON_TINT_COLOR,
                                MessageBannerProperties.TINT_NONE)
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                resources.getString(R.string.chrome_survey_message_button))
                        .with(MessageBannerProperties.ON_PRIMARY_ACTION,
                                () -> {
                                    showSurvey(siteId);
                                    return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                                })
                        .with(MessageBannerProperties.ON_DISMISSED, this::onMessageDismissed)
                        .build();

        // Dismiss the message when the original tab in which the message is shown is
        // hidden. This prevents the prompt from being shown if the tab is opened after being
        // hidden for a duration in which the survey expired. See crbug.com/1249055 for details.
        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onHidden(Tab tab, @TabHidingType int type) {
                mMessageDispatcher.dismissMessage(message, DismissReason.TAB_SWITCHED);
            }
        };

        // This observer will be added exactly once because of the `sMessageShown` conditional above
        // that restricts enqueueing the message only once in a session. The observer will be
        // removed when the enqueued message is dismissed.
        mSurveyPromptTab.addObserver(mTabObserver);

        if (mLifecycleDispatcher != null) {
            mLifecycleObserver = new PauseResumeWithNativeObserver() {
                @Override
                public void onResumeWithNative() {
                    if (mSurveyController.isSurveyExpired(siteId)) {
                        mMessageDispatcher.dismissMessage(
                                message, DismissReason.DISMISSED_BY_FEATURE);
                    }
                }

                @Override
                public void onPauseWithNative() {}
            };
            mLifecycleDispatcher.register(mLifecycleObserver);
        }

        mMessageDispatcher.enqueueWindowScopedMessage(message, false);
        sMessageShown = true;
    }

    /**
     * Shows the survey and closes the survey prompt.
     * @param siteId The site id of the survey to display.
     */
    private void showSurvey(String siteId) {
        mSurveyController.showSurveyIfAvailable(
                mActivity, siteId, R.drawable.chrome_sync_logo, mLifecycleDispatcher, null);
    }

    /**
     * Perform tasks after the message is dismissed.
     * @param dismissReason The reason for dismissal of the survey prompt.
     */
    private void onMessageDismissed(@DismissReason int dismissReason) {
        if (mSurveyPromptTab != null && mTabObserver != null) {
            mSurveyPromptTab.removeObserver(mTabObserver);
            mTabObserver = null;
        }
        if (mLifecycleDispatcher != null && mLifecycleObserver != null) {
            mLifecycleDispatcher.unregister(mLifecycleObserver);
            mLifecycleObserver = null;
        }
        if (dismissReason == DismissReason.GESTURE || dismissReason == DismissReason.TIMER
                || dismissReason == DismissReason.TAB_SWITCHED) {
            recordSurveyPromptDisplayed();
        } else if (dismissReason == DismissReason.PRIMARY_ACTION) {
            recordSurveyPromptDisplayed();
            mSurveyThrottler.recordSurveyAccepted();
        }
    }

    /**
     * @return The observer that handles cases where the user switches tabs before the prompt is
     *         shown.
     */
    private TabObserver createTabObserver(Tab tab, String siteId) {
        return new EmptyTabObserver() {
            @Override
            public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
                showPromptIfApplicable(tab, siteId, this);
            }

            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                showPromptIfApplicable(tab, siteId, this);
            }

            @Override
            public void onHidden(Tab tab, @TabHidingType int type) {
                // A prompt shouldn't appear on a tab that the user left.
                tab.removeObserver(this);
            }
        };
    }

    private SurveyThrottler getThrottler() {
        return mSurveyThrottler;
    }

    /**
     * Shows the prompt if the passed in tab is fully loaded and interactable.
     * @param tab The tab to attach the survey info bar.
     * @param siteId The site id of the survey to display.
     * @param observer The tab observer to remove from the passed in tab.
     */
    @VisibleForTesting
    void showPromptIfApplicable(Tab tab, String siteId, TabObserver observer) {
        if (!tab.isUserInteractable() || tab.isLoading()) return;
        if (isUMAEnabled()) {
            showSurveyPrompt(tab, siteId);
        }
        tab.removeObserver(observer);
    }

    /**
     * Checks if the tab is valid for a survey (i.e. not null, no null webcontents & not incognito).
     * @param tab The tab to be checked.
     * @return Whether or not the tab is valid.
     */
    @VisibleForTesting
    boolean isValidTabForSurvey(Tab tab) {
        return tab != null && tab.getWebContents() != null && !tab.isIncognito();
    }

    /** Logs in SharedPreferences that the info bar was displayed. */
    @VisibleForTesting
    void recordSurveyPromptDisplayed() {
        // This can be called multiple times e.g. by mLoggingHandler.
        // Return early to allow only one call to this method (http://crbug.com/791076).
        if (mSurveyPromptTab == null) return;

        mLoggingHandler.removeCallbacksAndMessages(null);

        mSurveyThrottler.recordSurveyPromptDisplayed();
        mSurveyPromptTab = null;
    }

    @VisibleForTesting
    void setTabModelSelector(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
    }

    static class StartDownloadIfEligibleTask extends AsyncTask<Boolean> {
        ChromeSurveyController mController;
        final TabModelSelector mSelector;

        public StartDownloadIfEligibleTask(
                ChromeSurveyController controller, TabModelSelector tabModelSelector) {
            mController = controller;
            mSelector = tabModelSelector;
        }

        @Override
        protected Boolean doInBackground() {
            return mController.getThrottler().canShowSurvey();
        }

        @Override
        protected void onPostExecute(Boolean result) {
            if (result) {
                mController.startDownload(ContextUtils.getApplicationContext(), mSelector);
            }
        }
    }

    /** @return If the survey is enabled by finch flag or commandline switch. */
    @VisibleForTesting
    static boolean isSurveyEnabled() {
        return isSurveyForceEnabled()
                || ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SURVEY_NEXT_ANDROID);
    }

    /** @return Whether metrics and crash dumps are enabled. */
    private static boolean isUMAEnabled() {
        return sForceUmaEnabledForTesting
                || PrivacyPreferencesManagerImpl.getInstance().isUsageAndCrashReportingPermitted();
    }

    /** @return Whether survey is enabled by command line flag. */
    public static boolean isSurveyForceEnabled() {
        return CommandLine.getInstance().hasSwitch(ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY);
    }

    /** @return The trigger Id that used to download / display certain survey. */
    @VisibleForTesting
    static String getTriggerId() {
        CommandLine commandLine = CommandLine.getInstance();
        if (commandLine.hasSwitch(COMMAND_LINE_PARAM_NAME)) {
            return commandLine.getSwitchValue(COMMAND_LINE_PARAM_NAME);
        } else {
            return ChromeFeatureList.getFieldTrialParamByFeature(
                    ChromeFeatureList.CHROME_SURVEY_NEXT_ANDROID, SITE_ID_PARAM_NAME);
        }
    }

    /** @return The max number that used to control the rate limit from the finch config. */
    private static int getMaxNumber() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.CHROME_SURVEY_NEXT_ANDROID, MAX_NUMBER, -1);
    }

    private static int getMaxDownloadAttempt() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.CHROME_SURVEY_NEXT_ANDROID, MAX_DOWNLOAD_ATTEMPTS, 0);
    }

    /** @return Whether the message has been previously shown to the client. */
    @VisibleForTesting
    public static boolean isMessageShown() {
        return sMessageShown;
    }

    /** Set whether UMA consent is granted during tests. Reset to "false" after tests. */
    public static void forceIsUMAEnabledForTesting(boolean forcedUMAStatus) {
        sForceUmaEnabledForTesting = forcedUMAStatus;
        ResettersForTesting.register(() -> sForceUmaEnabledForTesting = false);
    }

    /** Reset the tracker whether HaTS messages has shown during tests. */
    public static void resetMessageShownForTesting() {
        sMessageShown = false;
    }

    /**
     * Set the test only survey controller to use instead of creating new SurveyController.
     */
    public static void setSurveyControllerForTesting(SurveyController surveyController) {
        sControllerForTesting = surveyController;
        ResettersForTesting.register(() -> sControllerForTesting = null);
    }
}
