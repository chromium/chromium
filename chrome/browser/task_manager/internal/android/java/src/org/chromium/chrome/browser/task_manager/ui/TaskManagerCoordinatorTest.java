// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.COLUMNS;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.CPU;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.HEADER_PROPERTY_KEYS;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.IS_SELECTED;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.MEMORY_FOOTPRINT;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.PROCESS_ID;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.SORT_DESCRIPTOR;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.TASK_ID;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.TASK_NAME;

import android.app.Activity;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.RowType;
import org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.SortDescriptor;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
public class TaskManagerCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TaskManagerMediator mMediator;

    Activity mActivity;
    PropertyModel mHeaderModel;
    ModelList mTasksModel;

    LinearLayout mHeaderView;
    RecyclerView mRecyclerView;

    TaskManagerCoordinator mCoordinator;

    PropertyKey[] mTaskModelKeys;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().visible().get();
        mActivity.setTheme(R.style.Theme_Chromium_Activity);

        mActivity.setContentView(R.layout.task_manager_activity);
        View taskManagerView = mActivity.findViewById(android.R.id.content);

        mHeaderModel = new PropertyModel(HEADER_PROPERTY_KEYS);
        mTasksModel = new ModelList();

        mCoordinator =
                new TaskManagerCoordinator(taskManagerView, mHeaderModel, mTasksModel, mMediator);

        mHeaderView = mActivity.findViewById(R.id.header_linear_layout);
        mRecyclerView = mActivity.findViewById(R.id.tasks_view);

        assertTrue(mHeaderView.isShown());
        assertTrue(mRecyclerView.isShown());

        PropertyKey[] columnKeys = new PropertyKey[] {TASK_NAME, MEMORY_FOOTPRINT, CPU, PROCESS_ID};
        mTaskModelKeys =
                PropertyModel.concatKeys(columnKeys, new PropertyKey[] {TASK_ID, IS_SELECTED});

        mHeaderModel.set(COLUMNS, columnKeys);
    }

    @Test
    @SmallTest
    public void testTaskProperties() {
        mTasksModel.add(
                new ListItem(
                        RowType.TASK,
                        new PropertyModel.Builder(mTaskModelKeys)
                                .with(TASK_ID, 1)
                                .with(TASK_NAME, "foo")
                                .with(MEMORY_FOOTPRINT, 1_000_000)
                                .with(CPU, 0.5F)
                                .with(PROCESS_ID, 1234)
                                .with(IS_SELECTED, false)
                                .build()));

        mRecyclerView.layout(0, 0, 1024, 640); // let the items render

        assertNotNull(mHeaderView.findViewById(R.id.task_name));

        TextView taskName =
                mRecyclerView
                        .findViewHolderForAdapterPosition(0)
                        .itemView
                        .findViewById(R.id.task_name);
        assertEquals("foo", taskName.getText().toString());
        // TODO(crbug.com/380188424): Test other properties on customizing stringification per type.
    }

    @Test
    @SmallTest
    public void testSelectedRowColor() {
        mTasksModel.add(
                new ListItem(
                        RowType.TASK,
                        new PropertyModel.Builder(mTaskModelKeys)
                                .with(IS_SELECTED, false)
                                .build()));

        mRecyclerView.layout(0, 0, 1024, 640);

        assertEquals(
                0,
                ((ColorDrawable)
                                mRecyclerView
                                        .findViewHolderForAdapterPosition(0)
                                        .itemView
                                        .getBackground())
                        .getColor());

        mTasksModel.get(0).model.set(IS_SELECTED, true);

        assertNotEquals(
                0,
                ((ColorDrawable)
                                mRecyclerView
                                        .findViewHolderForAdapterPosition(0)
                                        .itemView
                                        .getBackground())
                        .getColor());
    }

    @Test
    @SmallTest
    public void testSortIndicator() {
        TextView taskNameHeader = mHeaderView.findViewById(R.id.task_name);
        String defaultText = taskNameHeader.getText().toString();

        assertNotEquals("", defaultText);

        mHeaderModel.set(SORT_DESCRIPTOR, new SortDescriptor(TASK_NAME, false));
        assertNotEquals(defaultText, taskNameHeader.getText().toString());
    }
}
