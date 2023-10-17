// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestion;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/** Integration tests for MessageCardProvider component. */
@DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
public class MessageCardProviderTest extends BlankUiTestActivityTestCase {
    private static final int SUGGESTED_TAB_COUNT = 2;

    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    private TabListRecyclerView mRecyclerView;
    private TabListRecyclerView.VisibilityListener mRecyclerViewVisibilityListener =
            new TabListRecyclerView.VisibilityListener() {
                @Override
                public void startedShowing(boolean isAnimating) {
                    List<MessageCardProviderMediator.Message> messageList =
                            mCoordinator.getMessageItems();
                    for (int i = 0; i < messageList.size(); i++) {
                        MessageCardProviderMediator.Message message = messageList.get(i);
                        if (message.type == MessageService.MessageType.PRICE_MESSAGE) {
                            mModelList.add(
                                    new MVCListAdapter.ListItem(
                                            TabProperties.UiType.LARGE_MESSAGE, message.model));
                        } else {
                            mModelList.add(
                                    new MVCListAdapter.ListItem(
                                            TabProperties.UiType.MESSAGE, message.model));
                        }
                    }
                }

                @Override
                public void finishedShowing() {
                    mFinishedShowing.set(true);
                }

                @Override
                public void startedHiding(boolean isAnimating) {
                    mFinishedShowing.set(false);
                    mModelList.clear();
                }

                @Override
                public void finishedHiding() {}
            };

    private TabListModel mModelList;
    private SimpleRecyclerViewAdapter mAdapter;

    private AtomicBoolean mFinishedShowing = new AtomicBoolean(false);

    private MessageCardProviderCoordinator mCoordinator;
    private MessageService mTestingService;
    private MessageService mSuggestionService;
    private MessageService mPriceService;

    private MessageCardView.DismissActionProvider mUiDismissActionProvider = (messageType) -> {};

    @Mock private TabSuggestionMessageService.TabSuggestionMessageData mTabSuggestionMessageData;

