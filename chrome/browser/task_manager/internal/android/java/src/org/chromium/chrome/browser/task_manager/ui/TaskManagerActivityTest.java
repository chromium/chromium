// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.COLUMNS;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.CPU;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.MEMORY_FOOTPRINT;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.PROCESS_ID;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.TASK_ID;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.TASK_NAME;

import android.app.Activity;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.RowType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
public class TaskManagerActivityTest {
    Activity mActivity;
    ModelList mModel;
    RecyclerView mRecyclerView;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().visible().get();
        mActivity.setTheme(R.style.Theme_Chromium_Activity);

        mModel = new ModelList();
        TaskManagerActivity.bind(mActivity, mModel);

        mRecyclerView = mActivity.findViewById(R.id.tasks_view);
        assertTrue(mRecyclerView.isShown());
    }

    @Test
    @SmallTest
    public void testTaskManagerActivity() {
        PropertyKey[] propertyKeys =
                new PropertyKey[] {TASK_ID, TASK_NAME, MEMORY_FOOTPRINT, CPU, PROCESS_ID};

        mModel.add(
                new ListItem(
                        RowType.HEADER,
                        new PropertyModel.Builder(COLUMNS).with(COLUMNS, propertyKeys).build()));
        mModel.add(
                new ListItem(
                        RowType.TASK,
                        new PropertyModel.Builder(propertyKeys)
                                .with(TASK_ID, 1)
                                .with(TASK_NAME, "foo")
                                .with(MEMORY_FOOTPRINT, 1_000_000)
                                .with(CPU, 0.5F)
                                .with(PROCESS_ID, 1234)
                                .build()));

        mRecyclerView.layout(0, 0, 1024, 640); // let the items render

        TextView taskName =
                mRecyclerView
                        .findViewHolderForAdapterPosition(1)
                        .itemView
                        .findViewById(R.id.task_name);
        assertEquals("foo", taskName.getText().toString());
    }
}
