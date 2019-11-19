// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.junit.Assume.assumeFalse;
import static org.junit.Assume.assumeTrue;

import android.support.test.espresso.intent.rule.IntentsTestRule;
import android.support.test.filters.SmallTest;
import android.support.v7.widget.RecyclerView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.history.HistoryTestUtils.TestObserver;
import org.chromium.chrome.browser.ui.widget.MoreProgressButton;
import org.chromium.chrome.browser.ui.widget.MoreProgressButton.State;
import org.chromium.chrome.browser.widget.DateDividedAdapter.FooterItem;
import org.chromium.chrome.browser.widget.DateDividedAdapter.TimedItem;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.List;

/**
 * Tests the scrolling behavior on {@link HistoryActivity}.
 * The main difference for this test file with {@link HistoryActivityTest}is to test scrolling
 * behavior under different settings.
 */
// clang-format off
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
public class HistoryActivityScrollingTest {
    // clang-format on
    @Rule
    public IntentsTestRule<HistoryActivity> mActivityTestRule =
            new IntentsTestRule<>(HistoryActivity.class, false, false);

    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams = new TestParamsProvider().getParameters();

    private static class TestParams extends ParameterSet {
        public final int mPaging;
        public final int mTotalItems;
        public final boolean mIsScrollToLoadDisabled;

        public TestParams(int paging, int totalItems, boolean isScrollToLoadDisabled) {
            super();
            mPaging = paging;
            mTotalItems = totalItems;
            mIsScrollToLoadDisabled = isScrollToLoadDisabled;

            value(paging, totalItems, isScrollToLoadDisabled);
        }

        @Override
        public String toString() {
            return "paging: " + mPaging + ", totalItems: " + mTotalItems
                    + ", isScrollToLoadDisabled: " + mIsScrollToLoadDisabled;
        }
    }

    private static class TestParamsProvider implements ParameterProvider {
        private static List<ParameterSet> sScrollToLoad =
                Arrays.asList(new TestParams(5, 30, false).name("Enabled"),
                        new TestParams(5, 30, true).name("Disabled"),
                        new TestParams(5, 12, true).name("Disabled_Less"));

        @Override
        public List<ParameterSet> getParameters() {
            return sScrollToLoad;
        }
    }

    private StubbedHistoryProvider mHistoryProvider;
    private HistoryAdapter mAdapter;
    private HistoryManager mHistoryManager;
    private RecyclerView mRecyclerView;
    private TestObserver mTestObserver;
    // private PrefChangeRegistrar mPrefChangeRegistrar;

    private List<HistoryItem> mItems;

    private int mPaging;
    private int mTotalItems;
    private boolean mIsScrollToLoadDisabled;
    private int mOrigItemsCount;

    public HistoryActivityScrollingTest(
            int paging, int totalItems, boolean isScrollToLoadDisabled) {
        mPaging = paging;
        mTotalItems = totalItems;
        mIsScrollToLoadDisabled = isScrollToLoadDisabled;
        mItems = new ArrayList<>(mTotalItems);
    }

    @Before
    public void setUp() throws Exception {
        // Account not signed in by default. The clear browsing data header, one date view, and two
        // history item views should be shown, but the info header should not. We enforce a defaultx
        // state because the number of headers shown depends on the signed-in state.
        SigninTestUtil.setUpAuthForTest();

        mHistoryProvider = new StubbedHistoryProvider();
        mHistoryProvider.setPaging(mPaging);

        Date today = new Date();
        long timestamp = today.getTime();

        for (int i = 0; i < mTotalItems; i++) {
            HistoryItem item = StubbedHistoryProvider.createHistoryItem(0, --timestamp);
            mItems.add(item);
            mHistoryProvider.addItem(item);
        }

        HistoryManager.setProviderForTests(mHistoryProvider);
        HistoryManager.setScrollToLoadDisabledForTesting(mIsScrollToLoadDisabled);

        launchHistoryActivity();
        HistoryTestUtils.setupHistoryTestHeaders(mAdapter, mTestObserver);

        mOrigItemsCount = mAdapter.getItemCount();
        Assert.assertTrue("At least one item should be loaded to adapter", mOrigItemsCount > 0);
    }

    @After
    public void tearDown() {
        SigninTestUtil.tearDownAuthForTest();
    }

    private void launchHistoryActivity() {
        HistoryActivity activity = mActivityTestRule.launchActivity(null);
        mHistoryManager = activity.getHistoryManagerForTests();
        mAdapter = mHistoryManager.getAdapterForTests();
        mTestObserver = new TestObserver();
        mHistoryManager.getSelectionDelegateForTests().addObserver(mTestObserver);
        mAdapter.registerAdapterDataObserver(mTestObserver);
        mRecyclerView = ((RecyclerView) activity.findViewById(R.id.recycler_view));
    }

    @Test
    @SmallTest
    public void testScrollToLoadEnabled() {
        assumeFalse(mIsScrollToLoadDisabled);

        RecyclerViewTestUtils.scrollToBottom(mRecyclerView);

        Assert.assertTrue("Should load more items into view after scroll",
                mAdapter.getItemCount() > mOrigItemsCount);
        Assert.assertTrue(String.valueOf(mPaging) + " more Items should be loaded",
                mAdapter.getItemCount() == mOrigItemsCount + mPaging);
    }

    @Test
    @SmallTest
    public void testScrollToLoadDisabled() throws Exception {
        assumeTrue(mIsScrollToLoadDisabled);

        RecyclerViewTestUtils.scrollToBottom(mRecyclerView);

        Assert.assertTrue("Should not load more items into view after scroll",
                mAdapter.getItemCount() == mOrigItemsCount);
        Assert.assertTrue(
                "Footer should be added to the end of the view", mAdapter.hasListFooter());
        Assert.assertEquals(
                "Footer group should contain one item", 1, mAdapter.getLastGroupForTests().size());

        // Verify the button is correctly displayed
        TimedItem item = mAdapter.getLastGroupForTests().getItemAt(0);
        MoreProgressButton button = (MoreProgressButton) ((FooterItem) item).getView();
        Assert.assertSame("FooterItem view should be MoreProgressButton",
                mAdapter.getMoreProgressButtonForTest(), button);
        Assert.assertEquals(
                "State for the MPB should be button", button.getStateForTest(), State.BUTTON);

        // Test click, should load more items
        int callCount = mTestObserver.onChangedCallback.getCallCount();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> button.findViewById(R.id.action_button).performClick());
        mTestObserver.onChangedCallback.waitForCallback(callCount);

        Assert.assertTrue("Should load more items into view after click more button",
                mAdapter.getItemCount() > mOrigItemsCount);
    }
}
