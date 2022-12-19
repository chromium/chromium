// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.verifyZeroInteractions;

import android.app.Activity;
import android.content.Context;

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
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.CommandLine;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.task.test.BackgroundShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
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
import java.util.Map.Entry;

/**
 * "Integration" style unit tests for {@link ChromeSurveyController} that mocks most of the
 * info bar related code, aiming to cover the workflow from initialize survey download task.
 */
// clang-format off
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {BackgroundShadowAsyncTask.class})
//TODO(crbug.com/1210371): Rewrite using paused loop. See crbug for details.
@LooperMode(LooperMode.Mode.LEGACY)
// Set user is selected and by pass the rate limit. The rate limiting logic is tested in
// ChromeSurveyControllerTest.
@CommandLineFlags.Add(ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY)
public class ChromeSurveyControllerFlowTest {
    // clang-format on
    private static final String TEST_TRIGGER_ID = "test_trigger_id";

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

    Map<String, String> mFieldTrialParams;

    @Before
    public void setup() {
        mFieldTrialParams = new HashMap<>();
        mFieldTrialParams.put(ChromeSurveyController.SITE_ID_PARAM_NAME, TEST_TRIGGER_ID);
        // By setting MAX_NUMBER to 1, #isRandomSelectedBySurvey is always true.
        mFieldTrialParams.put(ChromeSurveyController.MAX_NUMBER, "1");
        enableChromeSurveyNextFeatureWithParams(mFieldTrialParams, true);

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
        FeatureList.setTestValues(null);
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

        enableChromeSurveyNextFeatureWithParams(null, false);
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
        mFieldTrialParams.put(ChromeSurveyController.MAX_DOWNLOAD_ATTEMPTS, "0");
        enableChromeSurveyNextFeatureWithParams(mFieldTrialParams, true);

        initializeChromeSurveyController();
        assertDownloadAttempted(true);
    }

    @Test
    public void testStartDownloadIfEligibleTask_DownloadWithinCap() {
        CommandLine.getInstance().removeSwitch(ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY);
        mFieldTrialParams.put(ChromeSurveyController.MAX_DOWNLOAD_ATTEMPTS, "99");
        enableChromeSurveyNextFeatureWithParams(mFieldTrialParams, true);

        assertDownloadAttempted(false);
        initializeChromeSurveyController();
        assertDownloadAttempted(true);
    }

    @Test
    public void testStartDownloadIfEligibleTask_DownloadReachCap() {
        CommandLine.getInstance().removeSwitch(ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY);
        mFieldTrialParams.put(ChromeSurveyController.MAX_DOWNLOAD_ATTEMPTS, "2");
        enableChromeSurveyNextFeatureWithParams(mFieldTrialParams, true);
        mSharedPreferencesManager.writeInt(mPrefKeyDownloadAttempts, 2);

        initializeChromeSurveyController();
        Assert.assertEquals("Download should not be triggered.", 0,
                mTestSurveyController.downloadIfApplicableCallback.getCallCount());
    }

    @Test
    public void testStartDownloadIfEligibleTask_DownloadCapZero_ForceEnable() {
        mFieldTrialParams.put(ChromeSurveyController.MAX_DOWNLOAD_ATTEMPTS, "0");
        enableChromeSurveyNextFeatureWithParams(mFieldTrialParams, true);

        assertDownloadAttempted(false);
        initializeChromeSurveyController();
        assertDownloadAttempted(true);
    }

    @Test
    public void testSurveyMessagesUI() {
        setupTabMocks();
        initializeChromeSurveyController();
        assertCallbackAssignedInSurveyController();

        // Verify the survey should be attempted to present on a valid tab.
        mockTabReady();
        Assert.assertNotNull(mMessageDispatcher);
        mTestSurveyController.onDownloadSuccessRunnable.run();
        assertSurveyMessageEnqueued(true);
    }

