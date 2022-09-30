// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.verifyZeroInteractions;

import android.app.Activity;
import android.content.Context;
import android.os.Looper;

import androidx.annotation.Nullable;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.RealObject;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.task.test.BackgroundShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.infobar.SurveyInfoBar;
import org.chromium.chrome.browser.infobar.SurveyInfoBarDelegate;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.survey.ChromeSurveyController.InfoBarClosingState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * "Integration" style unit tests for {@link ChromeSurveyController} that mocks most of the
 * info bar related code, aiming to cover the workflow from initialize survey download task.
 */
// clang-format off
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = { BackgroundShadowAsyncTask.class,
            ChromeSurveyControllerFlowTest.ShadowChromeFeatureList.class,
            ChromeSurveyControllerFlowTest.ShadowSurveyInfoBar.class,
            ChromeSurveyControllerFlowTest.ShadowInfoBarContainer.class
        })
//TODO(crbug.com/1210371): Rewrite using paused loop. See crbug for details.
@LooperMode(LooperMode.Mode.LEGACY)
// Set user is selected and by pass the rate limit. The rate limiting logic is tested in
// ChromeSurveyControllerTest.
@CommandLineFlags.Add(ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY)
public class ChromeSurveyControllerFlowTest {
    // clang-format on
    private static final String TEST_TRIGGER_ID = "test_trigger_id";

    @Implements(ChromeFeatureList.class)
    static class ShadowChromeFeatureList {
        static final Map<String, String> sParamValues = new HashMap<>();
        static boolean sEnableSurvey;
        static boolean sEnableMessages;

        @Implementation
        public static boolean isEnabled(String featureName) {
            if (featureName.equals(ChromeFeatureList.MESSAGES_FOR_ANDROID_CHROME_SURVEY)) {
                return sEnableMessages;
            }
            return featureName.equals(ChromeFeatureList.CHROME_SURVEY_NEXT_ANDROID)
                    && sEnableSurvey;
        }

        @Implementation
        public static String getFieldTrialParamByFeature(String feature, String paramKey) {
            Assert.assertTrue("Survey is not enabled.", isEnabled(feature));
            return sParamValues.getOrDefault(paramKey, "");
        }

        @Implementation
        public static int getFieldTrialParamByFeatureAsInt(
                String featureName, String paramName, int defaultValue) {
            Assert.assertTrue("Survey is not enabled.", isEnabled(featureName));
            return sParamValues.containsKey(paramName)
                    ? Integer.valueOf(sParamValues.get(paramName))
                    : defaultValue;
        }
    }

    @Implements(SurveyInfoBar.class)
    static class ShadowSurveyInfoBar {
        static PayloadCallbackHelper<SurveyInfoBarDelegate> sShowInfoBarCallback;

        @Implementation
        public static void showSurveyInfoBar(WebContents webContents, int displayLogoResId,
                SurveyInfoBarDelegate surveyInfoBarDelegate) {
            Assert.assertNotNull("sShowInfoBarCallback is null.", sShowInfoBarCallback);
            sShowInfoBarCallback.notifyCalled(surveyInfoBarDelegate);
        }
    }

    @Implements(InfoBarContainer.class)
    static class ShadowInfoBarContainer {
        @RealObject
        static InfoBarContainer sInfoBarContainer;

        @Implementation
        public static InfoBarContainer get(Tab tab) {
            return sInfoBarContainer;
        }
    }

    @Rule
    public MockitoRule mRule = MockitoJUnit.rule();
    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    TabModelSelector mMockModelSelector;
    @Mock
    Tab mMockTab;
    @Mock
    WebContents mMockWebContent;
    @Mock
    InfoBarContainer mMockInfoBarContainer;
    @Mock
    ActivityLifecycleDispatcher mMockLifecycleDispatcher;
    @Mock
    Activity mActivity;
    @Mock
    MessageDispatcher mMessageDispatcher;
    @Captor
    ArgumentCaptor<PropertyModel> mMessagePropertyCaptor;

    private final TestSurveyController mTestSurveyController = new TestSurveyController();

    private String mPrefKeyPromptShown;
    private String mPrefKeyDownloadAttempts;
    private SharedPreferencesManager mSharedPreferencesManager;

    private TabModelSelectorObserver mTabModelSelectorObserver;
    private TabObserver mTabObserver;
    private PauseResumeWithNativeObserver mLifecycleObserver;

