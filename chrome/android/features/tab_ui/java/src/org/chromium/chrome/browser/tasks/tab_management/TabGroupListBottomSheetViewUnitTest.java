// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.RowType.EXISTING_GROUP;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Unit tests for {@link TabGroupListBottomSheetView}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupListBottomSheetViewUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    // These values are chosen somewhat arbitrarily.
    private static final int CONTAINER_HEIGHT_FOR_TEST = 1000;
    private static final int CONTAINER_WIDTH_FOR_TEST = 500;
    private static final int ITEM_HEIGHT_FOR_TEST = 50;
    private static final int ITEM_WIDTH_FOR_TEST = 500;

    @Mock private SimpleRecyclerViewAdapter mAdapter;
    @Mock private PropertyModel mTabGroupListRowPropertyModel;
    @Mock private BottomSheetController mBottomSheetController;
    private Context mContext;
    private TabGroupListBottomSheetView mBottomSheetView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mBottomSheetView = new TabGroupListBottomSheetView(mContext, mBottomSheetController, true);
    }

    @Test
    public void testConstruction() {
        assertNotNull(mBottomSheetView.getContentView());
        RecyclerView recyclerView =
                mBottomSheetView.getContentView().findViewById(R.id.tab_group_parity_recycler_view);
        assertNotNull(recyclerView);
        assertNotNull(recyclerView.getLayoutManager());
    }

    @Test
    public void testSetRecyclerViewAdapter() {
        mBottomSheetView.setRecyclerViewAdapter(mAdapter);
        RecyclerView recyclerView =
                mBottomSheetView.getContentView().findViewById(R.id.tab_group_parity_recycler_view);
        assertEquals(mAdapter, recyclerView.getAdapter());
    }

    @Test
    public void testGetSheetContentDescription_withNewGroupRow() {
        String expectedDescription =
                mContext.getString(
                        R.string.tab_group_list_with_add_button_bottom_sheet_content_description);
        assertEquals(expectedDescription, mBottomSheetView.getSheetContentDescription(mContext));
    }

    @Test
    public void testGetSheetContentDescription_withoutNewGroupRow() {
        mBottomSheetView = new TabGroupListBottomSheetView(mContext, mBottomSheetController, false);
        String expectedDescription =
                mContext.getString(R.string.tab_group_list_bottom_sheet_content_description);
        assertEquals(expectedDescription, mBottomSheetView.getSheetContentDescription(mContext));
    }

    @Test
    public void testSheetHeight_withFewItems() {
        when(mBottomSheetController.getContainerHeight()).thenReturn(CONTAINER_HEIGHT_FOR_TEST);
        when(mBottomSheetController.getContainerWidth()).thenReturn(CONTAINER_WIDTH_FOR_TEST);
        mBottomSheetView = new TabGroupListBottomSheetView(mContext, mBottomSheetController, false);
        ModelList modelList = new ModelList();
        addItemsToModelList(modelList, 2);
        SimpleRecyclerViewAdapter adapter = createAdapterForTesting(modelList);
        mBottomSheetView.setRecyclerViewAdapter(adapter);

        // Because the bottom sheet also includes padding and the "Add to" header, using
        // CONTAINER_HEIGHT_FOR_TEST and ITEM_HEIGHT_FOR_TEST isn't sufficient to calculate the
        // ratio.
        assertEquals(0.18d, mBottomSheetView.getFullHeightRatio(), /* delta= */ 0.01);
        assertEquals(0.18, mBottomSheetView.getHalfHeightRatio(), /* delta= */ 0.01);
    }

    @Test
    public void testSheetHeight_withManyItems() {
        when(mBottomSheetController.getContainerHeight()).thenReturn(CONTAINER_HEIGHT_FOR_TEST);
        when(mBottomSheetController.getContainerWidth()).thenReturn(CONTAINER_WIDTH_FOR_TEST);
        mBottomSheetView = new TabGroupListBottomSheetView(mContext, mBottomSheetController, false);
        ModelList modelList = new ModelList();
        addItemsToModelList(modelList, 10);
        SimpleRecyclerViewAdapter adapter = createAdapterForTesting(modelList);
        mBottomSheetView.setRecyclerViewAdapter(adapter);

        // Because the bottom sheet also includes padding and the "Add to" header, using
        // CONTAINER_HEIGHT_FOR_TEST and ITEM_HEIGHT_FOR_TEST isn't sufficient to calculate the
        // ratio.
        assertEquals(0.58d, mBottomSheetView.getFullHeightRatio(), /* delta= */ 0.01);
        assertEquals(0.5, mBottomSheetView.getHalfHeightRatio(), /* delta= */ 0.01);
    }

    @Test
    public void testSheetHeight_withItemsTallerThanContent() {
        when(mBottomSheetController.getContainerHeight()).thenReturn(CONTAINER_HEIGHT_FOR_TEST);
        when(mBottomSheetController.getContainerWidth()).thenReturn(CONTAINER_WIDTH_FOR_TEST);
        mBottomSheetView = new TabGroupListBottomSheetView(mContext, mBottomSheetController, false);
        ModelList modelList = new ModelList();
        addItemsToModelList(modelList, 25);
        SimpleRecyclerViewAdapter adapter = createAdapterForTesting(modelList);
        mBottomSheetView.setRecyclerViewAdapter(adapter);

        // Because the bottom sheet also includes padding and the "Add to" header, using
        // CONTAINER_HEIGHT_FOR_TEST and ITEM_HEIGHT_FOR_TEST isn't sufficient to calculate the
        // ratio.
        assertEquals(1.0, mBottomSheetView.getFullHeightRatio(), /* delta= */ 0.01);
        assertEquals(0.5, mBottomSheetView.getHalfHeightRatio(), /* delta= */ 0.01);
    }

    private void addItemsToModelList(ModelList modelList, int numItems) {
        for (int i = 0; i < numItems; i++) {
            modelList.add(new ListItem(EXISTING_GROUP, mTabGroupListRowPropertyModel));
        }
    }

    private static SimpleRecyclerViewAdapter createAdapterForTesting(ModelList modelList) {
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(modelList);
        adapter.registerType(
                EXISTING_GROUP,
                TabGroupListBottomSheetViewUnitTest::buildItemView,
                TabGroupListBottomSheetViewUnitTest::bindItemView);
        return adapter;
    }

    private static View buildItemView(ViewGroup parent) {
        final var itemView = new View(parent.getContext());
        itemView.setLayoutParams(new LayoutParams(ITEM_WIDTH_FOR_TEST, ITEM_HEIGHT_FOR_TEST));
        return itemView;
    }

    private static void bindItemView(PropertyModel model, View itemView, PropertyKey key) {
        final var lp = itemView.getLayoutParams();
        lp.height = ITEM_HEIGHT_FOR_TEST;
        lp.width = ITEM_WIDTH_FOR_TEST;
    }
}
