// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.os.Handler;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.infobar.SurveyInfoBar;
import org.chromium.chrome.browser.infobar.SurveyInfoBarDelegate;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.components.infobars.InfoBarAnimationListener;
import org.chromium.components.infobars.InfoBarUiItem;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Calendar;
import java.util.Random;

/**
 * Class that controls if and when to show surveys. One instance of this class is associated with
 * one trigger ID, which is used to fetch a survey, at the time it is created.
 *
 * @see #getTriggerId()
 */
public class ChromeSurveyController implements InfoBarAnimationListener {
    private static final String TAG = "ChromeSurveyCtrler";
    private static final int DOWNLOAD_ATTEMPTS_HIST_NUM_BUCKETS = 20;

    @VisibleForTesting
    static final long REQUIRED_VISIBILITY_DURATION_MS = 5000;
    @VisibleForTesting
    public static final String COMMAND_LINE_PARAM_NAME = "survey_override_site_id";
    @VisibleForTesting
    static final String MAX_DOWNLOAD_ATTEMPTS = "max-download-attempts";
    @VisibleForTesting
    static final String MAX_NUMBER = "max-number";
    @VisibleForTesting
    static final String SITE_ID_PARAM_NAME = "site-id";
    private static boolean sForceUmaEnabledForTesting;
    private static boolean sMessageShown;

