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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.hats.SurveyThrottler;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.content_public.browser.WebContents;

/**
 * Unit tests for {@link ChromeSurveyController} and {@link SurveyThrottler}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeSurveyControllerTest {
    private static final String TEST_SURVEY_TRIGGER_ID = "foobar";

    private TestChromeSurveyController mTestController;

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
        Assert.assertNull("Tab should be null", mTestController.getLastTabPromptShown());
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
