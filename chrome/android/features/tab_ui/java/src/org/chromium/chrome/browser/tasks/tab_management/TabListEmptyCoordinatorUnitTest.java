// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;
import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType.ARCHIVED_TABS_MESSAGE;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowDrawable;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link TabListEmptyCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabListEmptyCoordinatorUnitTest {
    /** Adapter that shows a single pre-built view. */
    private static class SingleViewAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
        private final View mView;

        SingleViewAdapter(View view) {
            mView = view;
        }

        @Override
        public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            return new RecyclerView.ViewHolder(mView) {};
        }

        @Override
        public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {}

        @Override
        public int getItemCount() {
            return 1;
        }
    }

    private static final int MESSAGE_CARD_HEIGHT = 50;
    private static final int MESSAGE_CARD_TOP = 5;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Callback<Runnable> mRunOnItemAnimatorFinished;

    private TabListRecyclerView mRecyclerView;
    private FrameLayout mRootView;
    private Activity mContext;
    private TabListModel mModel;
    private TabListEmptyCoordinator mCoordinator;

    @Before
    public void setUp() {
        mModel = new TabListModel();

        mActivityScenarioRule.getScenario().onActivity(activity -> mContext = activity);
        mRootView = new FrameLayout(mContext);
        mRootView.layout(0, 0, 100, 100);
        mRecyclerView = new TabListRecyclerView(mContext, /* attributeSet= */ null);

        // Immediately execute runnables passed to mRunOnItemAnimatorFinished.
        doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mRunOnItemAnimatorFinished)
                .onResult(any());

        mCoordinator =
                new TabListEmptyCoordinator(
                        mRecyclerView, mRootView, mModel, mRunOnItemAnimatorFinished);
    }

    @Test
    public void testInitializeEmptyStateView() {
        mCoordinator.initializeEmptyStateView(
                R.drawable.phone_tab_switcher_empty_state_illustration_static,
                R.string.tabswitcher_no_tabs_empty_state,
                R.string.tabswitcher_no_tabs_open_to_visit_different_pages);

        mCoordinator.attachEmptyView();
        View emptyView = mRootView.getChildAt(0);
        assertNotNull(emptyView);
        assertEquals(View.GONE, emptyView.getVisibility());

        ImageView imageView = emptyView.findViewById(R.id.empty_state_icon);
        ShadowDrawable shadowDrawable = Shadows.shadowOf(imageView.getDrawable());
        assertEquals(
                R.drawable.phone_tab_switcher_empty_state_illustration_static,
                shadowDrawable.getCreatedFromResId());

        TextView heading = emptyView.findViewById(R.id.empty_state_text_title);
        assertEquals(
                mContext.getString(R.string.tabswitcher_no_tabs_empty_state), heading.getText());

        TextView subheading = emptyView.findViewById(R.id.empty_state_text_description);
        assertEquals(
                mContext.getString(R.string.tabswitcher_no_tabs_open_to_visit_different_pages),
                subheading.getText());
    }

    @Test
    public void testEmptyStateVisibility_EmptyModel() {
        mCoordinator.initializeEmptyStateView(
                R.drawable.phone_tab_switcher_empty_state_illustration_static,
                R.string.tabswitcher_no_tabs_empty_state,
                R.string.tabswitcher_no_tabs_open_to_visit_different_pages);
        mCoordinator.attachEmptyView();
        View emptyView = mRootView.getChildAt(0);

        assertEquals(View.GONE, emptyView.getVisibility());

        mCoordinator.setIsTabSwitcherShowing(true);
        assertEquals(View.VISIBLE, emptyView.getVisibility());

        mCoordinator.setIsTabSwitcherShowing(false);
        assertEquals(View.GONE, emptyView.getVisibility());
    }

    @Test
    public void testEmptyStateVisibility_WithTabs() {
        mCoordinator.initializeEmptyStateView(
                R.drawable.phone_tab_switcher_empty_state_illustration_static,
                R.string.tabswitcher_no_tabs_empty_state,
                R.string.tabswitcher_no_tabs_open_to_visit_different_pages);
        mCoordinator.attachEmptyView();
        View emptyView = mRootView.getChildAt(0);

        PropertyModel tabModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(CARD_TYPE, TAB)
                        .build();
        mModel.add(new ListItem(TAB, tabModel));

        mCoordinator.setIsTabSwitcherShowing(true);
        assertEquals(View.GONE, emptyView.getVisibility());
    }

    @Test
    public void testEmptyStateVisibility_OnlyArchivedMessage() {
        mCoordinator.initializeEmptyStateView(
                R.drawable.phone_tab_switcher_empty_state_illustration_static,
                R.string.tabswitcher_no_tabs_empty_state,
                R.string.tabswitcher_no_tabs_open_to_visit_different_pages);
        mCoordinator.attachEmptyView();
        View emptyView = mRootView.getChildAt(0);

        PropertyModel messageModel =
                new PropertyModel.Builder(MessageCardViewProperties.ALL_KEYS)
                        .with(CARD_TYPE, MESSAGE)
                        .with(MESSAGE_TYPE, ARCHIVED_TABS_MESSAGE)
                        .build();
        mModel.add(new ListItem(MESSAGE, messageModel));

        // Lay out a single message card of MESSAGE_CARD_HEIGHT, offset by MESSAGE_CARD_TOP.
        View msgCard = new View(mContext);
        msgCard.setLayoutParams(
                new RecyclerView.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, MESSAGE_CARD_HEIGHT));
        mRecyclerView.setLayoutManager(new LinearLayoutManager(mContext));
        mRecyclerView.setAdapter(new SingleViewAdapter(msgCard));
        mRecyclerView.setPadding(0, MESSAGE_CARD_TOP, 0, 0);
        mRecyclerView.measure(
                View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY));
        mRecyclerView.layout(0, 0, 100, 200);
        assertEquals(msgCard, mRecyclerView.getChildAt(0));
        assertEquals(MESSAGE_CARD_TOP, msgCard.getTop());
        assertEquals(MESSAGE_CARD_HEIGHT, msgCard.getMeasuredHeight());

        mCoordinator.setIsTabSwitcherShowing(true);
        assertEquals(View.VISIBLE, emptyView.getVisibility());

        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) emptyView.getLayoutParams();
        int rowMargin =
                mContext.getResources().getDimensionPixelSize(R.dimen.default_list_row_padding);
        assertEquals(rowMargin + MESSAGE_CARD_HEIGHT + MESSAGE_CARD_TOP, params.topMargin);
    }

    @Test
    public void testEmptyStateVisibility_NoArchivedMessage() {
        mCoordinator.initializeEmptyStateView(
                R.drawable.phone_tab_switcher_empty_state_illustration_static,
                R.string.tabswitcher_no_tabs_empty_state,
                R.string.tabswitcher_no_tabs_open_to_visit_different_pages);
        mCoordinator.attachEmptyView();
        View emptyView = mRootView.getChildAt(0);

        mRecyclerView.setTop(20);

        mCoordinator.setIsTabSwitcherShowing(true);
        assertEquals(View.VISIBLE, emptyView.getVisibility());

        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) emptyView.getLayoutParams();
        int rowMargin =
                mContext.getResources().getDimensionPixelSize(R.dimen.default_list_row_padding);
        assertEquals(rowMargin + 20, params.topMargin);
    }

    @Test
    public void testDestroyEmptyView() {
        mCoordinator.initializeEmptyStateView(
                R.drawable.phone_tab_switcher_empty_state_illustration_static,
                R.string.tabswitcher_no_tabs_empty_state,
                R.string.tabswitcher_no_tabs_open_to_visit_different_pages);
        mCoordinator.attachEmptyView();

        assertEquals(1, mRootView.getChildCount());

        mCoordinator.destroyEmptyView();

        assertEquals(0, mRootView.getChildCount());
    }

    @Test
    public void testLayoutChangeListener() {
        mCoordinator.initializeEmptyStateView(
                R.drawable.phone_tab_switcher_empty_state_illustration_static,
                R.string.tabswitcher_no_tabs_empty_state,
                R.string.tabswitcher_no_tabs_open_to_visit_different_pages);
        mCoordinator.attachEmptyView();
        View emptyView = mRootView.getChildAt(0);

        mRecyclerView.setTop(0);

        mCoordinator.setIsTabSwitcherShowing(true);

        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) emptyView.getLayoutParams();
        int rowMargin =
                mContext.getResources().getDimensionPixelSize(R.dimen.default_list_row_padding);
        assertEquals(rowMargin, params.topMargin);

        // Change layout
        mRecyclerView.setTop(20);
        mRootView.layout(0, 0, 100, 200);

        assertEquals(rowMargin + 20, params.topMargin);
    }

    @Test
    public void testObserverUpdatesVisibility() {
        mCoordinator.initializeEmptyStateView(
                R.drawable.phone_tab_switcher_empty_state_illustration_static,
                R.string.tabswitcher_no_tabs_empty_state,
                R.string.tabswitcher_no_tabs_open_to_visit_different_pages);
        mCoordinator.attachEmptyView();
        View emptyView = mRootView.getChildAt(0);

        mCoordinator.setIsTabSwitcherShowing(true);
        assertEquals(View.VISIBLE, emptyView.getVisibility());

        // Add a tab, observer should trigger and hide empty view.
        PropertyModel tabModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(CARD_TYPE, TAB)
                        .build();
        mModel.add(new ListItem(TAB, tabModel));
        assertEquals(View.GONE, emptyView.getVisibility());

        // Remove the tab, observer should trigger and show empty view.
        mModel.removeAt(0);
        assertEquals(View.VISIBLE, emptyView.getVisibility());
    }
}
