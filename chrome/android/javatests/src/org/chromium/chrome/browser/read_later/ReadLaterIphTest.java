// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.read_later;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.toolbar.top.ButtonHighlightMatcher.withHighlight;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.test.espresso.ViewInteraction;
import androidx.test.espresso.matcher.RootMatchers;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.contextmenu.RevampedContextMenuUtils;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.test.util.UiRestriction;

/** Integration tests for showing IPH bubbles for read later. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.READ_LATER)
@Batch(Batch.PER_CLASS)
public class ReadLaterIphTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public EmbeddedTestServerRule mTestServer = new EmbeddedTestServerRule();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock
    private Tracker mTracker;

    private static final String CONTEXT_MENU_TEST_URL =
            "/chrome/test/data/android/contextmenu/context_menu_test.html";
    private static final String CONTEXT_MENU_LINK_DOM_ID = "testLink";

    @Before
    public void setUp() {
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
    public void tearDown() throws Exception {
        TrackerFactory.setTrackerForTests(null);
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testShowIPHOnContextMenuLinkCopied() throws Throwable {
        when(mTracker.shouldTriggerHelpUI(
                     FeatureConstants.READ_LATER_APP_MENU_BOOKMARK_THIS_PAGE_FEATURE))
                .thenReturn(true);
        mActivityTestRule.loadUrlInNewTab(mTestServer.getServer().getURL(CONTEXT_MENU_TEST_URL));

        ChromeActivity activity = mActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        RevampedContextMenuUtils.selectContextMenuItem(InstrumentationRegistry.getInstrumentation(),
                activity, tab, CONTEXT_MENU_LINK_DOM_ID, R.id.contextmenu_copy_link_address);

        onView(withId(R.id.menu_button_wrapper)).check(matches(withHighlight(true)));
        waitForHelpBubble(withText(R.string.reading_list_save_pages_for_later));
    }

    private ViewInteraction waitForHelpBubble(Matcher<View> matcher) {
        View mainDecorView = mActivityTestRule.getActivity().getWindow().getDecorView();
        return onView(isRoot())
                .inRoot(RootMatchers.withDecorView(not(is(mainDecorView))))
                .check(waitForView(matcher));
    }
}
