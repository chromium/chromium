// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager.ui;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.task_manager.RefreshType;
import org.chromium.chrome.browser.task_manager.TaskManagerObserver;
import org.chromium.chrome.browser.task_manager.TaskManagerServiceBridge;
import org.chromium.chrome.browser.task_manager.TaskManagerServiceBridgeJni;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
public class TaskManagerMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TaskManagerServiceBridge.Natives mBridge;

    private PropertyModel mHeader;
    private ModelList mTasks;
    private TaskManagerMediator mMediator;
    private TaskManagerObserver mObserver;

    @Before
    public void setUp() {
        TaskManagerServiceBridgeJni.setInstanceForTesting(mBridge);

        mHeader = new PropertyModel(TaskManagerProperties.COLUMNS);
        mTasks = new ModelList();
        mMediator =
                new TaskManagerMediator(
                        1000,
                        mHeader,
                        mTasks,
                        TaskManagerProperties.TASK_ID,
                        TaskManagerProperties.MEMORY_FOOTPRINT);
        mMediator.startObserving();

        ArgumentCaptor<TaskManagerObserver> observerCaptor =
                ArgumentCaptor.forClass(TaskManagerObserver.class);
        verify(mBridge)
                .addObserver(observerCaptor.capture(), eq(1000), eq(RefreshType.MEMORY_FOOTPRINT));

        mObserver = observerCaptor.getValue();
    }

    @Test
    @SmallTest
    public void testTasksAreSorted() {
        when(mBridge.getMemoryFootprintUsage(1)).thenReturn(1_000_000L);
        when(mBridge.getMemoryFootprintUsage(2)).thenReturn(2_000_000L);

        // ModelList should be sorted by the task id regardless of the order of addition.
        mObserver.onTaskAdded(2);
        mObserver.onTaskAdded(1);

        assertArrayEquals(
                mHeader.get(TaskManagerProperties.COLUMNS),
                new PropertyKey[] {
                    TaskManagerProperties.TASK_ID, TaskManagerProperties.MEMORY_FOOTPRINT
                });

        assertEquals(mTasks.get(0).type, TaskManagerProperties.RowType.TASK);
        assertEquals(mTasks.get(0).model.get(TaskManagerProperties.TASK_ID), 1);
        assertEquals(mTasks.get(0).model.get(TaskManagerProperties.MEMORY_FOOTPRINT), 1_000_000);

        assertEquals(mTasks.get(1).type, TaskManagerProperties.RowType.TASK);
        assertEquals(mTasks.get(1).model.get(TaskManagerProperties.TASK_ID), 2);
        assertEquals(mTasks.get(1).model.get(TaskManagerProperties.MEMORY_FOOTPRINT), 2_000_000);
    }

    @Test
    @SmallTest
    public void testTasksRefreshed() {
        when(mBridge.getMemoryFootprintUsage(1)).thenReturn(1_000_000L);
        mObserver.onTaskAdded(1);
        // Task 1 gets new memory footporint usage.
        when(mBridge.getMemoryFootprintUsage(1)).thenReturn(3_000_000L);
        mObserver.onTasksRefreshed(new long[] {1});

        assertEquals(mTasks.get(0).model.get(TaskManagerProperties.MEMORY_FOOTPRINT), 3_000_000);
    }

    @Test
    @SmallTest
    public void testTaskToBeRemoved() {
        when(mBridge.getMemoryFootprintUsage(1)).thenReturn(1_000_000L);
        when(mBridge.getMemoryFootprintUsage(2)).thenReturn(2_000_000L);

        mObserver.onTaskAdded(2);
        mObserver.onTaskAdded(1);
        // Task 1 gets removed.
        mObserver.onTaskToBeRemoved(1);
        assertEquals(mTasks.get(0).model.get(TaskManagerProperties.TASK_ID), 2);
    }
}
