// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1.tooltip;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.ViewHolderType.TYPE_CARD;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.matcher.RootMatchers;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.feed.v1.DataFilePath;
import org.chromium.chrome.browser.feed.v1.FeedDataInjectRule;
import org.chromium.chrome.browser.feed.shared.stream.Stream;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for tooltips (i.e. in-product help) shown on the {@link Stream}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=IPH_DemoMode<Trial", "force-fieldtrials=Trial/Group",
        "force-fieldtrial-params=Trial.Group:chosen_feature/IPH_FeedCardMenu"})
@Features.EnableFeatures(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)
@Features.DisableFeatures(ChromeFeatureList.INTEREST_FEED_NOTICE_CARD_AUTO_DISMISS)
public class FeedTooltipTest {
    private static final String FEED_TEST_RESPONSE_FILE_PATH =
            "/chrome/test/data/android/feed/feed_tooltip.gcl.bin";

    // Defined in feed_tooltip.gcl.textpb.
    private static final String TOOLTIP_TEXT = "oh look a pretty tooltip";

    private static final int FIRST_CARD_POSITION = 3;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public FeedDataInjectRule mFeedDataInjector = new FeedDataInjectRule(false);

    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    private class TestObserver implements Stream.ContentChangedListener {
        public final CallbackHelper firstCardShownCallback = new CallbackHelper();

        @Override
        public void onContentChanged() {
            if (mRecyclerViewAdapter.getItemViewType(FIRST_CARD_POSITION) == TYPE_CARD) {
                firstCardShownCallback.notifyCalled();
            }
        }
    }

    private final TestObserver mTestObserver = new TestObserver();

    private Stream mStream;
    private RecyclerView.Adapter mRecyclerViewAdapter;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL("about:blank");
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(tab);

        Assert.assertTrue(
                "The current tab is not a NewTabPage.", tab.getNativePage() instanceof NewTabPage);
        final NewTabPage ntp = (NewTabPage) tab.getNativePage();
        mStream = ntp.getCoordinatorForTesting().getStreamForTesting();

        Assert.assertTrue("The Stream view should be a RecyclerView.",
                mStream.getView() instanceof RecyclerView);
        mRecyclerViewAdapter = ((RecyclerView) mStream.getView()).getAdapter();

        mStream.addOnContentChangedListener(mTestObserver);
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
    @DataFilePath(FEED_TEST_RESPONSE_FILE_PATH)
    public void testShowTooltip() throws Exception {
        int callCount = mTestObserver.firstCardShownCallback.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> mStream.triggerRefresh());
        mTestObserver.firstCardShownCallback.waitForCallback(callCount);

        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(FIRST_CARD_POSITION),
                        RecyclerViewActions.actionOnItemAtPosition(FIRST_CARD_POSITION, click()));
        onView(withText(TOOLTIP_TEXT))
                .inRoot(RootMatchers.withDecorView(
                        not(is(mActivityTestRule.getActivity().getWindow().getDecorView()))))
                .check(matches(isDisplayed()));
    }
}