    @Mock private PriceMessageService.PriceMessageData mPriceMessageData;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);
        // TODO(meiliang): Replace with TabSwitcher instead when ready to integrate with
        // TabSwitcher.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModelList = new TabListModel();
                    ViewGroup view = new FrameLayout(getActivity());
                    mAdapter = new SimpleRecyclerViewAdapter(mModelList);

                    getActivity().setContentView(view);

                    mRecyclerView =
                            (TabListRecyclerView)
                                    getActivity()
                                            .getLayoutInflater()
                                            .inflate(
                                                    R.layout.tab_list_recycler_view_layout,
                                                    view,
                                                    false);
                    mRecyclerView.setVisibilityListener(mRecyclerViewVisibilityListener);
                    mRecyclerView.setVisibility(View.INVISIBLE);

                    mAdapter.registerType(
                            TabProperties.UiType.MESSAGE,
                            new LayoutViewBuilder(R.layout.tab_grid_message_card_item),
                            MessageCardViewBinder::bind);

                    mAdapter.registerType(
                            TabProperties.UiType.LARGE_MESSAGE,
                            new LayoutViewBuilder(R.layout.large_message_card_item),
                            LargeMessageCardViewBinder::bind);

                    GridLayoutManager layoutManager =
                            new GridLayoutManager(mRecyclerView.getContext(), 2);
                    layoutManager.setSpanSizeLookup(
                            new GridLayoutManager.SpanSizeLookup() {
                                @Override
                                public int getSpanSize(int i) {
                                    int itemType = mAdapter.getItemViewType(i);

                                    if (itemType == TabProperties.UiType.MESSAGE
                                            || itemType == TabProperties.UiType.LARGE_MESSAGE) {
                                        return 2;
                                    }
                                    return 1;
                                }
                            });
                    mRecyclerView.setLayoutManager(layoutManager);
                    mRecyclerView.setAdapter(mAdapter);

                    view.addView(mRecyclerView);

                    mTestingService = new MessageService(MessageService.MessageType.FOR_TESTING);
                    mSuggestionService =
                            new MessageService(MessageService.MessageType.TAB_SUGGESTION);
                    mPriceService = new MessageService(MessageService.MessageType.PRICE_MESSAGE);

                    mCoordinator =
                            new MessageCardProviderCoordinator(
                                    getActivity(), () -> false, mUiDismissActionProvider);
                    mCoordinator.subscribeMessageService(mTestingService);
                    mCoordinator.subscribeMessageService(mSuggestionService);
                    mCoordinator.subscribeMessageService(mPriceService);
                });

        when(mTabSuggestionMessageData.getActionType())
                .thenReturn(TabSuggestion.TabSuggestionAction.CLOSE);
    }

    @Test
    @SmallTest
    public void testShowingTabSuggestionMessage() {
        when(mTabSuggestionMessageData.getSize()).thenReturn(SUGGESTED_TAB_COUNT);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSuggestionService.sendAvailabilityNotification(mTabSuggestionMessageData);
                    mRecyclerView.startShowing(false);
                });

        CriteriaHelper.pollUiThread(
                () -> mRecyclerView.getVisibility() == View.VISIBLE && mFinishedShowing.get());

        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testReviewTabSuggestionMessage() {
        AtomicBoolean reviewed = new AtomicBoolean();
        when(mTabSuggestionMessageData.getSize()).thenReturn(SUGGESTED_TAB_COUNT);
        when(mTabSuggestionMessageData.getReviewActionProvider())
                .thenReturn(() -> reviewed.set(true));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSuggestionService.sendAvailabilityNotification(mTabSuggestionMessageData);
                    mRecyclerView.startShowing(false);
                });

        CriteriaHelper.pollUiThread(
                () -> mRecyclerView.getVisibility() == View.VISIBLE && mFinishedShowing.get());

        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        assertFalse(reviewed.get());
        onView(withId(R.id.action_button)).perform(click());
        assertTrue(reviewed.get());
    }

    @Test
    @SmallTest
    public void testDismissTabSuggestionMessage() {
        AtomicBoolean dismissed = new AtomicBoolean();
        when(mTabSuggestionMessageData.getSize()).thenReturn(SUGGESTED_TAB_COUNT);
        when(mTabSuggestionMessageData.getDismissActionProvider())
                .thenReturn((type) -> dismissed.set(true));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSuggestionService.sendAvailabilityNotification(mTabSuggestionMessageData);
                    mRecyclerView.startShowing(false);
                });

        CriteriaHelper.pollUiThread(
                () -> mRecyclerView.getVisibility() == View.VISIBLE && mFinishedShowing.get());

        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        assertFalse(dismissed.get());
        onView(withId(R.id.close_button)).perform(click());
        assertTrue(dismissed.get());
    }

    @Test
    @SmallTest
    public void testPriceMessage() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPriceService.sendAvailabilityNotification(mPriceMessageData);
                    mRecyclerView.startShowing(false);
                });

        CriteriaHelper.pollUiThread(
                () -> mRecyclerView.getVisibility() == View.VISIBLE && mFinishedShowing.get());

        onView(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testReviewPriceMessage() {
        AtomicBoolean reviewed = new AtomicBoolean();
        when(mPriceMessageData.getReviewActionProvider()).thenReturn(() -> reviewed.set(true));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPriceService.sendAvailabilityNotification(mPriceMessageData);
                    mRecyclerView.startShowing(false);
                });

        CriteriaHelper.pollUiThread(
                () -> mRecyclerView.getVisibility() == View.VISIBLE && mFinishedShowing.get());

        onView(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));

        assertFalse(reviewed.get());
        onView(withId(R.id.action_button)).perform(click());
        assertTrue(reviewed.get());
    }

    @Test
    @SmallTest
    public void testDismissPriceMessage() {
        AtomicBoolean dismissed = new AtomicBoolean();
        when(mPriceMessageData.getDismissActionProvider())
                .thenReturn((type) -> dismissed.set(true));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPriceService.sendAvailabilityNotification(mPriceMessageData);
                    mRecyclerView.startShowing(false);
                });

        CriteriaHelper.pollUiThread(
                () -> mRecyclerView.getVisibility() == View.VISIBLE && mFinishedShowing.get());

        onView(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));

        assertFalse(dismissed.get());
        onView(withId(R.id.close_button)).perform(click());
        assertTrue(dismissed.get());
    }
}
