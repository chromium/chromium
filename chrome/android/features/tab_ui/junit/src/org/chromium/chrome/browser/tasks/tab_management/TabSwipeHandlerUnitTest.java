// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB_GROUP;

import android.app.Activity;
import android.view.HapticFeedbackConstants;
import android.view.View;

import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter.ViewHolder;
import org.chromium.ui.test.util.TestHapticFeedbackView;

import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link TabSwipeHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSwipeHandlerUnitTest {
    private static final String COMPONENT_NAME = "TestComponent";
    private static final float SWIPE_THRESHOLD = 50f;
    private static final int TAB_ID = 101;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabActionListener mTabClosedListener;

    private SimpleRecyclerViewAdapter mSimpleAdapter;
    private RecyclerView mRecyclerView;
    private TabSwipeHandler mTabSwipeHandler;
    private UserActionTester mActionTester;

    @Before
    public void setUp() {
        mActionTester = new UserActionTester();
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();

        TabListModel tabListModel = new TabListModel();
        mSimpleAdapter = new SimpleRecyclerViewAdapter(tabListModel);
        mSimpleAdapter.registerType(
                UiType.TAB,
                parent -> new TestHapticFeedbackView(parent.getContext()),
                (m, v, k) -> {});
        mSimpleAdapter.registerType(
                UiType.TAB_GROUP,
                parent -> new TestHapticFeedbackView(parent.getContext()),
                (m, v, k) -> {});
        mSimpleAdapter.registerType(
                UiType.IPH_MESSAGE,
                parent -> {
                    TestHapticFeedbackView view = new TestHapticFeedbackView(parent.getContext());
                    View closeButton = new View(parent.getContext());
                    closeButton.setId(R.id.close_button);
                    view.addView(closeButton);
                    return view;
                },
                (m, v, k) -> {});

        mRecyclerView = new RecyclerView(activity);
        mRecyclerView.setAdapter(mSimpleAdapter);

        mTabSwipeHandler = new TabSwipeHandler(tabListModel, mTabClosedListener, COMPONENT_NAME);
        mTabSwipeHandler.setSwipeToDismissThresholdPx(SWIPE_THRESHOLD);
    }

    @After
    public void tearDown() {
        mActionTester.tearDown();
    }

    @Test
    public void getSwipeFlags_swipeableCards_returnsStartAndEnd() {
        List<ViewHolder> viewHolders =
                List.of(
                        createViewHolder(UiType.TAB, createTabModel(TAB)),
                        createViewHolder(UiType.TAB, createTabModel(TAB_GROUP)),
                        createViewHolder(UiType.IPH_MESSAGE, createMessageModel()));
        for (ViewHolder viewHolder : viewHolders) {
            assertEquals(
                    "Card type: " + viewHolder.model.get(CARD_TYPE),
                    ItemTouchHelper.START | ItemTouchHelper.END,
                    mTabSwipeHandler.getSwipeFlags(viewHolder, /* isMouseInputSource= */ false));
        }
    }

    @Test
    public void getSwipeFlags_savedTabGroupCard_returnsZero() {
        PropertyModel savedTabGroupModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GROUP_GRID)
                        .with(TabProperties.TAB_GROUP_SYNC_ID, "sync_group_1")
                        .with(CARD_TYPE, TAB_GROUP)
                        .build();
        ViewHolder savedTabGroupViewHolder = createViewHolder(UiType.TAB_GROUP, savedTabGroupModel);

        assertEquals(
                0,
                mTabSwipeHandler.getSwipeFlags(
                        savedTabGroupViewHolder, /* isMouseInputSource= */ false));
    }

    @Test
    public void onChildDrawSwipe_clampsAlphaAndPerformsHapticFeedbackOncePerThresholdCrossing() {
        PropertyModel tabModel = createTabModel(TAB);
        ViewHolder tabViewHolder = createViewHolder(UiType.TAB, tabModel);
        TestHapticFeedbackView hapticView = (TestHapticFeedbackView) tabViewHolder.itemView;

        // Sub-threshold swipe (dX = 25f) does not trigger haptics.
        mTabSwipeHandler.onChildDrawSwipe(tabViewHolder, /* dX= */ 25f);
        assertTrue(hapticView.getHapticFeedbackConstants().isEmpty());

        // Crossing threshold (dX = 50f) triggers haptics on the rising edge.
        mTabSwipeHandler.onChildDrawSwipe(tabViewHolder, /* dX= */ 50f);
        assertEquals(1, hapticView.getHapticFeedbackConstants().size());
        assertEquals(
                Integer.valueOf(HapticFeedbackConstants.LONG_PRESS),
                hapticView.getHapticFeedbackConstants().get(0));

        // Moving further past threshold (dX = 75f) clamps alpha at 0.2f and does not emit
        // duplicate haptics.
        mTabSwipeHandler.onChildDrawSwipe(tabViewHolder, /* dX= */ 75f);
        assertEquals(0.2f, tabModel.get(CARD_ALPHA), /* delta= */ 0.001f);
        assertEquals(1, hapticView.getHapticFeedbackConstants().size());

        // Sub-threshold retreat (dX = 25f) re-arms haptic feedback for subsequent threshold
        // crossings.
        mTabSwipeHandler.onChildDrawSwipe(tabViewHolder, /* dX= */ 25f);
        assertEquals(1, hapticView.getHapticFeedbackConstants().size());

        // Crossing threshold again emits haptic feedback a second time.
        mTabSwipeHandler.onChildDrawSwipe(tabViewHolder, /* dX= */ 50f);
        assertEquals(2, hapticView.getHapticFeedbackConstants().size());
    }

    @Test
    public void resetSwipeState_reArmsHapticFeedbackPastThreshold() {
        ViewHolder tabViewHolder = createViewHolder(UiType.TAB, createTabModel(TAB));
        TestHapticFeedbackView hapticView = (TestHapticFeedbackView) tabViewHolder.itemView;

        mTabSwipeHandler.onChildDrawSwipe(tabViewHolder, /* dX= */ 50f);
        assertEquals(1, hapticView.getHapticFeedbackConstants().size());

        // After a reset, the next frame past the threshold emits haptic feedback again.
        mTabSwipeHandler.resetSwipeState();
        mTabSwipeHandler.onChildDrawSwipe(tabViewHolder, /* dX= */ 50f);
        assertEquals(2, hapticView.getHapticFeedbackConstants().size());
    }

    @Test
    public void onSwiped_tabCard_updatesRecentlySwipedSupplierAndRunsListenerAndRecordsAction() {
        ViewHolder tabViewHolder = createViewHolder(UiType.TAB, createTabModel(TAB));

        mTabSwipeHandler.onSwiped(tabViewHolder);

        assertEquals(
                Integer.valueOf(TAB_ID), mTabSwipeHandler.getRecentlySwipedTabIdSupplier().get());
        verify(mTabClosedListener)
                .run(eq(tabViewHolder.itemView), eq(TAB_ID), /* triggeringMotion= */ eq(null));
        assertEquals(
                1, mActionTester.getActionCount("MobileStackViewSwipeCloseTab." + COMPONENT_NAME));
    }

    @Test
    public void onSwiped_messageCard_clicksCloseButtonWithoutRecordingTabSwipeAction() {
        ViewHolder messageViewHolder = createViewHolder(UiType.IPH_MESSAGE, createMessageModel());
        AtomicBoolean closeClicked = new AtomicBoolean(false);
        View closeButton = messageViewHolder.itemView.findViewById(R.id.close_button);
        closeButton.setOnClickListener(v -> closeClicked.set(true));

        mTabSwipeHandler.onSwiped(messageViewHolder);

        assertTrue(closeClicked.get());
        assertEquals(
                0, mActionTester.getActionCount("MobileStackViewSwipeCloseTab." + COMPONENT_NAME));
    }

    private ViewHolder createViewHolder(@UiType int uiType, PropertyModel model) {
        ViewHolder viewHolder = mSimpleAdapter.createViewHolder(mRecyclerView, uiType);
        viewHolder.model = model;
        return viewHolder;
    }

    private static PropertyModel createTabModel(@ModelType int cardType) {
        return new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                .with(TabProperties.TAB_ID, TAB_ID)
                .with(CARD_ALPHA, 1f)
                .with(CARD_TYPE, cardType)
                .build();
    }

    private static PropertyModel createMessageModel() {
        return new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                .with(MESSAGE_TYPE, MessageType.FOR_TESTING)
                .with(CARD_TYPE, MESSAGE)
                .build();
    }
}