    @Test
    public void testPresentSurvey_LoadingTab() {
        setupTabMocks();
        initializeChromeSurveyController();
        assertCallbackAssignedInSurveyController();

        // Verify the survey should be attempted to present on a valid tab.
        Mockito.doReturn(true).when(mMockTab).isLoading();
        mTestSurveyController.onDownloadSuccessRunnable.run();

        assertSurveyMessageEnqueued(false);
        Assert.assertNotNull("Tab observer should be registered.", mTabObserver);

        // Assume tab loading is complete.
        mockTabReady();
        mTabObserver.onPageLoadFinished(mMockTab, null);
        assertSurveyMessageEnqueued(true);
    }

    @Test
    public void testPresentSurvey_TabInteractabilityChanged() {
        setupTabMocks();
        initializeChromeSurveyController();
        assertCallbackAssignedInSurveyController();

        // Verify the survey should be attempted to present on a valid tab.
        Mockito.doReturn(false).when(mMockTab).isUserInteractable();
        mTestSurveyController.onDownloadSuccessRunnable.run();

        assertSurveyMessageEnqueued(false);
        Assert.assertNotNull("Tab observer should be registered.", mTabObserver);

        // Assume tab loading is complete.
        mockTabReady();
        mTabObserver.onInteractabilityChanged(mMockTab, true);
        assertSurveyMessageEnqueued(true);
    }

    @Test
    public void testPresentSurvey_SwitchingTab() {
        setupTabMocks();
        initializeChromeSurveyController();
        assertCallbackAssignedInSurveyController();

        // Verify the survey should be attempted to present on a valid tab.
        Mockito.when(mMockModelSelector.getCurrentTab()).thenReturn(null);
        mTestSurveyController.onDownloadSuccessRunnable.run();

        assertSurveyMessageEnqueued(false);
        Assert.assertNotNull(
                "TabModelSelectorObserver should be registered.", mTabModelSelectorObserver);

        // Assume tab selector can provide a tab (e.g. switch to a fully loaded tab).
        mockTabReady();
        Mockito.when(mMockModelSelector.getCurrentTab()).thenReturn(mMockTab);
        mTabModelSelectorObserver.onChange();
        assertSurveyMessageEnqueued(true);
    }

    @Test
    public void testPresentSurvey_UmaDisabledBeforeTabReady() {
        setupTabMocks();
        initializeChromeSurveyController();
        assertCallbackAssignedInSurveyController();

        // Verify the survey should be attempted to present on a valid tab.
        Mockito.when(mMockModelSelector.getCurrentTab()).thenReturn(null);
        mTestSurveyController.onDownloadSuccessRunnable.run();

        assertSurveyMessageEnqueued(false);
        Assert.assertNotNull(
                "TabModelSelectorObserver should be registered.", mTabModelSelectorObserver);

        // Assume user turn off UMA upload before survey is shown.
        ChromeSurveyController.forceIsUMAEnabledForTesting(false);

        mockTabReady();
        Mockito.when(mMockModelSelector.getCurrentTab()).thenReturn(mMockTab);
        mTabModelSelectorObserver.onChange();
        assertSurveyMessageEnqueued(false);
        Assert.assertNull("TabModelSelectorObserver is unregistered since UMA upload is disabled.",
                mTabModelSelectorObserver);
    }

    @Test
    public void testMessages_PrimaryAction() {
        presentMessages();
        PropertyModel messageModel = mMessagePropertyCaptor.getValue();

        messageModel.get(MessageBannerProperties.ON_DISMISSED)
                .onResult(DismissReason.PRIMARY_ACTION);
        assertPromptDisplayedRecorded();
        assertDownloadAttemptRecordedWithSample(1);
    }