    /**
     * Reasons that the user was rejected from being selected for a survey
     * Note: these values cannot change and must match the SurveyFilteringResult enum in enums.xml
     * because they're written to logs.
     */
    @IntDef({FilteringResult.SURVEY_INFOBAR_ALREADY_DISPLAYED,
            FilteringResult.FORCE_SURVEY_ON_COMMAND_PRESENT,
            FilteringResult.USER_ALREADY_SAMPLED_TODAY, FilteringResult.MAX_NUMBER_MISSING,
            FilteringResult.ROLLED_NON_ZERO_NUMBER, FilteringResult.USER_SELECTED_FOR_SURVEY,
            FilteringResult.FIRST_TIME_USER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface FilteringResult {
        int SURVEY_INFOBAR_ALREADY_DISPLAYED = 0;
        int FORCE_SURVEY_ON_COMMAND_PRESENT = 2;
        int USER_ALREADY_SAMPLED_TODAY = 3;
        int MAX_NUMBER_MISSING = 4;
        int ROLLED_NON_ZERO_NUMBER = 5;
        int USER_SELECTED_FOR_SURVEY = 6;
        int FIRST_TIME_USER = 8;
        // Number of entries
        int NUM_ENTRIES = 9;
    }

    /**
     * How the infobar was closed and its visibility status when it was closed.
     * Note: these values must match the InfoBarClosingStates enum in enums.xml.
     */
    @IntDef({InfoBarClosingState.ACCEPTED_SURVEY, InfoBarClosingState.CLOSE_BUTTON,
            InfoBarClosingState.VISIBLE_INDIRECT, InfoBarClosingState.HIDDEN_INDIRECT,
            InfoBarClosingState.UNKNOWN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface InfoBarClosingState {
        int ACCEPTED_SURVEY = 0;
        int CLOSE_BUTTON = 1;
        int VISIBLE_INDIRECT = 2;
        int HIDDEN_INDIRECT = 3;
        int UNKNOWN = 4;
        // Number of entries
        int NUM_ENTRIES = 5;
    }

    private TabModelSelector mTabModelSelector;
    private Handler mLoggingHandler;
    private Tab mSurveyPromptTab;
    private TabModelSelectorObserver mTabModelObserver;

    private final String mTriggerId;
    private final String mPrefKeyPromptDisplayed;
    private final String mPrefKeyDownloadAttempts;
    private final @Nullable ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final Activity mActivity;
    private final MessageDispatcher mMessageDispatcher;
    private @Nullable TabObserver mTabObserver;
    private @Nullable PauseResumeWithNativeObserver mLifecycleObserver;

    @VisibleForTesting
    ChromeSurveyController(String triggerId,
            @Nullable ActivityLifecycleDispatcher lifecycleDispatcher, Activity activity,
            MessageDispatcher messageDispatcher) {
        mTriggerId = triggerId;
        mPrefKeyPromptDisplayed =
                ChromePreferenceKeys.CHROME_SURVEY_PROMPT_DISPLAYED_TIMESTAMP.createKey(mTriggerId);
        mPrefKeyDownloadAttempts =
                ChromePreferenceKeys.CHROME_SURVEY_DOWNLOAD_ATTEMPTS.createKey(mTriggerId);
        mLifecycleDispatcher = lifecycleDispatcher;
        mActivity = activity;
        mMessageDispatcher = messageDispatcher;
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
        mLoggingHandler = new Handler();
        mTabModelSelector = tabModelSelector;

        SurveyController surveyController = SurveyController.getInstance();
        Runnable onSuccessRunnable = () -> onSurveyAvailable(mTriggerId);
        Runnable onFailureRunnable = () -> Log.w(TAG, "Survey does not exists or download failed.");
        surveyController.downloadSurvey(context, mTriggerId, onSuccessRunnable, onFailureRunnable);
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
     * Shows the survey prompt as an infobar or a message.
     * @param tab The tab to attach the survey prompt.
     * @param siteId The site id of the survey to display.
     */
    @VisibleForTesting
    void showSurveyPrompt(@NonNull Tab tab, String siteId) {
        String debugMessage =
                "Logging invocation of #showSurveyPrompt to investigate crbug.com/1249055.";
        String callTrace = Log.getStackTraceString(new Throwable(debugMessage));
        Log.i(TAG, callTrace);

        mSurveyPromptTab = tab;

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MESSAGES_FOR_ANDROID_CHROME_SURVEY)
                && mMessageDispatcher != null) {
            // Return early if the message is already shown once.
            if (sMessageShown) {
                String logMessage = String.format(
                        "The survey prompt for survey with ID %s has already been shown.", siteId);
                Log.w(TAG, logMessage);
                ChromePureJavaExceptionReporter.reportJavaException(new Throwable(logMessage));
                return;
            }

            // Return early without displaying the message prompt if the survey has expired.
            if (SurveyController.getInstance().isSurveyExpired(siteId)) {
                String logMessage =
                        String.format("The message prompt will not be shown because the survey "
                                        + "with ID %s has expired.",
                                siteId);
                Log.w(TAG, logMessage);
                ChromePureJavaExceptionReporter.reportJavaException(new Throwable(logMessage));
                return;
            }
            Resources resources = mActivity.getResources();

            PropertyModel message =
                    new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                            .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                    MessageIdentifier.CHROME_SURVEY)
                            .with(MessageBannerProperties.TITLE,
                                    resources.getString(R.string.chrome_survey_message_title))
                            .with(MessageBannerProperties.ICON_RESOURCE_ID,
                                    R.drawable.chrome_sync_logo)
                            .with(MessageBannerProperties.ICON_TINT_COLOR,
                                    MessageBannerProperties.TINT_NONE)
                            .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                    resources.getString(R.string.chrome_survey_message_button))
                            .with(MessageBannerProperties.ON_PRIMARY_ACTION,
                                    () -> {
                                        showSurvey(siteId);
                                        return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                                    })
                            .with(MessageBannerProperties.ON_DISMISSED,
                                    this::recordSurveyPromptMetrics)
                            .build();

            // Dismiss an enqueued message when the survey has expired so that it does not get shown
            // subsequently.
            message.set(MessageBannerProperties.ON_STARTED_SHOWING, () -> {
                boolean surveyExpired = SurveyController.getInstance().isSurveyExpired(siteId);
                if (!surveyExpired && isUMAEnabled()) {
                    return true;
                }
                if (surveyExpired) {
                    Log.w(TAG,
                            "The survey message prompt was dismissed because the survey "
                                    + "with ID %s has expired.",
                            siteId);
                }
                new Handler(ThreadUtils.getUiThreadLooper())
                        .post(()
                                        -> mMessageDispatcher.dismissMessage(
                                                message, DismissReason.DISMISSED_BY_FEATURE));
                return false;
            });

            // Dismiss the message when the original tab in which the message is shown is
            // hidden. This keeps in line with existing infobar behavior and prevents the prompt
            // from being shown if the tab is opened after being hidden for a duration in which
            // the survey expired. See crbug.com/1249055 for details.
            mTabObserver = new EmptyTabObserver() {
                @Override
                public void onHidden(Tab tab, @TabHidingType int type) {
                    mMessageDispatcher.dismissMessage(message, DismissReason.TAB_SWITCHED);
                }
            };
            mSurveyPromptTab.addObserver(mTabObserver);

            if (mLifecycleDispatcher != null) {
                mLifecycleObserver = new PauseResumeWithNativeObserver() {
                    @Override
                    public void onResumeWithNative() {
                        if (SurveyController.getInstance().isSurveyExpired(siteId)) {
                            String logMessage = String.format(
                                    "The survey message prompt was dismissed on activity resumption"
                                            + " because the survey with ID %s has expired.",
                                    siteId);
                            Log.w(TAG, logMessage);
                            ChromePureJavaExceptionReporter.reportJavaException(
                                    new Throwable(logMessage));
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
        } else {
            InfoBarContainer.get(tab).addAnimationListener(this);

            SurveyInfoBar.showSurveyInfoBar(
                    tab.getWebContents(), R.drawable.chrome_sync_logo, getSurveyInfoBarDelegate());

            RecordUserAction.record("Android.Survey.ShowSurveyInfoBar");
        }
        mTabModelSelector.removeObserver(mTabModelObserver);
    }

    /**
     * Shows the survey and closes the survey prompt.
     * @param siteId The site id of the survey to display.
     */
    private void showSurvey(String siteId) {
        SurveyController.getInstance().showSurveyIfAvailable(
                mActivity, siteId, true, R.drawable.chrome_sync_logo, mLifecycleDispatcher);
    }

    /**
     * Record metrics for user actions on the survey prompt.
     * @param dismissReason The reason for dismissal of the survey prompt.
     */
    private void recordSurveyPromptMetrics(@DismissReason int dismissReason) {
        if (mSurveyPromptTab != null && mTabObserver != null) {
            mSurveyPromptTab.removeObserver(mTabObserver);
            mTabObserver = null;
        }
        if (mLifecycleDispatcher != null && mLifecycleObserver != null) {
            mLifecycleDispatcher.unregister(mLifecycleObserver);
            mLifecycleObserver = null;
        }
        if (dismissReason == DismissReason.GESTURE) {
            // Survey prompt was dismissed by the user.
            recordInfoBarClosingState(InfoBarClosingState.CLOSE_BUTTON);
            recordSurveyPromptDisplayed();
        } else if (dismissReason == DismissReason.PRIMARY_ACTION) {
            // Survey was accepted by the user.
            recordInfoBarClosingState(InfoBarClosingState.ACCEPTED_SURVEY);
            recordSurveyPromptDisplayed();
            recordSurveyAccepted();
        } else if (dismissReason == DismissReason.TIMER) {
            // Survey prompt was auto-dismissed from the front.
            recordInfoBarClosingState(InfoBarClosingState.VISIBLE_INDIRECT);
            recordSurveyPromptDisplayed();
        } else if (dismissReason == DismissReason.TAB_SWITCHED) {
            // Survey prompt was dismissed when the tab showing the prompt was hidden.
            recordInfoBarClosingState(InfoBarClosingState.UNKNOWN);
            recordSurveyPromptDisplayed();
        } else if (dismissReason == DismissReason.DISMISSED_BY_FEATURE) {
            // Survey prompt was dismissed when the survey expired.
            recordInfoBarClosingState(InfoBarClosingState.UNKNOWN);
        } else {
            recordInfoBarClosingState(InfoBarClosingState.UNKNOWN);
        }
    }

    /**
     * @return The observer that handles cases where the user switches tabs before an infobar is
     *         shown.
     */
    private TabObserver createTabObserver(Tab tab, String siteId) {
        return new EmptyTabObserver() {
            @Override
            public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
                showInfoBarIfApplicable(tab, siteId, this);
            }

            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                showInfoBarIfApplicable(tab, siteId, this);
            }

            @Override
            public void onHidden(Tab tab, @TabHidingType int type) {
                // An infobar shouldn't appear on a tab that the user left.
                tab.removeObserver(this);
            }
        };
    }

    /**
     * Shows the infobar if the passed in tab is fully loaded and interactable.
     * @param tab The tab to attach the survey info bar.
     * @param siteId The site id of the survey to display.
     * @param observer The tab observer to remove from the passed in tab.
     */
    @VisibleForTesting
    void showInfoBarIfApplicable(Tab tab, String siteId, TabObserver observer) {
        if (!tab.isUserInteractable() || tab.isLoading()) return;
        if (isUMAEnabled()) {
            showSurveyPrompt(tab, siteId);
        }
        tab.removeObserver(observer);
    }

    /** @return If the survey info bar for this survey was logged as seen before. */
    @VisibleForTesting
    boolean hasInfoBarBeenDisplayed() {
        SharedPreferencesManager preferences = SharedPreferencesManager.getInstance();

        // TODO(https://crbug.com/1195928): Get an expiration date from feature flag.
        if (preferences.readLong(mPrefKeyPromptDisplayed, -1L) != -1L) {
            recordSurveyFilteringResult(FilteringResult.SURVEY_INFOBAR_ALREADY_DISPLAYED);
            return true;
        }
        return false;
    }

    private void recordDownloadAttempted() {
        SharedPreferencesManager.getInstance().incrementInt(mPrefKeyDownloadAttempts);
    }

    /** Return whether the number of download attempts falls within the max cap. */
    private boolean isDownloadAttemptAllowed() {
        int maxDownloadAttempts = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.CHROME_SURVEY_NEXT_ANDROID, MAX_DOWNLOAD_ATTEMPTS, 0);
        int downloadAttemptsMade =
                SharedPreferencesManager.getInstance().readInt(mPrefKeyDownloadAttempts, 0);
        return maxDownloadAttempts <= 0 || downloadAttemptsMade < maxDownloadAttempts;
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

    @Override
    public void notifyAnimationFinished(int animationType) {}

    @Override
    public void notifyAllAnimationsFinished(InfoBarUiItem frontInfoBar) {
        mLoggingHandler.removeCallbacksAndMessages(null);

        // If the survey info bar is in front, start the countdown to log that it was displayed.
        if (frontInfoBar == null
                || frontInfoBar.getInfoBarIdentifier()
                        != InfoBarIdentifier.SURVEY_INFOBAR_ANDROID) {
            return;
        }

        mLoggingHandler.postDelayed(
                () -> recordSurveyPromptDisplayed(), REQUIRED_VISIBILITY_DURATION_MS);
    }

    /**
     * Rolls a random number to see if the user was eligible for the survey. The user will skip the
     * roll if:
     *  1. User is a first time user
     *  2. User as performed the roll today
     *  3. Max number is not setup correctly
     *
     * @return Whether the user is eligible (i.e. the random number rolled was 0).
     */
    @VisibleForTesting
    boolean isRandomlySelectedForSurvey() {
        if (FirstRunStatus.isFirstRunTriggered()) {
            recordSurveyFilteringResult(FilteringResult.FIRST_TIME_USER);
            return false;
        }

        SharedPreferencesManager preferences = SharedPreferencesManager.getInstance();
        int lastDate = preferences.readInt(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED, -1);
        int today = getDayOfYear();
        if (lastDate == today) {
            recordSurveyFilteringResult(FilteringResult.USER_ALREADY_SAMPLED_TODAY);
            return false;
        }

        int maxNumber = getMaxNumber();
        if (maxNumber == -1) {
            recordSurveyFilteringResult(FilteringResult.MAX_NUMBER_MISSING);
            return false;
        }

        preferences.writeInt(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED, today);
        if (getRandomNumberUpTo(maxNumber) == 0) {
            recordSurveyFilteringResult(FilteringResult.USER_SELECTED_FOR_SURVEY);
            return true;
        } else {
            recordSurveyFilteringResult(FilteringResult.ROLLED_NON_ZERO_NUMBER);
            return false;
        }
    }

    /**
     * @param max The max threshold for the random number generator.
     * @return A random number from 0 (inclusive) to the max number (exclusive).
     */
    @VisibleForTesting
    int getRandomNumberUpTo(int max) {
        return new Random().nextInt(max);
    }

    /** @return The max number that used to control the rate limit from the finch config. */
    @VisibleForTesting
    int getMaxNumber() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.CHROME_SURVEY_NEXT_ANDROID, MAX_NUMBER, -1);
    }

    /** @return The day of the year for today. */
    @VisibleForTesting
    int getDayOfYear() {
        ThreadUtils.assertOnBackgroundThread();
        return Calendar.getInstance().get(Calendar.DAY_OF_YEAR);
    }

    /**
     * @return The survey info bar delegate containing actions specific to the Chrome Home survey.
     */
    private SurveyInfoBarDelegate getSurveyInfoBarDelegate() {
        return new SurveyInfoBarDelegate() {
            @Override
            public void onSurveyInfoBarTabInteractabilityChanged(boolean isInteractable) {
                if (mSurveyPromptTab == null) return;

                if (!isInteractable) {
                    mLoggingHandler.removeCallbacksAndMessages(null);
                    return;
                }

                mLoggingHandler.postDelayed(
                        () -> recordSurveyPromptDisplayed(), REQUIRED_VISIBILITY_DURATION_MS);
            }

            @Override
            public void onSurveyInfoBarTabHidden() {
                mLoggingHandler.removeCallbacksAndMessages(null);
                mSurveyPromptTab = null;
            }

            @Override
            public void onSurveyInfoBarClosed(boolean viaCloseButton, boolean visibleWhenClosed) {
                if (viaCloseButton) {
                    recordInfoBarClosingState(InfoBarClosingState.CLOSE_BUTTON);
                    recordSurveyPromptDisplayed();
                } else {
                    if (visibleWhenClosed) {
                        recordInfoBarClosingState(InfoBarClosingState.VISIBLE_INDIRECT);
                    } else {
                        recordInfoBarClosingState(InfoBarClosingState.HIDDEN_INDIRECT);
                    }
                }
            }

            @Override
            public void onSurveyTriggered() {
                recordSurveyPromptMetrics(DismissReason.PRIMARY_ACTION);
                showSurvey(mTriggerId);
            }

            @Override
            public String getSurveyPromptString() {
                return ContextUtils.getApplicationContext().getString(
                        R.string.chrome_survey_prompt);
            }

            @Override
            public ActivityLifecycleDispatcher getLifecycleDispatcher() {
                return mLifecycleDispatcher;
            }
        };
    }

    /** Logs in SharedPreferences that the info bar was displayed. */
    @VisibleForTesting
    void recordSurveyPromptDisplayed() {
        // This can be called multiple times e.g. by mLoggingHandler & onSurveyInfoBarClosed().
        // Return early to allow only one call to this method (http://crbug.com/791076).
        if (mSurveyPromptTab == null) return;

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.MESSAGES_FOR_ANDROID_CHROME_SURVEY)
                && !mSurveyPromptTab.isDestroyed()) {
            // If this function is invoked after activity / tab is destroyed, do nothing.
            // (http://crbug.com/1245624)
            InfoBarContainer container = InfoBarContainer.get(mSurveyPromptTab);
            if (container != null) container.removeAnimationListener(this);
        }

