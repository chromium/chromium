// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskInfo.NetworkType;
import org.chromium.components.offline_items_collection.ContentId;

import java.util.UUID;
import java.util.concurrent.TimeUnit;

/**
 * Test class to validate that the {@link DownloadResumptionScheduler} correctly interacts with the
 * {@link BackgroundTaskScheduler} based on the parameters stored about currently running downloads
 * in the persistence layer exposed by {@link DownloadSharedPreferenceHelper}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DownloadResumptionSchedulerTest {
    @Mock
    private BackgroundTaskScheduler mScheduler;

    @Rule
    public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        FeatureUtilities.setDownloadAutoResumptionEnabledInNativeForTesting(false);
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mScheduler);
    }

    @After
    public void tearDown() {
        removePersistentEntries();
    }

    @Test
    @Feature({"Download"})
    public void testCancelRequest() {
        DownloadResumptionScheduler.getDownloadResumptionScheduler().cancel();

        verify(mScheduler, never()).schedule(any(), any());
        verify(mScheduler, times(1)).cancel(any(), eq(TaskIds.DOWNLOAD_RESUMPTION_JOB_ID));
    }

    @Test
    @Feature({"Download"})
    public void testScheduleRequestWithNoDownloads() {
        DownloadResumptionScheduler.getDownloadResumptionScheduler().scheduleIfNecessary();

        verify(mScheduler, never()).schedule(any(), any());
        verify(mScheduler, times(1)).cancel(any(), eq(TaskIds.DOWNLOAD_RESUMPTION_JOB_ID));
    }

    @Test
    @Feature({"Download"})
    public void testScheduleRequestWithNoResumableDownloads() {
        populatePersistentEntries(new DownloadSharedPreferenceEntry[] {
                buildEntry(false /* isAutoResumable */, false /* meteredOk */),
        });

        DownloadResumptionScheduler.getDownloadResumptionScheduler().scheduleIfNecessary();

        verify(mScheduler, never()).schedule(any(), any());
        verify(mScheduler, times(1)).cancel(any(), eq(TaskIds.DOWNLOAD_RESUMPTION_JOB_ID));
    }

    @Test
    @Feature({"Download"})
    public void testScheduleRequestWithOneUnmeteredResumableDownload() {
        populatePersistentEntries(new DownloadSharedPreferenceEntry[] {
                buildEntry(true /* isAutoResumable */, false /* meteredOk */),
        });

        DownloadResumptionScheduler.getDownloadResumptionScheduler().scheduleIfNecessary();

        ArgumentCaptor<TaskInfo> taskCaptor = ArgumentCaptor.forClass(TaskInfo.class);
        verify(mScheduler, times(1)).schedule(any(), taskCaptor.capture());
        verify(mScheduler, never()).cancel(any(), anyInt());

        assertTaskIsDownloadResumptionTask(taskCaptor.getValue(), false /* meteredOk */);
    }

    @Test
    @Feature({"Download"})
    public void testScheduleRequestWithOneMeteredResumableDownload() {
        populatePersistentEntries(new DownloadSharedPreferenceEntry[] {
                buildEntry(true /* isAutoResumable */, true /* meteredOk */),
        });

        DownloadResumptionScheduler.getDownloadResumptionScheduler().scheduleIfNecessary();

        ArgumentCaptor<TaskInfo> taskCaptor = ArgumentCaptor.forClass(TaskInfo.class);
        verify(mScheduler, times(1)).schedule(any(), taskCaptor.capture());
        verify(mScheduler, never()).cancel(any(), anyInt());

        assertTaskIsDownloadResumptionTask(taskCaptor.getValue(), true /* meteredOk */);
    }

    @Test
    @Feature({"Download"})
    public void testScheduleRequestWithOneMeteredAndOneUnmeteredResumableDownload() {
        populatePersistentEntries(new DownloadSharedPreferenceEntry[] {
                buildEntry(true /* isAutoResumable */, true /* meteredOk */),
                buildEntry(true /* isAutoResumable */, false /* meteredOk */),
        });

        DownloadResumptionScheduler.getDownloadResumptionScheduler().scheduleIfNecessary();

        ArgumentCaptor<TaskInfo> taskCaptor = ArgumentCaptor.forClass(TaskInfo.class);
        verify(mScheduler, times(1)).schedule(any(), taskCaptor.capture());
        verify(mScheduler, never()).cancel(any(), anyInt());

        assertTaskIsDownloadResumptionTask(taskCaptor.getValue(), true /* meteredOk */);
    }

    @Test
    @Feature({"Download"})
    public void testScheduleRequestWithOneMeteredAndOneUnresumableDownload() {
        populatePersistentEntries(new DownloadSharedPreferenceEntry[] {
                buildEntry(true /* isAutoResumable */, true /* meteredOk */),
                buildEntry(false /* isAutoResumable */, false /* meteredOk */),
        });

        DownloadResumptionScheduler.getDownloadResumptionScheduler().scheduleIfNecessary();

        ArgumentCaptor<TaskInfo> taskCaptor = ArgumentCaptor.forClass(TaskInfo.class);
        verify(mScheduler, times(1)).schedule(any(), taskCaptor.capture());
        verify(mScheduler, never()).cancel(any(), anyInt());

        assertTaskIsDownloadResumptionTask(taskCaptor.getValue(), true /* meteredOk */);
    }

    @Test
    @Feature({"Download"})
    public void testScheduleRequestWithOneUnmeteredAndOneUnresumableDownload() {
        populatePersistentEntries(new DownloadSharedPreferenceEntry[] {
                buildEntry(true /* isAutoResumable */, false /* meteredOk */),
                buildEntry(false /* isAutoResumable */, false /* meteredOk */),
        });

        DownloadResumptionScheduler.getDownloadResumptionScheduler().scheduleIfNecessary();

        ArgumentCaptor<TaskInfo> taskCaptor = ArgumentCaptor.forClass(TaskInfo.class);
        verify(mScheduler, times(1)).schedule(any(), taskCaptor.capture());
        verify(mScheduler, never()).cancel(any(), anyInt());

        assertTaskIsDownloadResumptionTask(taskCaptor.getValue(), false /* meteredOk */);
    }

    @Test
    @Feature({"Download"})
    public void testScheduleRequestWithMultipleResumableDownloads() {
        populatePersistentEntries(new DownloadSharedPreferenceEntry[] {
                buildEntry(false /* isAutoResumable */, false /* meteredOk */),
                buildEntry(false /* isAutoResumable */, true /* meteredOk */),
                buildEntry(true /* isAutoResumable */, false /* meteredOk */),
        });

        DownloadResumptionScheduler.getDownloadResumptionScheduler().scheduleIfNecessary();

        ArgumentCaptor<TaskInfo> taskCaptor = ArgumentCaptor.forClass(TaskInfo.class);
        verify(mScheduler, times(1)).schedule(any(), taskCaptor.capture());
        verify(mScheduler, never()).cancel(any(), anyInt());

        assertTaskIsDownloadResumptionTask(taskCaptor.getValue(), false /* meteredOk */);
    }

    // Helper method to validate that the queued task is correctly set.
    private static void assertTaskIsDownloadResumptionTask(TaskInfo task, boolean meteredOk) {
        @NetworkType
        int expectedNetworkType =
                meteredOk ? TaskInfo.NetworkType.ANY : TaskInfo.NetworkType.UNMETERED;

        assertNotNull(task);
        assertEquals(TaskIds.DOWNLOAD_RESUMPTION_JOB_ID, task.getTaskId());
        assertEquals(DownloadResumptionBackgroundTask.class, task.getBackgroundTaskClass());
        assertTrue(task.getExtras().isEmpty());
        assertEquals(expectedNetworkType, task.getRequiredNetworkType());
        assertFalse(task.requiresCharging());
        assertTrue(task.shouldUpdateCurrent());
        assertFalse(task.isPeriodic());
        assertNotNull(task.getOneOffInfo());
        assertFalse(task.getOneOffInfo().hasWindowStartTimeConstraint());
        assertEquals(TimeUnit.DAYS.toMillis(1), task.getOneOffInfo().getWindowEndTimeMs());
    }

    // Helper method to push a set of test download entries into the persistence layer.
    private static void populatePersistentEntries(DownloadSharedPreferenceEntry[] entries) {
        DownloadSharedPreferenceHelper prefStore = DownloadSharedPreferenceHelper.getInstance();
        for (DownloadSharedPreferenceEntry entry : entries) {
            prefStore.addOrReplaceSharedPreferenceEntry(entry);
        }
    }

    // Helper method to remove all test download entries from the persistence layer.
    private static void removePersistentEntries() {
        DownloadSharedPreferenceHelper prefStore = DownloadSharedPreferenceHelper.getInstance();
        while (prefStore.getEntries().size() > 0) {
            prefStore.removeSharedPreferenceEntry(prefStore.getEntries().get(0).id);
        }
    }

    // Helper method to build test download entries.
    private static DownloadSharedPreferenceEntry buildEntry(
            boolean isAutoResumable, boolean meteredOk) {
        return new DownloadSharedPreferenceEntry(
                new ContentId("test", UUID.randomUUID().toString()), 1 /* notificationId */,
                false /* offTheRecord */, meteredOk, "fileName", isAutoResumable,
                false /* isTransient */);
    }
}