// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.widget.FrameLayout;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Unit tests for {@link PinnedTabStripCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PinnedTabStripCoordinatorTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private TabListModel mTabListModel;
    @Mock private RecyclerView mTabListRecyclerView;
    @Mock private GridLayoutManager mLayoutManager;
    @Mock private PinnedTabStripMediator mMediator;

    private PinnedTabStripCoordinator mCoordinator;
    private final ArgumentCaptor<RecyclerView.OnScrollListener> mScrollListenerCaptor =
            ArgumentCaptor.forClass(RecyclerView.OnScrollListener.class);

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(TestActivity activity) {
        when(mTabListRecyclerView.getLayoutManager()).thenReturn(mLayoutManager);
        FrameLayout parentView = new FrameLayout(activity);
        mCoordinator =
                new PinnedTabStripCoordinator(
                        activity, parentView, mTabListRecyclerView, mTabListModel) {
                    @Override
                    PinnedTabStripMediator createMediator(
                            RecyclerView tabGridListRecyclerView,
                            TabListModel tabListModel,
                            TabListModel pinnedTabsModelList,
                            PropertyModel stripPropertyModel) {
                        return mMediator;
                    }
                };
    }

    @Test
    public void testSetsUpRecyclerView() {
        RecyclerView pinnedTabRecyclerView = mCoordinator.getPinnedTabsRecyclerViewForTesting();
        assert pinnedTabRecyclerView.getLayoutManager() != null;
        assert pinnedTabRecyclerView.getAdapter() != null;

        assertTrue(pinnedTabRecyclerView.getAdapter() instanceof SimpleRecyclerViewAdapter);
        assertTrue(pinnedTabRecyclerView.getLayoutManager() instanceof LinearLayoutManager);

        Assert.assertEquals(
                LinearLayoutManager.HORIZONTAL,
                ((LinearLayoutManager) pinnedTabRecyclerView.getLayoutManager()).getOrientation());
    }

    @Test
    public void testScrollListenerForwardsToMediator() {
        verify(mTabListRecyclerView).addOnScrollListener(mScrollListenerCaptor.capture());
        mScrollListenerCaptor.getValue().onScrolled(mTabListRecyclerView, 0, 0);
        verify(mMediator).onScrolled();
    }
}
