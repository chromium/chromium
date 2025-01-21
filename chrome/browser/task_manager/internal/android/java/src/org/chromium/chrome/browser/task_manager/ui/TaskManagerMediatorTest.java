// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager.ui;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.COLUMNS;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.HEADER_PROPERTY_KEYS;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.IS_SELECTED;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.MEMORY_FOOTPRINT;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.TASK_ID;
import static org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.TASK_NAME;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.task_manager.RefreshType;
import org.chromium.chrome.browser.task_manager.TaskManagerObserver;
import org.chromium.chrome.browser.task_manager.TaskManagerServiceBridge;
import org.chromium.chrome.browser.task_manager.TaskManagerServiceBridgeJni;
import org.chromium.chrome.browser.task_manager.ui.TaskManagerProperties.RowType;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
public class TaskManagerMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TaskManagerServiceBridge.Natives mBridge;
    @Mock private Callback<Boolean> mOnHasKillableSelectedTaskChanged;

    private PropertyModel mHeader;
    private ModelList mTasks;
    private TaskManagerMediator mMediator;
    private TaskManagerObserver mObserver;

    @Before
    public void setUp() {
        TaskManagerServiceBridgeJni.setInstanceForTesting(mBridge);

        mHeader = new PropertyModel(HEADER_PROPERTY_KEYS);
        mTasks = new ModelList();
        mMediator = new TaskManagerMediator(1000, mHeader, mTasks, TASK_NAME, MEMORY_FOOTPRINT);
        mMediator.onHasKillableSelectedTaskChanged(mOnHasKillableSelectedTaskChanged);
        mMediator.startObserving();

        ArgumentCaptor<TaskManagerObserver> observerCaptor =
                ArgumentCaptor.forClass(TaskManagerObserver.class);
        ArgumentCaptor<Integer> refreshTypeCaptor = ArgumentCaptor.forClass(Integer.class);

        verify(mBridge)
                .addObserver(observerCaptor.capture(), eq(1000), refreshTypeCaptor.capture());

        assertEquals(
                RefreshType.MEMORY_FOOTPRINT,
                refreshTypeCaptor.getValue() & RefreshType.MEMORY_FOOTPRINT);
        assertEquals(RefreshType.CPU, refreshTypeCaptor.getValue() & RefreshType.CPU);

        mObserver = observerCaptor.getValue();
    }

    @Test
    @SmallTest
    public void testBasicAttributes() {
        mObserver.onTaskAdded(1);

        assertArrayEquals(mHeader.get(COLUMNS), new PropertyKey[] {TASK_NAME, MEMORY_FOOTPRINT});

        assertEquals(RowType.TASK, mTasks.get(0).type);
        assertEquals(1, mTasks.get(0).model.get(TASK_ID));
    }

    @Test
    @SmallTest
    public void testTasksAreSorted() {
        when(mBridge.getMemoryFootprintUsage(1)).thenReturn(1_000_000L);
        when(mBridge.getMemoryFootprintUsage(2)).thenReturn(2_000_000L);
        when(mBridge.getMemoryFootprintUsage(3)).thenReturn(1_500_000L);

        mObserver.onTaskAdded(1);
        mObserver.onTaskAdded(2);

        // Tasks are added in order of addition by default.
        assertEquals(1_000_000L, mTasks.get(0).model.get(MEMORY_FOOTPRINT));
        assertEquals(2_000_000L, mTasks.get(1).model.get(MEMORY_FOOTPRINT));

        // Sort by memory footprint (desc).
        mMediator.cycleSortOrder(MEMORY_FOOTPRINT);
        assertEquals(2_000_000L, mTasks.get(0).model.get(MEMORY_FOOTPRINT));
        assertEquals(1_000_000L, mTasks.get(1).model.get(MEMORY_FOOTPRINT));

        mObserver.onTaskAdded(3);
        assertEquals(2_000_000L, mTasks.get(0).model.get(MEMORY_FOOTPRINT));
        assertEquals(1_500_000L, mTasks.get(1).model.get(MEMORY_FOOTPRINT));
        assertEquals(1_000_000L, mTasks.get(2).model.get(MEMORY_FOOTPRINT));

        when(mBridge.getMemoryFootprintUsage(3)).thenReturn(3_000_000L);
        mObserver.onTasksRefreshed(new long[] {3});
        assertEquals(3_000_000L, mTasks.get(0).model.get(MEMORY_FOOTPRINT));
        assertEquals(2_000_000L, mTasks.get(1).model.get(MEMORY_FOOTPRINT));
        assertEquals(1_000_000L, mTasks.get(2).model.get(MEMORY_FOOTPRINT));
    }

    @Test
    @SmallTest
    public void testCycleSortOrder() {
        when(mBridge.getMemoryFootprintUsage(1)).thenReturn(1_000_000L);
        when(mBridge.getMemoryFootprintUsage(2)).thenReturn(2_000_000L);

        mObserver.onTaskAdded(1);
        mObserver.onTaskAdded(2);

        mMediator.cycleSortOrder(MEMORY_FOOTPRINT); // desc

        assertEquals(2_000_000L, mTasks.get(0).model.get(MEMORY_FOOTPRINT));
        assertEquals(1_000_000L, mTasks.get(1).model.get(MEMORY_FOOTPRINT));

        mMediator.cycleSortOrder(MEMORY_FOOTPRINT); // asc

        assertEquals(1_000_000L, mTasks.get(0).model.get(MEMORY_FOOTPRINT));
        assertEquals(2_000_000L, mTasks.get(1).model.get(MEMORY_FOOTPRINT));

        mMediator.cycleSortOrder(MEMORY_FOOTPRINT); // unspecified
        mMediator.cycleSortOrder(MEMORY_FOOTPRINT); // desc

        assertEquals(2_000_000L, mTasks.get(0).model.get(MEMORY_FOOTPRINT));
        assertEquals(1_000_000L, mTasks.get(1).model.get(MEMORY_FOOTPRINT));
    }

    @Test
    @SmallTest
    public void testCycleSortOrderAscending() {
        when(mBridge.getTitle(1)).thenReturn("A");
        when(mBridge.getTitle(2)).thenReturn("C");
        when(mBridge.getTitle(3)).thenReturn("B");

        mObserver.onTaskAdded(1);
        mObserver.onTaskAdded(2);
        mObserver.onTaskAdded(3);

        mMediator.cycleSortOrder(TASK_NAME); // asc

        assertEquals("A", mTasks.get(0).model.get(TASK_NAME));
        assertEquals("B", mTasks.get(1).model.get(TASK_NAME));
        assertEquals("C", mTasks.get(2).model.get(TASK_NAME));

        mMediator.cycleSortOrder(TASK_NAME); // desc

        assertEquals("C", mTasks.get(0).model.get(TASK_NAME));
        assertEquals("B", mTasks.get(1).model.get(TASK_NAME));
        assertEquals("A", mTasks.get(2).model.get(TASK_NAME));

        mMediator.cycleSortOrder(TASK_NAME); // unspecified

        assertEquals("C", mTasks.get(0).model.get(TASK_NAME));
        assertEquals("B", mTasks.get(1).model.get(TASK_NAME));
        assertEquals("A", mTasks.get(2).model.get(TASK_NAME));
    }

    @Test
    @SmallTest
    public void testTasksRefreshed() {
        when(mBridge.getMemoryFootprintUsage(1)).thenReturn(1_000_000L);
        mObserver.onTaskAdded(1);
        // Task 1 gets new memory footporint usage.
        when(mBridge.getMemoryFootprintUsage(1)).thenReturn(3_000_000L);
        mObserver.onTasksRefreshed(new long[] {1});

        assertEquals(3_000_000, mTasks.get(0).model.get(MEMORY_FOOTPRINT));
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
        assertEquals(2, mTasks.get(0).model.get(TASK_ID));
    }

    @Test
    @SmallTest
    public void testTaskSelectionChange() {
        when(mBridge.isTaskKillable(1)).thenReturn(true);
        when(mBridge.isTaskKillable(2)).thenReturn(true);

        mObserver.onTaskAdded(1);
        mObserver.onTaskAdded(2);

        assertFalse(mTasks.get(0).model.get(IS_SELECTED));

        mMediator.toggleSelection(mTasks.get(0).model);

        assertTrue(mTasks.get(0).model.get(IS_SELECTED));
        verify(mOnHasKillableSelectedTaskChanged).onResult(true);

        mMediator.toggleSelection(mTasks.get(1).model);

        assertTrue(mTasks.get(1).model.get(IS_SELECTED));
        assertFalse(mTasks.get(0).model.get(IS_SELECTED));

        mMediator.toggleSelection(mTasks.get(1).model);

        verify(mOnHasKillableSelectedTaskChanged).onResult(false);
    }

    @Test
    @SmallTest
    public void testOnHasKillableSelectedTaskChanged() {
        when(mBridge.isTaskKillable(1)).thenReturn(true);
        when(mBridge.isTaskKillable(2)).thenReturn(false);

        mObserver.onTaskAdded(1);
        mObserver.onTaskAdded(2);

        mMediator.toggleSelection(mTasks.get(0).model);

        verify(mOnHasKillableSelectedTaskChanged).onResult(true);

        mMediator.toggleSelection(mTasks.get(1).model);

        verify(mOnHasKillableSelectedTaskChanged).onResult(false);
    }

    @Test
    @SmallTest
    public void testToggleColumnFiltering() {
        assertTrue(mMediator.toggleColumnFiltering(TASK_NAME));

        assertFalse(mMediator.toggleColumnFiltering(MEMORY_FOOTPRINT));

        assertArrayEquals(new PropertyKey[] {MEMORY_FOOTPRINT}, mHeader.get(COLUMNS));

        assertTrue(mMediator.toggleColumnFiltering(TASK_NAME));

        assertArrayEquals(new PropertyKey[] {TASK_NAME, MEMORY_FOOTPRINT}, mHeader.get(COLUMNS));
    }
}
