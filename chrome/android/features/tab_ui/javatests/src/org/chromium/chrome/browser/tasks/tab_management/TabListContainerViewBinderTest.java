// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.lessThan;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.hamcrest.MockitoHamcrest.intThat;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;

import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Spy;

import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/** Tests for {@link TabListRecyclerView} and {@link TabListContainerViewBinder} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TabListContainerViewBinderTest extends BlankUiTestActivityTestCase {
    private PropertyModel mContainerModel;
    private PropertyModelChangeProcessor mMCP;
    private TabListRecyclerView mRecyclerView;
    @Spy private GridLayoutManager mGridLayoutManager;
    @Spy private LinearLayoutManager mLinearLayoutManager;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        BlankUiTestActivity.setTestLayout(R.layout.tab_list_recycler_view_layout);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecyclerView = getActivity().findViewById(R.id.tab_list_recycler_view);
                });

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContainerModel = new PropertyModel(TabListContainerProperties.ALL_KEYS);

                    mMCP =
                            PropertyModelChangeProcessor.create(
                                    mContainerModel,
                                    mRecyclerView,
                                    TabListContainerViewBinder::bind);
                });
    }

    private void setUpGridLayoutManager() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mGridLayoutManager = spy(new GridLayoutManager(getActivity(), 2));
                    mRecyclerView.setLayoutManager(mGridLayoutManager);
                });
    }

    private void setUpLinearLayoutManager() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mLinearLayoutManager = spy(new LinearLayoutManager(getActivity()));
                    mRecyclerView.setLayoutManager(mLinearLayoutManager);
                });
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSetInitialScrollIndex_Grid() {
        setUpGridLayoutManager();
        mRecyclerView.layout(0, 0, 100, 500);

        mContainerModel.set(TabListContainerProperties.MODE, TabListCoordinator.TabListMode.GRID);
        mContainerModel.set(TabListContainerProperties.INITIAL_SCROLL_INDEX, 5);

        // Offset will be view height (500) / 2 - tab card height calculated from TabUtils / 2
        verify(mGridLayoutManager, times(1))
                .scrollToPositionWithOffset(
                        eq(5),
                        intThat(allOf(lessThan(mRecyclerView.getHeight() / 2), greaterThan(0))));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSetInitialScrollIndex_List_NoTabs() {
        setUpLinearLayoutManager();
        mRecyclerView.layout(0, 0, 100, 500);

        mContainerModel.set(TabListContainerProperties.MODE, TabListCoordinator.TabListMode.LIST);
        mContainerModel.set(TabListContainerProperties.INITIAL_SCROLL_INDEX, 7);

        // Offset will be 0 to avoid divide by 0 with no tabs.
        verify(mLinearLayoutManager, times(1)).scrollToPositionWithOffset(eq(7), eq(0));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testSetInitialScrollIndex_List_WithTabs() {
        setUpLinearLayoutManager();
        mRecyclerView.layout(0, 0, 100, 500);

        doReturn(9).when(mLinearLayoutManager).getItemCount();
        int range = mRecyclerView.computeVerticalScrollRange();

        mContainerModel.set(TabListContainerProperties.MODE, TabListCoordinator.TabListMode.LIST);
        mContainerModel.set(TabListContainerProperties.INITIAL_SCROLL_INDEX, 5);

        // 9 Tabs at 900 scroll extent = 100 per tab. With view height of 500 the offset is
        // 500 / 2 - range / 9 / 2 = result.
        verify(mLinearLayoutManager, times(1))
                .scrollToPositionWithOffset(eq(5), eq(250 - range / 9 / 2));
    }

    @Override
    public void tearDownTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(mMCP::destroy);
        super.tearDownTest();
    }
}