    @Test
    public void testMessages_PrimaryAction_DownloadBefore() {
        final int downloadAttempted = 3;
        mSharedPreferencesManager.writeInt(mPrefKeyDownloadAttempts, downloadAttempted);

        presentMessages();
        PropertyModel messageModel = mMessagePropertyCaptor.getValue();

        messageModel.get(MessageBannerProperties.ON_DISMISSED)
                .onResult(DismissReason.PRIMARY_ACTION);
        assertPromptDisplayedRecorded();
        assertDownloadAttemptRecordedWithSample(downloadAttempted + 1);
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
    public void testMessages_MessageShownOnce() {
        presentMessages();
        Assert.assertTrue("Message should be shown.", ChromeSurveyController.isMessageShown());
        verify(mMessageDispatcher).enqueueWindowScopedMessage(any(), anyBoolean());

        // Simulate survey download that triggers invocation of #showSurveyPrompt.
        mTestSurveyController.onDownloadSuccessRunnable.run();
        verifyNoMoreInteractions(mMessageDispatcher);
    }

    @Test
    public void testMessages_Dismiss_Gesture() {
        presentMessages();
        PropertyModel messageModel = mMessagePropertyCaptor.getValue();

        messageModel.get(MessageBannerProperties.ON_DISMISSED).onResult(DismissReason.GESTURE);
        assertPromptDisplayedRecorded();
    }

    @Test
    public void testMessages_Dismiss_Timer() {
        presentMessages();
        PropertyModel messageModel = mMessagePropertyCaptor.getValue();

        messageModel.get(MessageBannerProperties.ON_DISMISSED).onResult(DismissReason.TIMER);
        assertPromptDisplayedRecorded();
    }

    // Inspired by crbug.com/1245624: When tab is destroyed, PromptDisplayed should not be
    // recorded.
    @Test
    public void testMessages_Dismiss_Destroy() {
        presentMessages();
        PropertyModel messageModel = mMessagePropertyCaptor.getValue();

        int[] dismissReasons = {DismissReason.TAB_DESTROYED, DismissReason.ACTIVITY_DESTROYED,
                DismissReason.SCOPE_DESTROYED};
        for (int reason : dismissReasons) {
            messageModel.get(MessageBannerProperties.ON_DISMISSED).onResult(reason);
            assertPromptDisplayedNotRecorded(
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
        assertPromptDisplayedRecorded();
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
        Assert.assertFalse("SharedPreference for PromptShown should not be recorded.",
                SharedPreferencesManager.getInstance().contains(mPrefKeyPromptShown));
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

    private void presentMessages() {
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
        assertSurveyMessageEnqueued(true);
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

    private void assertSurveyMessageEnqueued(boolean enqueued) {
        if (enqueued) {
            verify(mMessageDispatcher)
                    .enqueueWindowScopedMessage(mMessagePropertyCaptor.capture(), eq(false));
            Assert.assertNotNull("Message captor is null.", mMessagePropertyCaptor.getValue());
        } else {
            verify(mMessageDispatcher, never())
                    .enqueueWindowScopedMessage(mMessagePropertyCaptor.capture(), eq(false));
        }
    }

    private void assertPromptDisplayedRecorded() {
        if (mTabObserver != null) {
            verify(mMockTab).removeObserver(mTabObserver);
        }
        if (mLifecycleObserver != null) {
            verify(mMockLifecycleDispatcher).unregister(mLifecycleObserver);
        }
        Assert.assertTrue("SharedPreference for PromptShown is not recorded.",
                SharedPreferencesManager.getInstance().contains(mPrefKeyPromptShown));
    }

    private void assertPromptDisplayedNotRecorded(String reason) {
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

    private void enableChromeSurveyNextFeatureWithParams(
            Map<String, String> params, boolean enable) {
        TestValues testValues = new TestValues();
        String featureName = ChromeFeatureList.CHROME_SURVEY_NEXT_ANDROID;
        testValues.addFeatureFlagOverride(featureName, enable);
        if (params != null) {
            for (Entry<String, String> param : params.entrySet()) {
                testValues.addFieldTrialParamOverride(
                        featureName, param.getKey(), param.getValue());
            }
        }
        FeatureList.setTestValues(testValues);
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