        mLoggingHandler.removeCallbacksAndMessages(null);

        SharedPreferencesManager preferences = SharedPreferencesManager.getInstance();
        preferences.writeLong(mPrefKeyPromptDisplayed, System.currentTimeMillis());
        mSurveyPromptTab = null;
    }

    @VisibleForTesting
    void setTabModelSelector(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
    }

    private void recordSurveyFilteringResult(@FilteringResult int value) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Survey.SurveyFilteringResults", value, FilteringResult.NUM_ENTRIES);
    }

    private void recordInfoBarClosingState(@InfoBarClosingState int value) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Survey.InfoBarClosingState", value, InfoBarClosingState.NUM_ENTRIES);
    }

    private void recordSurveyAccepted() {
        int downloadAttemptsMade =
                SharedPreferencesManager.getInstance().readInt(mPrefKeyDownloadAttempts, 0);
        RecordHistogram.recordLinearCountHistogram("Android.Survey.DownloadAttemptsBeforeAccepted",
                downloadAttemptsMade, 1, DOWNLOAD_ATTEMPTS_HIST_NUM_BUCKETS,
                DOWNLOAD_ATTEMPTS_HIST_NUM_BUCKETS + 1);
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
            if (!isUMAEnabled()) return false;

            if (isSurveyForceEnabled()) {
                mController.recordSurveyFilteringResult(
                        FilteringResult.FORCE_SURVEY_ON_COMMAND_PRESENT);
                return true;
            }
            return !mController.hasInfoBarBeenDisplayed() && mController.isDownloadAttemptAllowed()
                    && mController.isRandomlySelectedForSurvey();
        }

        @Override
        protected void onPostExecute(Boolean result) {
            if (result) {
                mController.startDownload(ContextUtils.getApplicationContext(), mSelector);
                mController.recordDownloadAttempted();
            }
        }
    }

    // Force enable UMA testing for testing.
    @VisibleForTesting
    public static void forceIsUMAEnabledForTesting(boolean forcedUMAStatus) {
        sForceUmaEnabledForTesting = forcedUMAStatus;
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

    @VisibleForTesting
    public static Long getRequiredVisibilityDurationMs() {
        return REQUIRED_VISIBILITY_DURATION_MS;
    }

    /** @return Whether the message has been previously shown to the client. */
    @VisibleForTesting
    public static boolean isMessageShown() {
        return sMessageShown;
    }

    // Reset sMessageShown for testing.
    @VisibleForTesting
    public static void resetMessageShownForTesting() {
        sMessageShown = false;
    }
}
