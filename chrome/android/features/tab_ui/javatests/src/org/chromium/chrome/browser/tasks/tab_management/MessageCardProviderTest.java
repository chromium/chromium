// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import android.support.test.filters.SmallTest;
import android.support.v7.widget.GridLayoutManager;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;

import org.chromium.chrome.browser.tab.TabFeatureUtilities;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Integration tests for TabGridMessageCardProvider component.
 */
public class MessageCardProviderTest extends DummyUiActivityTestCase {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private TabListRecyclerView mRecyclerView;
    private TabListRecyclerView.VisibilityListener mRecyclerViewVisibilityListener =
            new TabListRecyclerView.VisibilityListener() {
                @Override
                public void startedShowing(boolean isAnimating) {
                    List<MessageCardProviderMediator.Message> messageList =
                            mCoordinator.getMessageItems();
                    for (int i = 0; i < messageList.size(); i++) {
                        MessageCardProviderMediator.Message message = messageList.get(i);
                        mModelList.add(new MVCListAdapter.ListItem(
                                TabProperties.UiType.MESSAGE, message.model));
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

    private TabListModel mModelList = new TabListModel();
    private SimpleRecyclerViewAdapter mAdapter;

    private AtomicBoolean mFinishedShowing = new AtomicBoolean(false);

    private MessageService mTestingService =
            new MessageService(MessageService.MessageType.FOR_TESTING);
    private MessageService mSuggestionService =
            new MessageService(MessageService.MessageType.TAB_SUGGESTION);
    private MessageCardProviderCoordinator mCoordinator;

    private MessageCardView.DismissActionProvider mUiDismissActionProvider = () -> {};

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        TabFeatureUtilities.setIsTabToGtsAnimationEnabledForTesting(false);

        // TODO(meiliang): Replace with TabSwitcher instead when ready to integrate with
        // TabSwitcher.
        ViewGroup view = new FrameLayout(getActivity());
        mAdapter = new SimpleRecyclerViewAdapter(mModelList);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getActivity().setContentView(view);

            mRecyclerView = (TabListRecyclerView) getActivity().getLayoutInflater().inflate(
                    R.layout.tab_list_recycler_view_layout, view, false);
            mRecyclerView.setVisibilityListener(mRecyclerViewVisibilityListener);
            mRecyclerView.setVisibility(View.INVISIBLE);

            mAdapter.registerType(TabProperties.UiType.MESSAGE, () -> {
                return (ViewGroup) getActivity().getLayoutInflater().inflate(
                        R.layout.tab_grid_message_card_item, mRecyclerView, false);
            }, MessageCardViewBinder::bind);

            GridLayoutManager layoutManager = new GridLayoutManager(mRecyclerView.getContext(), 2);
            layoutManager.setSpanSizeLookup(new GridLayoutManager.SpanSizeLookup() {
                @Override
                public int getSpanSize(int i) {
                    int itemType = mAdapter.getItemViewType(i);

                    if (itemType == TabProperties.UiType.MESSAGE) return 2;
                    return 1;
                }
            });
            mRecyclerView.setLayoutManager(layoutManager);
            mRecyclerView.setAdapter(mAdapter);

            view.addView(mRecyclerView);
        });

        mCoordinator = new MessageCardProviderCoordinator(mUiDismissActionProvider);
        mCoordinator.subscribeMessageService(mTestingService);
        mCoordinator.subscribeMessageService(mSuggestionService);
    }

    @Test
    @SmallTest
    public void testShowingMessage() {
        // TODO(crbug.com/1004570): Use MessageData instead of null when ready to integrate with
        // MessageService component.
        mSuggestionService.sendAvailabilityNotification(null);

        TestThreadUtils.runOnUiThreadBlocking(() -> { mRecyclerView.startShowing(false); });

        CriteriaHelper.pollInstrumentationThread(
                () -> mRecyclerView.getVisibility() == View.VISIBLE && mFinishedShowing.get());

        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
    }
}
