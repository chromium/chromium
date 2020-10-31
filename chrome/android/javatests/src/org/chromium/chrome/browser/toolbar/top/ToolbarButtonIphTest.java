// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.toolbar.top.ButtonHighlightMatcher.withHighlight;

import android.os.Looper;

import androidx.test.espresso.ViewInteraction;
import androidx.test.espresso.action.ViewActions;
import androidx.test.espresso.assertion.ViewAssertions;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Integration tests for showing IPH bubbles on the toolbar.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.TOOLBAR_IPH_ANDROID)
public class ToolbarButtonIphTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock
    private Tracker mTracker;

    @Before
    public void setUp() {
        Looper.prepare();
        MockitoAnnotations.initMocks(this);
        // Pretend the feature engagement feature is already initialized. Otherwise
        // UserEducationHelper#requestShowIPH() calls get dropped during test.
        doAnswer(invocation -> {
            invocation.<Callback<Boolean>>getArgument(0).onResult(true);
            return null;
        })
                .when(mTracker)
                .addOnInitializedCallback(any());
        TrackerFactory.setTrackerForTests(mTracker);

        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        TrackerFactory.setTrackerForTests(null);
    }

    @Test
    @MediumTest
    public void testNewTabButtonIph() {
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.NEW_TAB_PAGE_HOME_BUTTON_FEATURE))
                .thenReturn(true);

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        onView(withId(R.id.home_button)).check(matches(withHighlight(false)));

        mActivityTestRule.loadUrl("about:blank");
        onView(withId(R.id.home_button)).check(matches(withHighlight(true)));
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testTabSwitcherEventEnabled() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button).performClick();
        });
        verify(mTracker, times(1)).notifyEvent(EventConstants.TAB_SWITCHER_BUTTON_CLICKED);
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @Features.DisableFeatures(ChromeFeatureList.TOOLBAR_IPH_ANDROID)
    public void testTabSwitcherEventDisabled() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().findViewById(R.id.tab_switcher_button).performClick();
        });
        verify(mTracker, never()).notifyEvent(EventConstants.TAB_SWITCHER_BUTTON_CLICKED);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1144263")
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testTabSwitcherButtonIph() {
        when(mTracker.shouldTriggerHelpUI(FeatureConstants.TAB_SWITCHER_BUTTON_FEATURE))
                .thenReturn(true);

        // Navigating to about:blank here was flaky for some reason, probably because the page was
        // already on about:blank.
        mActivityTestRule.loadUrl("chrome://about/");
        ViewInteraction toolbarTabButtonInteraction = onView(withId(R.id.tab_switcher_button));
        toolbarTabButtonInteraction.check(ViewAssertions.matches(withHighlight(true)));

        toolbarTabButtonInteraction.perform(ViewActions.click());
        onView(withId(R.id.new_tab_button)).check(ViewAssertions.matches(withHighlight(true)));

        onView(withId(R.id.tab_switcher_mode_tab_switcher_button)).perform(ViewActions.click());
        toolbarTabButtonInteraction.check(ViewAssertions.matches(withHighlight(false)));
    }
}
