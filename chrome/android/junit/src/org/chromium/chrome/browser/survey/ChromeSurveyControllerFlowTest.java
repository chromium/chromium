// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import static org.mockito.ArgumentMatchers.any;

import android.content.Context;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.RealObject;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.CommandLine;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.task.test.BackgroundShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.infobar.SurveyInfoBar;
import org.chromium.chrome.browser.infobar.SurveyInfoBarDelegate;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.content_public.browser.WebContents;

import java.util.HashMap;
import java.util.Map;

/**
 * "Integration" style unit tests for {@link ChromeSurveyController} that mocks most of the
 * info bar related code, aiming to cover the workflow from initialize survey download task.
 */
// clang-format off
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = { BackgroundShadowAsyncTask.class, ShadowRecordHistogram.class,
            ChromeSurveyControllerFlowTest.ShadowChromeFeatureList.class,
            ChromeSurveyControllerFlowTest.ShadowSurveyInfoBar.class,
            ChromeSurveyControllerFlowTest.ShadowInfoBarContainer.class
        })
public class ChromeSurveyControllerFlowTest {
    // clang-format on
    private static final String TEST_TRIGGER_ID = "test_trigger_id";

    @Implements(ChromeFeatureList.class)
    static class ShadowChromeFeatureList {
        static final Map<String, String> sParamValues = new HashMap<>();
        static boolean sEnableSurvey;

