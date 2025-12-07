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

import static org.chromium.chrome.browser.tasks.tab_management.UiTypeHelper.isMessageCard;
import static org.chromium.chrome.browser.tasks.tab_management.UiTypeHelper.isValidUiType;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ServiceDismissActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.Message;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceMessageData;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.atomic.AtomicBoolean;

/** Integration tests for MessageCardProvider component. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class MessageCardProviderTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private TabListRecyclerView mRecyclerView;

    private TabListModel mModelList;
    private SimpleRecyclerViewAdapter mAdapter;

    private MessageCardProviderCoordinator<@MessageType Integer, @UiType Integer> mCoordinator;
    private MessageService<@MessageType Integer, @UiType Integer> mTestingService;
    private MessageService<@MessageType Integer, @UiType Integer> mPriceService;

    private final ServiceDismissActionProvider<@MessageType Integer> mServiceDismissActionProvider =
            (messageType) -> {};

    @Mock private PriceMessageData mPriceMessageData;

    @Mock private Profile mProfile;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        // TODO(meiliang): Replace with TabSwitcher instead when ready to integrate with
        // TabSwitcher.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModelList = new TabListModel();
                    ViewGroup view = new FrameLayout(sActivity);
                    mAdapter = new SimpleRecyclerViewAdapter(mModelList);

                    sActivity.setContentView(view);

                    mRecyclerView =
                            (TabListRecyclerView)
                                    sActivity
                                            .getLayoutInflater()
                                            .inflate(
                                                    R.layout.tab_list_recycler_view_layout,
                                                    view,
                                                    false);
                    mRecyclerView.setVisibility(View.VISIBLE);

                    mAdapter.registerType(
                            UiType.IPH_MESSAGE,
                            new LayoutViewBuilder<>(R.layout.tab_grid_message_card_item),
                            MessageCardViewBinder::bind);

                    mAdapter.registerType(
                            UiType.PRICE_MESSAGE,
                            new LayoutViewBuilder<>(R.layout.large_message_card_item),
                            LargeMessageCardViewBinder::bind);

                    GridLayoutManager layoutManager =
                            new GridLayoutManager(mRecyclerView.getContext(), 2);
                    layoutManager.setSpanSizeLookup(
                            new GridLayoutManager.SpanSizeLookup() {
                                @SuppressLint("WrongConstant")
                                @Override
                                public int getSpanSize(int i) {
                                    int itemType = mAdapter.getItemViewType(i);
                                    assertTrue(isValidUiType(itemType));

                                    if (isMessageCard(itemType)) {
                                        return 2;
                                    }
                                    return 1;
                                }
                            });
                    mRecyclerView.setLayoutManager(layoutManager);
                    mRecyclerView.setAdapter(mAdapter);

                    view.addView(mRecyclerView);

                    mTestingService =
                            new MessageService<>(
                                    MessageType.FOR_TESTING,
                                    UiType.IPH_MESSAGE,
                                    R.layout.tab_grid_message_card_item,
                                    MessageCardViewBinder::bind);
                    mPriceService =
                            new MessageService<>(
                                    MessageType.PRICE_MESSAGE,
                                    UiType.IPH_MESSAGE,
                                    R.layout.tab_grid_message_card_item,
                                    MessageCardViewBinder::bind);

                    mCoordinator =
                            new MessageCardProviderCoordinator<>(
                                    sActivity, mServiceDismissActionProvider);
                    mCoordinator.subscribeMessageService(mTestingService);
                    mCoordinator.subscribeMessageService(mPriceService);
                });
    }

    @Test
    @SmallTest
    public void testPriceMessage() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sendAvailabilityNotification();
                    addMessageCards();
                });

        onViewWaiting(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testReviewPriceMessage() {
        AtomicBoolean reviewed = new AtomicBoolean();
        when(mPriceMessageData.getAcceptActionProvider()).thenReturn(() -> reviewed.set(true));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sendAvailabilityNotification();
                    addMessageCards();
                });

        onViewWaiting(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));

        assertFalse(reviewed.get());
        onView(withId(R.id.action_button)).perform(click());
        assertTrue(reviewed.get());
    }

    @Test
    @SmallTest
    public void testDismissPriceMessage() {
        AtomicBoolean dismissed = new AtomicBoolean();
        when(mPriceMessageData.getDismissActionProvider()).thenReturn(() -> dismissed.set(true));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sendAvailabilityNotification();
                    addMessageCards();
                });

        onViewWaiting(withId(R.id.large_message_card_item)).check(matches(isDisplayed()));

        assertFalse(dismissed.get());
        onView(withId(R.id.close_button)).perform(click());
        assertTrue(dismissed.get());
    }

    private void addMessageCards() {
        for (MessageService<@MessageType Integer, @UiType Integer> service :
                mCoordinator.getMessageServices()) {
            Message<@MessageType Integer> message =
                    mCoordinator.getNextMessageItemForType(service.getMessageType());
            if (message == null) continue;
            if (message.type == MessageType.PRICE_MESSAGE) {
                mModelList.add(new MVCListAdapter.ListItem(UiType.PRICE_MESSAGE, message.model));
            } else {
                mModelList.add(new MVCListAdapter.ListItem(UiType.IPH_MESSAGE, message.model));
            }
        }
    }

    private void sendAvailabilityNotification() {
        mPriceService.sendAvailabilityNotification(
                (a, b) ->
                        PriceMessageCardViewModel.create(
                                sActivity,
                                c -> {},
                                mPriceMessageData,
                                new PriceDropNotificationManagerImpl(mProfile)));
    }
}