    @Before
    public void setup() {
        ShadowChromeFeatureList.sEnableSurvey = true;
        ShadowChromeFeatureList.sParamValues.put(
                ChromeSurveyController.SITE_ID_PARAM_NAME, TEST_TRIGGER_ID);
        // By setting MAX_NUMBER to 1, #isRandomSelectedBySurvey is always true.
        ShadowChromeFeatureList.sParamValues.put(ChromeSurveyController.MAX_NUMBER, "1");
        ShadowInfoBarContainer.sInfoBarContainer = mMockInfoBarContainer;
        ShadowSurveyInfoBar.sShowInfoBarCallback = new PayloadCallbackHelper<>();

        SurveyController.setInstanceForTesting(mTestSurveyController);

        ChromeSurveyController.forceIsUMAEnabledForTesting(true);
        mPrefKeyPromptShown =
                ChromePreferenceKeys.CHROME_SURVEY_PROMPT_DISPLAYED_TIMESTAMP.createKey(
                        TEST_TRIGGER_ID);
        mPrefKeyDownloadAttempts =
                ChromePreferenceKeys.CHROME_SURVEY_DOWNLOAD_ATTEMPTS.createKey(TEST_TRIGGER_ID);
        mSharedPreferencesManager = SharedPreferencesManager.getInstance();

        Mockito.when(mActivity.getResources())
                .thenReturn(ApplicationProvider.getApplicationContext().getResources());
    }

    @After
    public void tearDown() {
        ChromeSurveyController.forceIsUMAEnabledForTesting(false);
        ChromeSurveyController.resetMessageShownForTesting();
        ShadowChromeFeatureList.sParamValues.clear();
        ShadowChromeFeatureList.sEnableSurvey = false;
        ShadowChromeFeatureList.sEnableMessages = false;
        UmaRecorderHolder.resetForTesting();

        CommandLine.getInstance().removeSwitch(ChromeSurveyController.COMMAND_LINE_PARAM_NAME);
    }

    @Test
    public void testCommandLine_GetTriggerId() {
        Assert.assertEquals("TriggerId does not match feature flag setting.", TEST_TRIGGER_ID,
                ChromeSurveyController.getTriggerId());

        final String commandLineTriggerId = "command_line_trigger_id";
        CommandLine.getInstance().appendSwitchWithValue(
                ChromeSurveyController.COMMAND_LINE_PARAM_NAME, commandLineTriggerId);
        Assert.assertEquals("TriggerId does not match commandline overrides.", commandLineTriggerId,
                ChromeSurveyController.getTriggerId());
    }

    @Test
    public void testCommandLine_ForceEnableSurvey() {
        CommandLine.getInstance().removeSwitch(ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY);
        Assert.assertTrue("Survey should be enabled by feature flag.",
                ChromeSurveyController.isSurveyEnabled());

        ShadowChromeFeatureList.sEnableSurvey = false;
        Assert.assertFalse("Survey should be disabled by feature flag.",
                ChromeSurveyController.isSurveyEnabled());

        CommandLine.getInstance().appendSwitch(ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY);
        Assert.assertTrue("Survey should be enabled by commandline switch.",
                ChromeSurveyController.isSurveyEnabled());
    }

    @Test
    public void testStartDownloadIfEligibleTask() {
        assertDownloadAttempted(false);
        initializeChromeSurveyController();
        assertDownloadAttempted(true);
    }

    @Test
    public void testStartDownloadIfEligibleTask_ShowedBefore() {
        CommandLine.getInstance().removeSwitch(ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY);
        mSharedPreferencesManager.writeLong(mPrefKeyPromptShown, 1000L);

        initializeChromeSurveyController();
        assertDownloadAttempted(false);
    }

    @Test
    public void testStartDownloadIfEligibleTask_ShowedBefore_ForceEnabled() {
        mSharedPreferencesManager.writeLong(mPrefKeyPromptShown, 1000L);

        assertDownloadAttempted(false);
        initializeChromeSurveyController();
        assertDownloadAttempted(true);
    }

    @Test
    public void testStartDownloadIfEligibleTask_UmaDisabled() {
        ChromeSurveyController.forceIsUMAEnabledForTesting(false);
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER, false);

