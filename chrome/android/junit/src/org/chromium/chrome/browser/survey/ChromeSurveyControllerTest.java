// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.annotation.NonNull;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.survey.ChromeSurveyController.FilteringResult;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.content_public.browser.WebContents;

/**
 * Unit tests for ChromeSurveyController.java.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeSurveyControllerTest {
    private static final String TEST_SURVEY_TRIGGER_ID = "foobar";

    private TestChromeSurveyController mTestController;
    private RiggedSurveyController mRiggedController;
    private SharedPreferencesManager mSharedPreferences;

    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    Tab mTab;
    @Mock
    WebContents mWebContents;
    @Mock
    TabModelSelector mSelector;
    @Mock
    ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock
    Activity mActivity;
    @Mock
    MessageDispatcher mMessageDispatcher;

    @Before
    public void before() {
        ChromeSurveyController.forceIsUMAEnabledForTesting(true);
        mTestController = new TestChromeSurveyController(TEST_SURVEY_TRIGGER_ID,
                mActivityLifecycleDispatcher, mActivity, mMessageDispatcher);
        mTestController.setTabModelSelector(mSelector);
        mSharedPreferences = SharedPreferencesManager.getInstance();
        Assert.assertNull("Tab should be null", mTestController.getLastTabPromptShown());
    }

    @After
    public void after() {
        FirstRunStatus.setFirstRunTriggered(false);
    }

    @Test
    public void testPromptDisplayedBefore() {
        final String triggerId1 = "triggerId1";
        final String triggerId2 = "triggerId2";

        TestChromeSurveyController controller1 = new TestChromeSurveyController(
                triggerId1, mActivityLifecycleDispatcher, mActivity, mMessageDispatcher);
        TestChromeSurveyController controller2 = new TestChromeSurveyController(
                triggerId2, mActivityLifecycleDispatcher, mActivity, mMessageDispatcher);

        String prefKey1 =
                ChromePreferenceKeys.CHROME_SURVEY_PROMPT_DISPLAYED_TIMESTAMP.createKey(triggerId1);
        String prefKey2 =
                ChromePreferenceKeys.CHROME_SURVEY_PROMPT_DISPLAYED_TIMESTAMP.createKey(triggerId2);

        // The new survey should not have been presented before.
        Assert.assertFalse("SharedPref for triggerId1 should not be recorded.",
                mSharedPreferences.contains(prefKey1));
        Assert.assertFalse("SharedPref for triggerId2 should not be recorded.",
                mSharedPreferences.contains(prefKey2));
        Assert.assertFalse(
                "Prompt for triggerId1 is marked displayed.", controller1.hasPromptBeenDisplayed());
        Assert.assertFalse(
                "Prompt for triggerId2 is marked displayed.", controller2.hasPromptBeenDisplayed());

        mSharedPreferences.writeLong(prefKey1, System.currentTimeMillis());
        Assert.assertTrue("Prompt for triggerId1 should be marked displayed.",
                controller1.hasPromptBeenDisplayed());
        Assert.assertFalse("Prompt for triggerId2 should not be marked displayed yet.",
                controller2.hasPromptBeenDisplayed());
        verifyFilteringResultRecorded(FilteringResult.SURVEY_PROMPT_ALREADY_DISPLAYED, 1);

        mSharedPreferences.writeLong(prefKey2, System.currentTimeMillis());
        Assert.assertTrue("Prompt for trggerId2 should be marked displayed.",
                controller2.hasPromptBeenDisplayed());
        verifyFilteringResultRecorded(FilteringResult.SURVEY_PROMPT_ALREADY_DISPLAYED, 2);
    }

    @Test
    public void testIsValidTabForSurvey_ValidTab() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(false).when(mTab).isIncognito();

        Assert.assertTrue(mTestController.isValidTabForSurvey(mTab));

        verify(mTab, atLeastOnce()).getWebContents();
        verify(mTab, atLeastOnce()).isIncognito();
    }

    @Test
    public void testIsValidTabForSurvey_NullTab() {
        Assert.assertFalse(mTestController.isValidTabForSurvey(null));
    }

    @Test
    public void testIsValidTabForSurvey_IncognitoTab() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(true).when(mTab).isIncognito();

        Assert.assertFalse(mTestController.isValidTabForSurvey(mTab));

        verify(mTab, atLeastOnce()).isIncognito();
    }

    @Test
    public void testIsValidTabForSurvey_NoWebContents() {
        doReturn(null).when(mTab).getWebContents();

        Assert.assertFalse(mTestController.isValidTabForSurvey(mTab));

        verify(mTab, atLeastOnce()).getWebContents();
        verify(mTab, never()).isIncognito();
    }

    @Test
    public void testShowPromptTabApplicable() {
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(false).when(mTab).isLoading();

        mTestController.showPromptIfApplicable(mTab, null, null);
        Assert.assertEquals("Tabs should be equal", mTab, mTestController.getLastTabPromptShown());
        verify(mTab, atLeastOnce()).isUserInteractable();
        verify(mTab, atLeastOnce()).isLoading();
    }

    @Test
    public void testShowPromptTabNotApplicable() {
        doReturn(false).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isLoading();

        mTestController.showPromptIfApplicable(mTab, null, null);
        Assert.assertNull("Tab should be null", mTestController.getLastTabPromptShown());
        verify(mTab, atLeastOnce()).isUserInteractable();
    }

    @Test
    public void testShowPromptUmaUploadNotEnabled() {
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isLoading();
        ChromeSurveyController.forceIsUMAEnabledForTesting(false);

        mTestController.showPromptIfApplicable(mTab, null, null);
        Assert.assertNull("Tab should be null", mTestController.getLastTabPromptShown());
        verify(mTab, atLeastOnce()).isUserInteractable();
    }

    @Test
    public void testSurveyAvailableWebContentsLoaded() {
        doReturn(mTab).when(mSelector).getCurrentTab();
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(false).when(mTab).isIncognito();
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(false).when(mWebContents).isLoading();

        mTestController.onSurveyAvailable(null);
        Assert.assertEquals("Tabs should be equal", mTab, mTestController.getLastTabPromptShown());

        verify(mSelector, atLeastOnce()).getCurrentTab();
        verify(mTab, atLeastOnce()).isIncognito();
        verify(mTab, atLeastOnce()).isUserInteractable();
        verify(mTab, atLeastOnce()).isLoading();
    }

    @Test
    public void testSurveyAvailableNullTab() {
        doReturn(null).when(mSelector).getCurrentTab();

        mTestController.onSurveyAvailable(null);
        Assert.assertNull("Tab should be null", mTestController.getLastTabPromptShown());
        verify(mSelector).addObserver(any());
    }

    @Test
    public void testSurveyAvailableUmaDisabled() {
        ChromeSurveyController.forceIsUMAEnabledForTesting(false);
        mTestController.onSurveyAvailable(null);
        Assert.assertNull("Tab should be null", mTestController.getLastTabPromptShown());
        verify(mSelector, never()).addObserver(any());
    }

    @Test
    public void testEligibilityFirstRun() {
        FirstRunStatus.setFirstRunTriggered(true);
        mRiggedController = new RiggedSurveyController(0, 1, 10);
        Assert.assertFalse("Random selection should be false",
                mRiggedController.isRandomlySelectedForSurvey());
        verifyFilteringResultRecorded(FilteringResult.FIRST_TIME_USER, 1);
    }

    @Test
    public void testEligibilityRolledYesterday() {
        mRiggedController = new RiggedSurveyController(0, 5, 10);
        mSharedPreferences.writeInt(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED, 4);
        Assert.assertTrue(
                "Random selection should be true", mRiggedController.isRandomlySelectedForSurvey());
        verifyFilteringResultRecorded(FilteringResult.USER_SELECTED_FOR_SURVEY, 1);
    }

    @Test
    public void testEligibilityRollingTwiceSameDay() {
        mRiggedController = new RiggedSurveyController(0, 5, 10);
        mSharedPreferences.writeInt(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED, 5);
        Assert.assertFalse("Random selection should be false",
                mRiggedController.isRandomlySelectedForSurvey());
        verifyFilteringResultRecorded(FilteringResult.USER_ALREADY_SAMPLED_TODAY, 1);
    }

    @Test
    public void testEligibilityFirstTimeRollingQualifies() {
        mRiggedController = new RiggedSurveyController(0, 5, 10);
        Assert.assertFalse(
                mSharedPreferences.contains(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED));
        Assert.assertTrue(
                "Random selection should be true", mRiggedController.isRandomlySelectedForSurvey());
        Assert.assertEquals("Numbers should match", 5,
                mSharedPreferences.readInt(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED, -1));
    }

    @Test
    public void testEligibilityFirstTimeRollingDoesNotQualify() {
        mRiggedController = new RiggedSurveyController(5, 1, 10);
        Assert.assertFalse(
                mSharedPreferences.contains(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED));
        Assert.assertFalse("Random selection should be false",
                mRiggedController.isRandomlySelectedForSurvey());
        Assert.assertEquals("Numbers should match", 1,
                mSharedPreferences.readInt(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED, -1));
        verifyFilteringResultRecorded(FilteringResult.ROLLED_NON_ZERO_NUMBER, 1);
    }

    @Test
    public void testEligibilityNoMaxNumber() {
        mRiggedController = new RiggedSurveyController(0, 1, -1);
        Assert.assertFalse(
                mSharedPreferences.contains(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED));
        Assert.assertFalse("Random selection should be false",
                mRiggedController.isRandomlySelectedForSurvey());
        Assert.assertFalse(
                mSharedPreferences.contains(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED));
        verifyFilteringResultRecorded(FilteringResult.MAX_NUMBER_MISSING, 1);
    }

    private void verifyFilteringResultRecorded(@FilteringResult int reason, int expectedCount) {
        int count = RecordHistogram.getHistogramValueCountForTesting(
                "Android.Survey.SurveyFilteringResults", reason);
        Assert.assertEquals(String.format("FilteringResult for type <%s> does not match.", reason),
                expectedCount, count);
    }

    /** Test class used to test the rate limiting logic for {@link ChromeSurveyController} */
    class RiggedSurveyController extends ChromeSurveyController {
        private int mRandomNumberToReturn;
        private int mDayOfYear;
        private int mMaxNumber;

        RiggedSurveyController(int randomNumberToReturn, int dayOfYear, int maxNumber) {
            super(TEST_SURVEY_TRIGGER_ID, null, null, null);
            mRandomNumberToReturn = randomNumberToReturn;
            mDayOfYear = dayOfYear;
            mMaxNumber = maxNumber;
        }

        @Override
        int getRandomNumberUpTo(int max) {
            return mRandomNumberToReturn;
        }

        @Override
        int getDayOfYear() {
            return mDayOfYear;
        }

        @Override
        int getMaxNumber() {
            return mMaxNumber;
        }
    }

    static class TestChromeSurveyController extends ChromeSurveyController {
        private Tab mTab;

        public TestChromeSurveyController(String triggerId,
                ActivityLifecycleDispatcher activityLifecycleDispatcher, Activity activity,
                MessageDispatcher messageDispatcher) {
            super(triggerId, activityLifecycleDispatcher, activity, messageDispatcher);
        }

        @Override
        void showSurveyPrompt(@NonNull Tab tab, String siteId) {
            mTab = tab;
        }

        public Tab getLastTabPromptShown() {
            return mTab;
        }
    }
}
