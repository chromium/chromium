// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Unit tests for {@link TabGroupListBottomSheetView}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupListBottomSheetViewUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private SimpleRecyclerViewAdapter mAdapter;
    private Context mContext;
    private TabGroupListBottomSheetView mBottomSheetView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mBottomSheetView = new TabGroupListBottomSheetView(mContext, true);
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
        mBottomSheetView = new TabGroupListBottomSheetView(mContext, false);
        String expectedDescription =
                mContext.getString(R.string.tab_group_list_bottom_sheet_content_description);
        assertEquals(expectedDescription, mBottomSheetView.getSheetContentDescription(mContext));
    }
}