        @Implementation
        public static boolean isEnabled(String featureName) {
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
        static CallbackHelper sShowInfoBarCallback;

        @Implementation
        public static void showSurveyInfoBar(WebContents webContents, String siteId,
                boolean showAsBottomSheet, int displayLogoResId,
                SurveyInfoBarDelegate surveyInfoBarDelegate) {
            Assert.assertNotNull("sShowInfoBarCallback is null.", sShowInfoBarCallback);
            sShowInfoBarCallback.notifyCalled();
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

    private final TestSurveyController mTestSurveyController = new TestSurveyController();

    private String mPrefKey;
    private SharedPreferencesManager mSharedPreferencesManager;

    private TabModelSelectorObserver mTabModelSelectorObserver;
    private TabObserver mTabObserver;

    @Before
    public void setup() {
        ShadowChromeFeatureList.sEnableSurvey = true;
        ShadowChromeFeatureList.sParamValues.put(
                ChromeSurveyController.SITE_ID_PARAM_NAME, TEST_TRIGGER_ID);
        ShadowChromeFeatureList.sParamValues.put(ChromeSurveyController.MAX_NUMBER, "99");
        ShadowInfoBarContainer.sInfoBarContainer = mMockInfoBarContainer;
        ShadowSurveyInfoBar.sShowInfoBarCallback = new CallbackHelper();

        // Set user is selected and by pass the rate limit. The rate limiting logic is tested in
        // ChromeSurveyControllerTest.
        CommandLine.getInstance().appendSwitch(ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY);

        SurveyController.setInstanceForTesting(mTestSurveyController);

        ChromeSurveyController.forceIsUMAEnabledForTesting(true);
        mPrefKey = ChromePreferenceKeys.CHROME_SURVEY_PROMPT_DISPLAYED_TIMESTAMP.createKey(
                TEST_TRIGGER_ID);
        mSharedPreferencesManager = SharedPreferencesManager.getInstance();
    }

    @After
    public void tearDown() {
        ChromeSurveyController.forceIsUMAEnabledForTesting(false);
        ShadowChromeFeatureList.sParamValues.clear();
        ShadowChromeFeatureList.sEnableSurvey = false;

        CommandLine.getInstance().removeSwitch(ChromeSurveyController.COMMAND_LINE_PARAM_NAME);
        CommandLine.getInstance().removeSwitch(ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY);
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
        initializeChromeSurveyController();

        Assert.assertEquals("Download should be triggered.", 1,
                mTestSurveyController.downloadIfApplicableCallback.getCallCount());
    }

    @Test
    public void testStartDownloadIfEligibleTask_ShowedBefore() {
        mSharedPreferencesManager.writeLong(mPrefKey, 1000L);

        initializeChromeSurveyController();

        Assert.assertEquals("Download should not trigger for user that has seen the survey prompt.",
                0, mTestSurveyController.downloadIfApplicableCallback.getCallCount());
    }

    @Test
    public void testStartDownloadIfEligibleTask_UmaDisabled() {
        ChromeSurveyController.forceIsUMAEnabledForTesting(false);
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.PRIVACY_METRICS_REPORTING, false);

        initializeChromeSurveyController();

        Assert.assertEquals("Download should not be triggered.", 0,
                mTestSurveyController.downloadIfApplicableCallback.getCallCount());
    }

    @Test
    public void testPresentSurvey_ValidTab() {
        setupTabMocks();
        initializeChromeSurveyController();
        assertCallbackAssignedInSurveyController();

        // Verify the survey should be attempted to present on a valid tab.
        mockTabReady();
        mTestSurveyController.onDownloadSuccessRunnable.run();

        Assert.assertEquals("presentSurvey should be triggered.", 1,
                ShadowSurveyInfoBar.sShowInfoBarCallback.getCallCount());
    }

    @Test
    public void testPresentSurvey_LoadingTab() {
        setupTabMocks();
        initializeChromeSurveyController();
        assertCallbackAssignedInSurveyController();

        // Verify the survey should be attempted to present on a valid tab.
        Mockito.doReturn(true).when(mMockTab).isLoading();
        mTestSurveyController.onDownloadSuccessRunnable.run();

        Assert.assertEquals("presentSurvey should not be triggered.", 0,
                ShadowSurveyInfoBar.sShowInfoBarCallback.getCallCount());
        Assert.assertNotNull("Tab observer should be registered.", mTabObserver);

        // Assume tab loading is complete.
        mockTabReady();
        mTabObserver.onPageLoadFinished(mMockTab, null);
        Assert.assertEquals("presentSurvey should be triggered.", 1,
                ShadowSurveyInfoBar.sShowInfoBarCallback.getCallCount());
    }

    @Test
    public void testPresentSurvey_TabInteractabilityChanged() {
        setupTabMocks();
        initializeChromeSurveyController();
        assertCallbackAssignedInSurveyController();

        // Verify the survey should be attempted to present on a valid tab.
        Mockito.doReturn(false).when(mMockTab).isUserInteractable();
        mTestSurveyController.onDownloadSuccessRunnable.run();

        Assert.assertEquals("presentSurvey should not be triggered.", 0,
                ShadowSurveyInfoBar.sShowInfoBarCallback.getCallCount());
        Assert.assertNotNull("Tab observer should be registered.", mTabObserver);

        // Assume tab loading is complete.
        mockTabReady();
        mTabObserver.onInteractabilityChanged(mMockTab, true);
        Assert.assertEquals("presentSurvey should br triggered.", 1,
                ShadowSurveyInfoBar.sShowInfoBarCallback.getCallCount());
    }

    @Test
    public void testPresentSurvey_SwitchingTab() {
        setupTabMocks();
        initializeChromeSurveyController();
        assertCallbackAssignedInSurveyController();

        // Verify the survey should be attempted to present on a valid tab.
        Mockito.when(mMockModelSelector.getCurrentTab()).thenReturn(null);
        mTestSurveyController.onDownloadSuccessRunnable.run();

        Assert.assertEquals("presentSurvey should not be triggered.", 0,
                ShadowSurveyInfoBar.sShowInfoBarCallback.getCallCount());
        Assert.assertNotNull(
                "TabModelSelectorObserver should be registered.", mTabModelSelectorObserver);

        // Assume tab selector can provide a tab (e.g. switch to a fully loaded tab)
        mockTabReady();
        Mockito.when(mMockModelSelector.getCurrentTab()).thenReturn(mMockTab);
        mTabModelSelectorObserver.onChange();
        Assert.assertEquals("presentSurvey should triggered.", 1,
                ShadowSurveyInfoBar.sShowInfoBarCallback.getCallCount());
    }

    private void initializeChromeSurveyController() {
        ChromeSurveyController.initialize(mMockModelSelector);
        try {
            BackgroundShadowAsyncTask.runBackgroundTasks();
        } catch (Exception e) {
            throw new AssertionError("#runBackgroundTasks failed", e);
        }
        ShadowLooper.runUiThreadTasks();
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

    private static class TestSurveyController extends SurveyController {
        public final CallbackHelper downloadIfApplicableCallback = new CallbackHelper();

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
    }
}
