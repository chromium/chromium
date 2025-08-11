// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static androidx.test.espresso.assertion.ViewAssertions.matches;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.toolbar.top.ButtonHighlightMatcher.withHighlight;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerDetails;
import org.chromium.ui.base.DeviceFormFactor;

/** Integration tests for showing IPH bubbles on the toolbar. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ToolbarButtonIphTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Mock private Tracker mTracker;

    private WebPageStation mPage;

    @Before
    public void setUp() {
        // Pretend the feature engagement feature is already initialized. Otherwise
        // UserEducationHelper#requestShowIph() calls get dropped during test.
        doAnswer(
                        invocation -> {
                            invocation.<Callback<Boolean>>getArgument(0).onResult(true);
                            return null;
                        })
                .when(mTracker)
                .addOnInitializedCallback(any());
        TrackerFactory.setTrackerForTests(mTracker);

        when(mTracker.shouldTriggerHelpUiWithSnooze(any()))
                .thenReturn(new TriggerDetails(false, false));

        // Start on a page from the test server. This works around a bug that causes the top toolbar
        // to flicker. If the flicker happens while the IPH is visible, it will auto dismiss, and
        // the test case will fail. See https://crbug.com/1144328.
        mPage = mActivityTestRule.startOnTestServerUrl("/chrome/test/data/android/about.html");
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testTabSwitcherEventEnabled() {
        mPage.openRegularTabSwitcher();
        verify(mTracker, times(1)).notifyEvent(EventConstants.TAB_SWITCHER_BUTTON_CLICKED);
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testTabSwitcherButtonIph() {
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.TAB_SWITCHER_BUTTON_FEATURE))
                .thenReturn(true);
        when(mTracker.shouldTriggerHelpUiWithSnooze(FeatureConstants.TAB_SWITCHER_BUTTON_FEATURE))
                .thenReturn(new TriggerDetails(true, false));

        mPage = mPage.loadAboutBlank();
        mPage.tabSwitcherButtonElement.check(matches(withHighlight(true)));
        mPage.openRegularTabSwitcher();
    }
}
