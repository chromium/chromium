// Copyright 2020 The Chromium Authors
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
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.toolbar.top.ButtonHighlightMatcher.withHighlight;

import android.view.View;

import androidx.test.espresso.ViewInteraction;
import androidx.test.espresso.matcher.RootMatchers;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matcher;
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
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.offlinepages.RequestCoordinatorBridge;
import org.chromium.chrome.browser.offlinepages.RequestCoordinatorBridgeJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerDetails;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.ViewUtils;

/** Integration tests for showing IPH bubbles for read later. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ReadLaterContextMenuTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public EmbeddedTestServerRule mTestServer = new EmbeddedTestServerRule();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mocker = new JniMocker();
    @Mock private Tracker mTracker;
    @Mock RequestCoordinatorBridge.Natives mRequestCoordinatorBridgeJniMock;

    private static final String CONTEXT_MENU_TEST_URL =
            "/chrome/test/data/android/contextmenu/context_menu_test.html";
    private static final String CONTEXT_MENU_LINK_URL =
            "/chrome/test/data/android/contextmenu/test_link.html";
    private static final String CONTEXT_MENU_LINK_DOM_ID = "testLink";

    @Before
    public void setUp() {
        // Pretend the feature engagement feature is already initialized. Otherwise
        // UserEducationHelper#requestShowIPH() calls get dropped during test.
        doAnswer(
                        invocation -> {
                            invocation.<Callback<Boolean>>getArgument(0).onResult(true);
                            return null;
                        })
                .when(mTracker)
                .addOnInitializedCallback(any());
        TrackerFactory.setTrackerForTests(mTracker);
        when(mTracker.shouldTriggerHelpUIWithSnooze(any()))
                .thenReturn(new TriggerDetails(false, false));
        mActivityTestRule.startMainActivityOnBlankPage();
        mocker.mock(RequestCoordinatorBridgeJni.TEST_HOOKS, mRequestCoordinatorBridgeJniMock);
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testShowIPHOnContextMenuLinkCopied() throws Throwable {
        when(mTracker.shouldTriggerHelpUI(
                        FeatureConstants.READ_LATER_APP_MENU_BOOKMARK_THIS_PAGE_FEATURE))
                .thenReturn(true);
        when(mTracker.shouldTriggerHelpUIWithSnooze(
                        FeatureConstants.READ_LATER_APP_MENU_BOOKMARK_THIS_PAGE_FEATURE))
                .thenReturn(new TriggerDetails(true, false));

        mActivityTestRule.loadUrlInNewTab(mTestServer.getServer().getURL(CONTEXT_MENU_TEST_URL));

        ChromeActivity activity = mActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        ContextMenuUtils.selectContextMenuItem(
                InstrumentationRegistry.getInstrumentation(),
                activity,
                tab,
                CONTEXT_MENU_LINK_DOM_ID,
                R.id.contextmenu_copy_link_address);

        onView(withId(R.id.menu_button_wrapper)).check(matches(withHighlight(true)));
        waitForHelpBubble(withText(R.string.reading_list_save_pages_for_later));
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testContextMenuAddToOfflinePage() throws Throwable {
        String url = mTestServer.getServer().getURL(CONTEXT_MENU_TEST_URL);
        mActivityTestRule.loadUrlInNewTab(url);
        ChromeActivity activity = mActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        ContextMenuUtils.selectContextMenuItem(
                InstrumentationRegistry.getInstrumentation(),
                activity,
                tab,
                CONTEXT_MENU_LINK_DOM_ID,
                R.id.contextmenu_read_later);
        String linkUrl = mTestServer.getServer().getURL(CONTEXT_MENU_LINK_URL);
        verify(mRequestCoordinatorBridgeJniMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .savePageLater(any(), any(), eq(linkUrl), any(), any(), any(), anyBoolean());
    }

    private ViewInteraction waitForHelpBubble(Matcher<View> matcher) {
        View mainDecorView = mActivityTestRule.getActivity().getWindow().getDecorView();
        return onView(isRoot())
                .inRoot(RootMatchers.withDecorView(not(is(mainDecorView))))
                .check(ViewUtils.isEventuallyVisible(matcher));
    }
}
