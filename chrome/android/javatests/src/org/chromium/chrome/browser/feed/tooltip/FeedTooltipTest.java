// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.tooltip;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static com.google.android.libraries.feed.basicstream.internal.viewholders.ViewHolderType.TYPE_CARD;

import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import android.support.test.espresso.contrib.RecyclerViewActions;
import android.support.test.espresso.matcher.RootMatchers;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;

import com.google.android.libraries.feed.api.client.stream.Stream;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.feed.FeedNewTabPage;
import org.chromium.chrome.browser.feed.FeedProcessScopeFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for tooltips (i.e. in-product help) shown on the {@link Stream}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=IPH_DemoMode<Trial", "force-fieldtrials=Trial/Group",
        "force-fieldtrial-params=Trial.Group:chosen_feature/IPH_FeedCardMenu"})
@Features.EnableFeatures(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)
public class FeedTooltipTest {
    private static final String FEED_TEST_RESPONSE_FILE_PATH =
            "/chrome/test/data/android/feed/feed_tooltip.gcl.bin";

    // Defined in feed_tooltip.gcl.textpb.
    private static final String TOOLTIP_TEXT = "oh look a pretty tooltip";

    private static final int FIRST_CARD_POSITION = 3;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule(FEED_TEST_RESPONSE_FILE_PATH);

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

        Assert.assertTrue("The current tab is not a FeedNewTabPage.",
                tab.getNativePage() instanceof FeedNewTabPage);
        final FeedNewTabPage ntp = (FeedNewTabPage) tab.getNativePage();
        mStream = ntp.getStreamForTesting();

        Assert.assertTrue("The Stream view should be a RecyclerView.",
                mStream.getView() instanceof RecyclerView);
        mRecyclerViewAdapter = ((RecyclerView) mStream.getView()).getAdapter();

        mStream.addOnContentChangedListener(mTestObserver);
    }

    @After
    public void tearDown() {
        FeedProcessScopeFactory.setTestNetworkClient(null);
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage"})
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