        initializeChromeSurveyController();
        assertDownloadAttempted(false);
    }

    @Test
    public void testStartDownloadIfEligibleTask_UmaEnabled() {
        ChromeSurveyController.forceIsUMAEnabledForTesting(false);
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING_PERMITTED_BY_USER, true);

        assertDownloadAttempted(false);
        initializeChromeSurveyController();
        assertDownloadAttempted(true);
    }

    @Test
    public void testStartDownloadIfEligibleTask_DownloadCapZero() {
        CommandLine.getInstance().removeSwitch(ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY);
        ShadowChromeFeatureList.sParamValues.put(ChromeSurveyController.MAX_DOWNLOAD_ATTEMPTS, "0");

        initializeChromeSurveyController();
        assertDownloadAttempted(true);
    }

    @Test
    public void testStartDownloadIfEligibleTask_DownloadWithinCap() {
        CommandLine.getInstance().removeSwitch(ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY);
        ShadowChromeFeatureList.sParamValues.put(
                ChromeSurveyController.MAX_DOWNLOAD_ATTEMPTS, "99");

        assertDownloadAttempted(false);
        initializeChromeSurveyController();
        assertDownloadAttempted(true);
    }

    @Test
    public void testStartDownloadIfEligibleTask_DownloadReachCap() {
        CommandLine.getInstance().removeSwitch(ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY);
        ShadowChromeFeatureList.sParamValues.put(ChromeSurveyController.MAX_DOWNLOAD_ATTEMPTS, "2");
        mSharedPreferencesManager.writeInt(mPrefKeyDownloadAttempts, 2);

        initializeChromeSurveyController();
        Assert.assertEquals("Download should not be triggered.", 0,
                mTestSurveyController.downloadIfApplicableCallback.getCallCount());
    }

    @Test
    public void testStartDownloadIfEligibleTask_DownloadCapZero_ForceEnable() {
        ShadowChromeFeatureList.sParamValues.put(ChromeSurveyController.MAX_DOWNLOAD_ATTEMPTS, "0");

        assertDownloadAttempted(false);
        initializeChromeSurveyController();
        assertDownloadAttempted(true);
    }

    @Test
    public void testSurveyInfobarUI() {
        setupTabMocks();
        initializeChromeSurveyController();
        assertCallbackAssignedInSurveyController();

        // Verify the survey should be attempted to present on a valid tab.
        mockTabReady();
        // Verify that the feature flag for the survey messages UI is disabled
        Assert.assertFalse(
                ChromeFeatureList.isEnabled(ChromeFeatureList.MESSAGES_FOR_ANDROID_CHROME_SURVEY));
        mTestSurveyController.onDownloadSuccessRunnable.run();
        assertSurveyInfoBarShown(true);
        verifyNoMoreInteractions(mMessageDispatcher);
    }

    @Test
    public void testSurveyMessagesUI() {
        setupTabMocks();
        initializeChromeSurveyController();
        assertCallbackAssignedInSurveyController();

        // Verify the survey should be attempted to present on a valid tab.
        mockTabReady();
        ShadowChromeFeatureList.sEnableMessages = true;
        // Verify that the feature flag for the survey messages UI is enabled
        Assert.assertTrue(
                ChromeFeatureList.isEnabled(ChromeFeatureList.MESSAGES_FOR_ANDROID_CHROME_SURVEY));
        Assert.assertNotNull(mMessageDispatcher);
        mTestSurveyController.onDownloadSuccessRunnable.run();
        assertSurveyInfoBarShown(false);
        assertSurveyMessagesEnqueued();
    }

    @Test
    public void testPresentSurvey_ValidTab_SurveyInfobarDelegate() {
        presentSurveyInfoBarInValidTab();
    }

    @Test
    public void testPresentSurvey_LoadingTab() {
        setupTabMocks();
        initializeChromeSurveyController();
        assertCallbackAssignedInSurveyController();

        // Verify the survey should be attempted to present on a valid tab.
        Mockito.doReturn(true).when(mMockTab).isLoading();
        mTestSurveyController.onDownloadSuccessRunnable.run();

        assertSurveyInfoBarShown(false);
        Assert.assertNotNull("Tab observer should be registered.", mTabObserver);

        // Assume tab loading is complete.
        mockTabReady();
        mTabObserver.onPageLoadFinished(mMockTab, null);
        assertSurveyInfoBarShown(true);
    }

    @Test
    public void testPresentSurvey_TabInteractabilityChanged() {
        setupTabMocks();
        initializeChromeSurveyController();
        assertCallbackAssignedInSurveyController();

        // Verify the survey should be attempted to present on a valid tab.
        Mockito.doReturn(false).when(mMockTab).isUserInteractable();
        mTestSurveyController.onDownloadSuccessRunnable.run();

        assertSurveyInfoBarShown(false);
        Assert.assertNotNull("Tab observer should be registered.", mTabObserver);

        // Assume tab loading is complete.
        mockTabReady();
        mTabObserver.onInteractabilityChanged(mMockTab, true);
        assertSurveyInfoBarShown(true);
    }

    @Test
    public void testPresentSurvey_SwitchingTab() {
        setupTabMocks();
        initializeChromeSurveyController();
        assertCallbackAssignedInSurveyController();

        // Verify the survey should be attempted to present on a valid tab.
        Mockito.when(mMockModelSelector.getCurrentTab()).thenReturn(null);
        mTestSurveyController.onDownloadSuccessRunnable.run();

        assertSurveyInfoBarShown(false);
        Assert.assertNotNull(
                "TabModelSelectorObserver should be registered.", mTabModelSelectorObserver);

        // Assume tab selector can provide a tab (e.g. switch to a fully loaded tab).
        mockTabReady();
        Mockito.when(mMockModelSelector.getCurrentTab()).thenReturn(mMockTab);
        mTabModelSelectorObserver.onChange();
        assertSurveyInfoBarShown(true);
    }

    @Test
    public void testPresentSurvey_UmaDisabledBeforeTabReady() {
        setupTabMocks();
        initializeChromeSurveyController();
        assertCallbackAssignedInSurveyController();

        // Verify the survey should be attempted to present on a valid tab.
        Mockito.when(mMockModelSelector.getCurrentTab()).thenReturn(null);
        mTestSurveyController.onDownloadSuccessRunnable.run();

        assertSurveyInfoBarShown(false);
        Assert.assertNotNull(
                "TabModelSelectorObserver should be registered.", mTabModelSelectorObserver);

        // Assume user turn off UMA upload before survey is shown.
        ChromeSurveyController.forceIsUMAEnabledForTesting(false);

        mockTabReady();
        Mockito.when(mMockModelSelector.getCurrentTab()).thenReturn(mMockTab);
        mTabModelSelectorObserver.onChange();
        assertSurveyInfoBarShown(false);
        Assert.assertNull("TabModelSelectorObserver is unregistered since UMA upload is disabled.",
                mTabModelSelectorObserver);
    }

    @Test
    public void testSurveyInfoBarDelegate_getLifecycleDispatcher() {
        presentSurveyInfoBarInValidTab();
        SurveyInfoBarDelegate surveyInfoBarDelegate =
                ShadowSurveyInfoBar.sShowInfoBarCallback.getOnlyPayloadBlocking();
        Assert.assertEquals("#getLifecycleDispatcher is different.", mMockLifecycleDispatcher,
                surveyInfoBarDelegate.getLifecycleDispatcher());
    }

    @Test
    public void testSurveyInfoBarDelegate_getSurveyPromptString() {
        presentSurveyInfoBarInValidTab();
        SurveyInfoBarDelegate surveyInfoBarDelegate =
                ShadowSurveyInfoBar.sShowInfoBarCallback.getOnlyPayloadBlocking();

        Assert.assertEquals("#getPromptString is different.",
                ContextUtils.getApplicationContext().getString(R.string.chrome_survey_prompt),
                surveyInfoBarDelegate.getSurveyPromptString());
    }

    @Test
    public void testSurveyInfoBarDelegate_onSurveyTriggered() {
        presentSurveyInfoBarInValidTab();
        SurveyInfoBarDelegate surveyInfoBarDelegate =
                ShadowSurveyInfoBar.sShowInfoBarCallback.getOnlyPayloadBlocking();

        surveyInfoBarDelegate.onSurveyTriggered();
        assertInfoBarClosingStateRecorded(InfoBarClosingState.ACCEPTED_SURVEY);
        assertDownloadAttemptRecordedWithSample(1);
        assertInfoBarDisplayedRecorded();
    }

    @Test
    public void testSurveyInfoBarDelegate_onSurveyTriggered_DownloadBefore() {
        final int downloadAttempted = 3;
        mSharedPreferencesManager.writeInt(mPrefKeyDownloadAttempts, downloadAttempted);

        presentSurveyInfoBarInValidTab();
        SurveyInfoBarDelegate surveyInfoBarDelegate =
                ShadowSurveyInfoBar.sShowInfoBarCallback.getOnlyPayloadBlocking();

        surveyInfoBarDelegate.onSurveyTriggered();
        assertInfoBarClosingStateRecorded(InfoBarClosingState.ACCEPTED_SURVEY);
        assertDownloadAttemptRecordedWithSample(downloadAttempted + 1);
        assertInfoBarDisplayedRecorded();
    }

    @Test
    public void testSurveyInfoBarDelegate_onSurveyInfoBarClosed() {
        presentSurveyInfoBarInValidTab();
        SurveyInfoBarDelegate surveyInfoBarDelegate =
                ShadowSurveyInfoBar.sShowInfoBarCallback.getOnlyPayloadBlocking();

        surveyInfoBarDelegate.onSurveyInfoBarClosed(
                /*viaCloseButton=*/false, /*visibleWhenClosed=*/true);
        assertInfoBarClosingStateRecorded(InfoBarClosingState.VISIBLE_INDIRECT);
        assertInfoBarDisplayedNotRecorded("onSurveyInfoBarClosed with VISIBLE_INDIRECT "
                + "should not result in info bar displayed being recorded.");

        surveyInfoBarDelegate.onSurveyInfoBarClosed(
                /*viaCloseButton=*/false, /*visibleWhenClosed=*/false);
        assertInfoBarClosingStateRecorded(InfoBarClosingState.HIDDEN_INDIRECT);
        assertInfoBarDisplayedNotRecorded("onSurveyInfoBarClosed with HIDDEN_INDIRECT "
                + "should not result in info bar displayed being recorded.");

        // #onSurveyInfoBarClosed(true, false) is not a valid case, so skipped in test.
        surveyInfoBarDelegate.onSurveyInfoBarClosed(
                /*viaCloseButton=*/true, /*visibleWhenClosed=*/true);
        assertInfoBarClosingStateRecorded(InfoBarClosingState.CLOSE_BUTTON);
        assertInfoBarDisplayedRecorded();
    }

    @Test
    public void testSurveyInfoBarDelegate_onSurveyInfoBarTabBecomeInteractable() {
        presentSurveyInfoBarInValidTab();
        SurveyInfoBarDelegate surveyInfoBarDelegate =
                ShadowSurveyInfoBar.sShowInfoBarCallback.getOnlyPayloadBlocking();

        surveyInfoBarDelegate.onSurveyInfoBarTabInteractabilityChanged(true);
        Shadows.shadowOf(Looper.myLooper())
                .idleFor(ChromeSurveyController.REQUIRED_VISIBILITY_DURATION_MS,
                        TimeUnit.MILLISECONDS);
        assertInfoBarDisplayedRecorded();
    }

    @Test
    public void testSurveyInfoBarDelegate_onSurveyInfoBarTabBecomeNotInteractable() {
        presentSurveyInfoBarInValidTab();
        SurveyInfoBarDelegate surveyInfoBarDelegate =
                ShadowSurveyInfoBar.sShowInfoBarCallback.getOnlyPayloadBlocking();

        surveyInfoBarDelegate.onSurveyInfoBarTabInteractabilityChanged(true);
        Shadows.shadowOf(Looper.myLooper())
                .idleFor(ChromeSurveyController.REQUIRED_VISIBILITY_DURATION_MS - 1,
                        TimeUnit.MILLISECONDS);
        surveyInfoBarDelegate.onSurveyInfoBarTabInteractabilityChanged(false);
        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        assertInfoBarDisplayedNotRecorded("Info bar should not be recorded as displayed "
                + "if interactivity changed before minimum required visibility duration.");
    }

    @Test
    public void testSurveyInfoBarDelegate_onSurveyInfoBarTabHidden() {
        presentSurveyInfoBarInValidTab();
        SurveyInfoBarDelegate surveyInfoBarDelegate =
                ShadowSurveyInfoBar.sShowInfoBarCallback.getOnlyPayloadBlocking();

        surveyInfoBarDelegate.onSurveyInfoBarTabInteractabilityChanged(true);
        Shadows.shadowOf(Looper.myLooper())
                .idleFor(ChromeSurveyController.REQUIRED_VISIBILITY_DURATION_MS - 1,
                        TimeUnit.MILLISECONDS);
        surveyInfoBarDelegate.onSurveyInfoBarTabHidden();
        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        assertInfoBarDisplayedNotRecorded("Info bar should not be recorded as displayed "
                + "if hidden before minimum required visibility duration.");
    }

    @Test
    public void testMessages_Properties() {
        presentMessages();

        PropertyModel messageModel = mMessagePropertyCaptor.getValue();
        Assert.assertEquals("Message identifier is different.", MessageIdentifier.CHROME_SURVEY,
                messageModel.get(MessageBannerProperties.MESSAGE_IDENTIFIER));
        Assert.assertEquals("Message icon resource is different.", R.drawable.chrome_sync_logo,
                messageModel.get(MessageBannerProperties.ICON_RESOURCE_ID));
        Assert.assertEquals("Message icon tint is different.", MessageBannerProperties.TINT_NONE,
                messageModel.get(MessageBannerProperties.ICON_TINT_COLOR));
        Assert.assertEquals("Message title string is different.",
                ApplicationProvider.getApplicationContext().getResources().getString(
                        R.string.chrome_survey_message_title),
                messageModel.get(MessageBannerProperties.TITLE));
        Assert.assertEquals("Message action button string is different.",
                ApplicationProvider.getApplicationContext().getResources().getString(
                        R.string.chrome_survey_message_button),
                messageModel.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));

        Assert.assertNotNull("Message primary action is null.",
                messageModel.get(MessageBannerProperties.ON_PRIMARY_ACTION));
        Assert.assertNotNull("Message dismissal action is null.",
                messageModel.get(MessageBannerProperties.ON_DISMISSED));

        Assert.assertEquals("Message ON_PRIMARY_ACTION should return DISMISS_IMMEDIATELY.",
                PrimaryActionClickBehavior.DISMISS_IMMEDIATELY,
                messageModel.get(MessageBannerProperties.ON_PRIMARY_ACTION).get().intValue());
        Assert.assertEquals("showSurvey should be called.", 1,
                mTestSurveyController.showSurveyIfAvailableCallback.getCallCount());
    }

    @Test
    public void testMessages_NotShownOnExpiredSurvey() {
        ShadowChromeFeatureList.sEnableMessages = true;
        setupTabMocks();
        initializeChromeSurveyController();
        assertCallbackAssignedInSurveyController();
        mockTabReady();
        // Simulate an expired survey.
        mTestSurveyController.isSurveyExpired = true;
        mTestSurveyController.onDownloadSuccessRunnable.run();
        verifyZeroInteractions(mMessageDispatcher);
        Assert.assertEquals("showSurvey should not be called.", 0,
                mTestSurveyController.showSurveyIfAvailableCallback.getCallCount());
    }

    @Test
    public void testMessages_NotShownWhenUmaDisabled() {
        presentMessages();

        // Assume user turn off UMA upload before survey is shown.
        ChromeSurveyController.forceIsUMAEnabledForTesting(false);
        PropertyModel messageModel = mMessagePropertyCaptor.getValue();
        boolean shouldShow =
                messageModel.get(MessageBannerProperties.ON_STARTED_SHOWING).getAsBoolean();
        Assert.assertFalse(
                "The enqueued message should not be shown if UMA upload is disabled.", shouldShow);
        verify(mMessageDispatcher).dismissMessage(messageModel, DismissReason.DISMISSED_BY_FEATURE);
    }

    @Test
    public void testMessages_EnqueuedMessageDismissedOnExpiredSurvey() throws Exception {
        presentMessages();

        // Simulate survey expiration after the message is enqueued.
        mTestSurveyController.isSurveyExpired = true;

        PropertyModel messageModel = mMessagePropertyCaptor.getValue();
        boolean shouldShow =
                messageModel.get(MessageBannerProperties.ON_STARTED_SHOWING).getAsBoolean();
        Assert.assertFalse(
                "The enqueued message should not be shown if the survey has expired.", shouldShow);
        verify(mMessageDispatcher).dismissMessage(messageModel, DismissReason.DISMISSED_BY_FEATURE);
    }

    @Test
    public void testMessages_MessageShownOnce() {
        presentMessages();
        Assert.assertTrue("Message should be shown.", ChromeSurveyController.isMessageShown());
        verify(mMessageDispatcher).enqueueWindowScopedMessage(any(), anyBoolean());

        // Simulate survey download that triggers invocation of #showSurveyPrompt.
        mTestSurveyController.onDownloadSuccessRunnable.run();
        verifyNoMoreInteractions(mMessageDispatcher);
    }

    @Test
    public void testMessages_Dismiss_PrimaryAction() {
        presentMessages();
        PropertyModel messageModel = mMessagePropertyCaptor.getValue();

        messageModel.get(MessageBannerProperties.ON_DISMISSED)
                .onResult(DismissReason.PRIMARY_ACTION);
        assertInfoBarDisplayedRecorded();
        assertInfoBarClosingStateRecorded(InfoBarClosingState.ACCEPTED_SURVEY);
    }

    @Test
    public void testMessages_Dismiss_Gesture() {
        presentMessages();
        PropertyModel messageModel = mMessagePropertyCaptor.getValue();

        messageModel.get(MessageBannerProperties.ON_DISMISSED).onResult(DismissReason.GESTURE);
        assertInfoBarDisplayedRecorded();
        assertInfoBarClosingStateRecorded(InfoBarClosingState.CLOSE_BUTTON);
    }

    @Test
    public void testMessages_Dismiss_Timer() {
        presentMessages();
        PropertyModel messageModel = mMessagePropertyCaptor.getValue();

        messageModel.get(MessageBannerProperties.ON_DISMISSED).onResult(DismissReason.TIMER);
        assertInfoBarDisplayedRecorded();
        assertInfoBarClosingStateRecorded(InfoBarClosingState.VISIBLE_INDIRECT);
    }

    // Inspired by crbug.com/1245624: When tab is destroyed, InfoBarDisplayed should not be
    // recorded.
    @Test
    public void testMessages_Dismiss_Destroy() {
        presentMessages();
        PropertyModel messageModel = mMessagePropertyCaptor.getValue();

        Mockito.doReturn(true).when(mMockTab).isDestroyed();
        Mockito.doReturn(true).when(mActivity).isDestroyed();
        int[] dismissReasons = {DismissReason.TAB_DESTROYED, DismissReason.ACTIVITY_DESTROYED,
                DismissReason.SCOPE_DESTROYED};
        for (int reason : dismissReasons) {
            messageModel.get(MessageBannerProperties.ON_DISMISSED).onResult(reason);
            assertInfoBarDisplayedNotRecorded(
                    "Messages destroyed should not directly result in close state recorded.");
        }
    }

    @Test
    public void testMessages_Dismiss_OnTabHidden() {
        presentMessages();
        PropertyModel messageModel = mMessagePropertyCaptor.getValue();

        mTabObserver.onHidden(mMockTab, TabHidingType.ACTIVITY_HIDDEN);
        verify(mMessageDispatcher).dismissMessage(messageModel, DismissReason.TAB_SWITCHED);

        // Simulate the invocation of the message dismissal callback.
        messageModel.get(MessageBannerProperties.ON_DISMISSED).onResult(DismissReason.TAB_SWITCHED);
        assertInfoBarDisplayedRecorded();
        assertInfoBarClosingStateRecorded(InfoBarClosingState.UNKNOWN);
    }

    @Test
    public void testMessages_Dismiss_OnResumeActivity() {
        presentMessages();
        PropertyModel messageModel = mMessagePropertyCaptor.getValue();

        // Simulate survey expiration.
        mTestSurveyController.isSurveyExpired = true;
        mLifecycleObserver.onResumeWithNative();
        verify(mMessageDispatcher).dismissMessage(messageModel, DismissReason.DISMISSED_BY_FEATURE);

        // Simulate the invocation of the message dismissal callback.
        messageModel.get(MessageBannerProperties.ON_DISMISSED)
                .onResult(DismissReason.DISMISSED_BY_FEATURE);
        Assert.assertFalse("SharedPreference for InfoBarShown should not be recorded.",
                SharedPreferencesManager.getInstance().contains(mPrefKeyPromptShown));
        assertInfoBarClosingStateRecorded(InfoBarClosingState.UNKNOWN);
    }

    private void initializeChromeSurveyController() {
        ChromeSurveyController.initialize(
                mMockModelSelector, mMockLifecycleDispatcher, mActivity, mMessageDispatcher);
        try {
            BackgroundShadowAsyncTask.runBackgroundTasks();
        } catch (Exception e) {
            throw new AssertionError("#runBackgroundTasks failed", e);
        }
        ShadowLooper.runUiThreadTasks();
    }

    private void presentSurveyInfoBarInValidTab() {
        setupTabMocks();
        initializeChromeSurveyController();
        assertCallbackAssignedInSurveyController();

        // Verify the survey should be attempted to present on a valid tab.
        mockTabReady();
        mTestSurveyController.onDownloadSuccessRunnable.run();
        assertSurveyInfoBarShown(true);
    }

    private void presentMessages() {
        ShadowChromeFeatureList.sEnableMessages = true;
        setupTabMocks();
        initializeChromeSurveyController();
        assertCallbackAssignedInSurveyController();
        Mockito.doAnswer(invocation -> {
                   mLifecycleObserver = invocation.getArgument(0);
                   return null;
               })
                .when(mMockLifecycleDispatcher)
                .register(any());

        // Verify the survey should be attempted to present on a valid tab.
        mockTabReady();
        mTestSurveyController.onDownloadSuccessRunnable.run();
        assertSurveyMessagesEnqueued();
        Assert.assertNotNull("mTabObserver is null.", mTabObserver);
        Assert.assertNotNull("mLifecycleObserver is null.", mLifecycleObserver);
    }

    private void mockTabReady() {
        Mockito.doReturn(false).when(mMockTab).isLoading();
        Mockito.doReturn(true).when(mMockTab).isUserInteractable();
    }

    private void setupTabMocks() {
        Mockito.when(mMockModelSelector.getCurrentTab()).thenReturn(mMockTab);
        Mockito.doAnswer(invocation -> {
                   mTabModelSelectorObserver = invocation.getArgument(0);
                   return null;
               })
                .when(mMockModelSelector)
                .addObserver(any());
        Mockito.doAnswer(invocation -> {
                   if (mTabModelSelectorObserver == invocation.getArgument(0)) {
                       mTabModelSelectorObserver = null;
                   }
                   return null;
               })
                .when(mMockModelSelector)
                .removeObserver(any());

        // Make the mock tab always valid. The cases with invalid tab are tested in
        // ChromeSurveyControllerTest.
        Mockito.when(mMockTab.getWebContents()).thenReturn(mMockWebContent);
        Mockito.when(mMockTab.isIncognito()).thenReturn(false);
        Mockito.doAnswer(invocation -> {
                   mTabObserver = invocation.getArgument(0);
                   return null;
               })
                .when(mMockTab)
                .addObserver(any());
    }

    private void assertCallbackAssignedInSurveyController() {
        Assert.assertNotNull("onDownloadSuccessRunnable is null.",
                mTestSurveyController.onDownloadSuccessRunnable);
        Assert.assertNotNull("onDownloadFailureRunnable is null.",
                mTestSurveyController.onDownloadFailureRunnable);
    }

    private void assertSurveyInfoBarShown(boolean shown) {
        Assert.assertEquals("presentSurvey should triggered.", shown ? 1 : 0,
                ShadowSurveyInfoBar.sShowInfoBarCallback.getCallCount());
        if (shown) {
            Assert.assertNotNull("SurveyInfoBarDelegate is null.",
                    ShadowSurveyInfoBar.sShowInfoBarCallback.getOnlyPayloadBlocking());
        }
    }

    private void assertSurveyMessagesEnqueued() {
        verify(mMessageDispatcher)
                .enqueueWindowScopedMessage(mMessagePropertyCaptor.capture(), eq(false));
        Assert.assertNotNull("Message captor is null.", mMessagePropertyCaptor.getValue());
    }

    private void assertInfoBarClosingStateRecorded(@InfoBarClosingState int state) {
        int count = RecordHistogram.getHistogramValueCountForTesting(
                "Android.Survey.InfoBarClosingState", state);
        Assert.assertEquals(
                String.format("InfoBarClosingState for state <%d> is not recorded.", state), 1,
                count);
    }

    private void assertInfoBarDisplayedRecorded() {
        if (mTabObserver != null) {
            verify(mMockTab).removeObserver(mTabObserver);
        }
        if (mLifecycleObserver != null) {
            verify(mMockLifecycleDispatcher).unregister(mLifecycleObserver);
        }
        Assert.assertTrue("SharedPreference for InfoBarShown is not recorded.",
                SharedPreferencesManager.getInstance().contains(mPrefKeyPromptShown));
    }

    private void assertInfoBarDisplayedNotRecorded(String reason) {
        Assert.assertFalse(
                reason, SharedPreferencesManager.getInstance().contains(mPrefKeyPromptShown));
    }

    private void assertDownloadAttempted(boolean attempted) {
        int expectedCount = attempted ? 1 : 0;
        Assert.assertEquals("Times of download triggered does not match.", expectedCount,
                mTestSurveyController.downloadIfApplicableCallback.getCallCount());
        Assert.assertEquals("Download attempt count is not recorded as expected.", expectedCount,
                mSharedPreferencesManager.readInt(mPrefKeyDownloadAttempts));
    }

    private void assertDownloadAttemptRecordedWithSample(int sample) {
        Assert.assertEquals(String.format("<Android.Survey.DownloadAttemptsBeforeAccepted> "
                                            + "with sample <%d> is not recorded.",
                                    sample),
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.Survey.DownloadAttemptsBeforeAccepted", sample));
    }

    private static class TestSurveyController extends SurveyController {
        public final CallbackHelper downloadIfApplicableCallback = new CallbackHelper();
        public final CallbackHelper showSurveyIfAvailableCallback = new CallbackHelper();
        public boolean isSurveyExpired;

        @Nullable
        public Runnable onDownloadSuccessRunnable;
        @Nullable
        public Runnable onDownloadFailureRunnable;

        @Override
        public void downloadSurvey(Context context, String triggerId, Runnable onSuccessRunnable,
                Runnable onFailureRunnable) {
            downloadIfApplicableCallback.notifyCalled();

            onDownloadSuccessRunnable = onSuccessRunnable;
            onDownloadFailureRunnable = onFailureRunnable;
        }

        @Override
        public void showSurveyIfAvailable(Activity activity, String siteId,
                boolean showAsBottomSheet, int displayLogoResId,
                @Nullable ActivityLifecycleDispatcher lifecycleDispatcher) {
            showSurveyIfAvailableCallback.notifyCalled();
        }

        @Override
        public boolean isSurveyExpired(String triggerId) {
            return isSurveyExpired;
        }
    }
}
