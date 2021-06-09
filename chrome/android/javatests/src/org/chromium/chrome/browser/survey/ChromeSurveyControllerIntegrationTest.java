// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import android.app.Activity;
import android.content.Context;
import android.support.test.filters.MediumTest;
import android.text.style.ClickableSpan;
import android.view.View;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.infobar.SurveyInfoBar;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.survey.ChromeSurveyController.InfoBarClosingState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarAnimationListener;
import org.chromium.components.infobars.InfoBarUiItem;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.List;
import java.util.concurrent.TimeoutException;

/** Integration test for {@link ChromeSurveyController} and {@link SurveyInfoBar}. */
// clang-format off
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.EnableFeatures(ChromeFeatureList.CHROME_SURVEY_NEXT_ANDROID)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY,
    ChromeSurveyController.COMMAND_LINE_PARAM_NAME + "=" +
        ChromeSurveyControllerIntegrationTest.TEST_TRIGGER_ID})
public class ChromeSurveyControllerIntegrationTest {
    // clang-format on
    static final String TEST_TRIGGER_ID = "test_trigger_id";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private SharedPreferencesManager mSharedPreferenceManager =
            SharedPreferencesManager.getInstance();
    private AlwaysSuccessfulSurveyController mTestSurveyController;
    private InfoBarAnimationListener mInfoBarAnimationListener;
    private CallbackHelper mAllAnimationsFinishedCallback;
    private String mPrefKey;

    @Before
    public void setUp() throws InterruptedException, TimeoutException {
        mAllAnimationsFinishedCallback = new CallbackHelper();
        mInfoBarAnimationListener = new InfoBarAnimationListener() {
            @Override
            public void notifyAnimationFinished(int animationType) {}

            @Override
            public void notifyAllAnimationsFinished(InfoBarUiItem frontInfoBar) {
                mAllAnimationsFinishedCallback.notifyCalled();
            }
        };

        ChromeSurveyController.forceIsUMAEnabledForTesting(true);
        mPrefKey = ChromePreferenceKeys.CHROME_SURVEY_PROMPT_DISPLAYED_TIMESTAMP.createKey(
                TEST_TRIGGER_ID);
        mSharedPreferenceManager = SharedPreferencesManager.getInstance();

        mTestSurveyController = new AlwaysSuccessfulSurveyController();
        SurveyController.setInstanceForTesting(mTestSurveyController);

        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.getInfoBarContainer().addAnimationListener(mInfoBarAnimationListener);
        waitUntilInfoBarPresented();
    }

    @After
    public void tearDown() {
        mActivityTestRule.getInfoBarContainer().removeAnimationListener(mInfoBarAnimationListener);
        SurveyController.setInstanceForTesting(null);
        mSharedPreferenceManager.removeKey(mPrefKey);
        ChromeSurveyController.forceIsUMAEnabledForTesting(false);
    }

    @Test
    @MediumTest
    public void testInfoBarClicked() throws TimeoutException {
        SurveyInfoBar surveyInfoBar = (SurveyInfoBar) getSurveyInfoBar();
        Assert.assertNotNull("SurveyInfoBar should not be null.", surveyInfoBar);

        int count = mAllAnimationsFinishedCallback.getCallCount();
        ClickableSpan clickableSpan = surveyInfoBar.getClickableSpan();
        TestThreadUtils.runOnUiThreadBlocking(() -> clickableSpan.onClick(null));
        mAllAnimationsFinishedCallback.waitForCallback(count);

        Assert.assertEquals("#showSurveyIfAvailable should be attempted.", 1,
                mTestSurveyController.showSurveyCallbackHelper.getCallCount());
        assertInfoBarClosingStateRecorded(InfoBarClosingState.ACCEPTED_SURVEY);
    }

    @Test
    @MediumTest
    public void testInfoBarClose() throws TimeoutException {
        InfoBar surveyInfoBar = getSurveyInfoBar();
        Assert.assertNotNull("SurveyInfoBar should not be null.", surveyInfoBar);

        int count = mAllAnimationsFinishedCallback.getCallCount();
        InfoBarUtil.clickCloseButton(surveyInfoBar);
        mAllAnimationsFinishedCallback.waitForCallback(count);

        assertInfoBarClosingStateRecorded(InfoBarClosingState.CLOSE_BUTTON);
    }

    @Test
    @MediumTest
    public void testNoInfoBarInNewTab() throws InterruptedException {
        waitUntilInfoBarStateRecorded();

        // Info bar should not displayed in another tab.
        mActivityTestRule.loadUrlInNewTab("about:blank", false);
        assertInfoBarClosingStateRecorded(InfoBarClosingState.VISIBLE_INDIRECT);

        Tab tabTwo = mActivityTestRule.getActivity().getActivityTab();
        waitUntilTabIsReady(tabTwo);
        Assert.assertNull("Tab two should not have the infobar", getSurveyInfoBar());
    }

    private void waitUntilInfoBarPresented() throws TimeoutException {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        waitUntilTabIsReady(tab);
        mTestSurveyController.downloadCallbackHelper.waitForFirst();
        Assert.assertNotNull("Tab should have an info bar", getSurveyInfoBar());
        Assert.assertEquals("Info Bars are not visible.", View.VISIBLE,
                mActivityTestRule.getInfoBarContainer().getVisibility());
    }

    private void waitUntilInfoBarStateRecorded() throws InterruptedException {
        Thread.sleep(ChromeSurveyController.getRequiredVisibilityDurationMs());
        CriteriaHelper.pollUiThread(
                () -> SharedPreferencesManager.getInstance().contains(mPrefKey));
    }

    private void waitUntilTabIsReady(Tab tab) {
        CriteriaHelper.pollUiThread(() -> !tab.isLoading() && tab.isUserInteractable());
    }

    private InfoBar getSurveyInfoBar() {
        List<InfoBar> infoBars = mActivityTestRule.getInfoBarContainer().getInfoBarsForTesting();
        for (int i = 0; i < infoBars.size(); i++) {
            if (infoBars.get(i).getInfoBarIdentifier()
                    == InfoBarIdentifier.SURVEY_INFOBAR_ANDROID) {
                return infoBars.get(i);
            }
        }
        return null;
    }

    private void assertInfoBarClosingStateRecorded(@InfoBarClosingState int state) {
        int count = RecordHistogram.getHistogramValueCountForTesting(
                "Android.Survey.InfoBarClosingState", state);
        Assert.assertEquals(
                String.format("InfoBarClosingState for state <%d> is not recorded.", state), 1,
                count);
    }

    private static class AlwaysSuccessfulSurveyController extends SurveyController {
        public final CallbackHelper downloadCallbackHelper = new CallbackHelper();
        public final CallbackHelper showSurveyCallbackHelper = new CallbackHelper();

        @Override
        public void downloadSurvey(Context context, String triggerId, Runnable onSuccessRunnable,
                Runnable onFailureRunnable) {
            downloadCallbackHelper.notifyCalled();

            Assert.assertEquals(TEST_TRIGGER_ID, triggerId);
            onSuccessRunnable.run();
        }

        @Override
        public void showSurveyIfAvailable(Activity activity, String siteId,
                boolean showAsBottomSheet, int displayLogoResId,
                ActivityLifecycleDispatcher lifecycleDispatcher) {
            showSurveyCallbackHelper.notifyCalled();
        }
    }
}
