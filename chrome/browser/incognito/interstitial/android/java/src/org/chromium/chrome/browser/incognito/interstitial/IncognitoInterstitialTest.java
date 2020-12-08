// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.interstitial;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.support.test.runner.lifecycle.Stage;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DummyUiActivity;

/**
 * Instrumentation test class for the incognito interstitial.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class IncognitoInterstitialTest {
    @Mock
    private IncognitoInterstitialDelegate mIncognitoInterstitialDelegateMock;

    @Mock
    private Runnable mOnIncognitoTabOpenedMock;

    @Rule
    public final BaseActivityTestRule<DummyUiActivity> mActivityTestRule =
            new BaseActivityTestRule<>(DummyUiActivity.class);

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        DummyUiActivity.setTestLayout(R.layout.incognito_interstitial_bottom_sheet_view);
    }

    @AfterClass
    public static void tearDown() {
        DummyUiActivity.setTestLayout(0);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.launchActivity(null);
        ApplicationTestUtils.waitForActivityState(mActivityTestRule.getActivity(), Stage.RESUMED);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            View contentView = mActivityTestRule.getActivity().findViewById(android.R.id.content);
            IncognitoInterstitialCoordinator incognitoInterstitialCoordinator =
                    new IncognitoInterstitialCoordinator(contentView,
                            mIncognitoInterstitialDelegateMock, mOnIncognitoTabOpenedMock);
        });
    }

    @Test
    @MediumTest
    public void testIncognitoInterstitialElementsShownCorrectly() {
        onView(withId(R.id.incognito_interstitial_learn_more)).check(matches(isDisplayed()));
        onView(withId(R.id.incognito_interstitial_continue_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testClickOnLearnMoreButton() {
        onView(withId(R.id.incognito_interstitial_learn_more)).perform(click());
        verify(mIncognitoInterstitialDelegateMock).openLearnMorePage();
        verify(mOnIncognitoTabOpenedMock, never()).run();
    }

    @Test
    @MediumTest
    public void testClickOnContinueButton() {
        onView(withId(R.id.incognito_interstitial_continue_button)).perform(click());
        verify(mOnIncognitoTabOpenedMock).run();
        verify(mIncognitoInterstitialDelegateMock).openCurrentUrlInIncognitoTab();
    }
}
