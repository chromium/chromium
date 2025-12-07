// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.ALL_COLUMN_KEYS;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.COLUMNS;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.CPU;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.HEADER_PROPERTY_KEYS;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.IS_SELECTED;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.MEMORY_FOOTPRINT;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.NETWORK_USAGE;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.PROCESS_ID;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.SORT_DESCRIPTOR;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.TASK_ID;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.TASK_NAME;

import android.app.Activity;
import android.graphics.drawable.ColorDrawable;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.appcompat.widget.PopupMenu;
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

import java.util.List;

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

        PropertyKey[] columnKeys =
                new PropertyKey[] {TASK_NAME, MEMORY_FOOTPRINT, CPU, NETWORK_USAGE, PROCESS_ID};
        mTaskModelKeys =
                PropertyModel.concatKeys(columnKeys, new PropertyKey[] {TASK_ID, IS_SELECTED});

        mHeaderModel.set(COLUMNS, columnKeys);
    }

    @Test
    @SmallTest
    public void testTaskProperties() {
        PropertyModel task =
                new PropertyModel.Builder(mTaskModelKeys)
                        .with(TASK_ID, 1)
                        .with(TASK_NAME, "foo")
                        .with(MEMORY_FOOTPRINT, 1024_000)
                        .with(CPU, 0.5F)
                        .with(NETWORK_USAGE, 0)
                        .with(PROCESS_ID, 1234)
                        .with(IS_SELECTED, false)
                        .build();
        mTasksModel.add(new ListItem(RowType.TASK, task));

        mRecyclerView.layout(0, 0, 1024, 640); // let the items render

        assertNotNull(mHeaderView.findViewById(R.id.task_name));

        View taskView = mRecyclerView.findViewHolderForAdapterPosition(0).itemView;

        TextView taskName = taskView.findViewById(R.id.task_name);
        TextView memoryFootprint = taskView.findViewById(R.id.memory_footprint);
        TextView cpu = taskView.findViewById(R.id.cpu);
        TextView networkUsage = taskView.findViewById(R.id.network_usage);
        TextView processId = taskView.findViewById(R.id.process_id);

        assertEquals("foo", taskName.getText().toString());
        assertEquals("1,000K", memoryFootprint.getText().toString());
        assertEquals("0.5", cpu.getText().toString());
        assertEquals("0", networkUsage.getText().toString());
        assertEquals("1234", processId.getText().toString());

        task.set(MEMORY_FOOTPRINT, -1);
        task.set(CPU, Float.NaN);

        assertEquals("–", memoryFootprint.getText().toString());
        assertEquals("–", cpu.getText().toString());
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

    @Test
    @SmallTest
    public void testOnCreateContextMenu() {
        mHeaderModel.set(COLUMNS, new PropertyKey[] {TASK_NAME, CPU});

        // Get a real Menu instance.
        Menu menu = new PopupMenu(mActivity, null).getMenu();

        mCoordinator.onCreateContextMenuImpl(menu);

        assertEquals(ALL_COLUMN_KEYS.length, menu.size());

        MenuItem taskNameItem = menu.getItem(List.of(ALL_COLUMN_KEYS).indexOf(TASK_NAME));
        assertNotEquals("", taskNameItem.getTitle());
        assertTrue(taskNameItem.isCheckable());
        assertTrue(taskNameItem.isChecked());

        MenuItem processIdItem = menu.getItem(List.of(ALL_COLUMN_KEYS).indexOf(PROCESS_ID));
        assertFalse(processIdItem.isChecked());
    }
}
