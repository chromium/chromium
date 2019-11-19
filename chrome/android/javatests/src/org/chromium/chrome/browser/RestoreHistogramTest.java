// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MetricsUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/**
 * This test tests the logic for writing the restore histogram at two different levels
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class RestoreHistogramTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    /**
     * Test that the fundamental method for writing the histogram
     * {@link ChromeBackupAgent#recordRestoreHistogram()} works correctly
     *
     * @Note This can't be tested in the ChromeBackupAgent Junit test, since the histograms are
     *       written in the C++ code, and because all the functions are static there is no easy way
     *       of mocking them in Mockito (one can disable them, but that would spoil the point of the
     *       test).
     */
    @Test
    @SmallTest
    public void testHistogramWriter() {
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
        MetricsUtils.HistogramDelta noRestoreDelta =
                new MetricsUtils.HistogramDelta(ChromeBackupAgent.HISTOGRAM_ANDROID_RESTORE_RESULT,
                        ChromeBackupAgent.RestoreStatus.NO_RESTORE);
        MetricsUtils.HistogramDelta restoreCompletedDelta =
                new MetricsUtils.HistogramDelta(ChromeBackupAgent.HISTOGRAM_ANDROID_RESTORE_RESULT,
                        ChromeBackupAgent.RestoreStatus.RESTORE_COMPLETED);
        MetricsUtils.HistogramDelta restoreStatusRecorded =
                new MetricsUtils.HistogramDelta(ChromeBackupAgent.HISTOGRAM_ANDROID_RESTORE_RESULT,
                        ChromeBackupAgent.RestoreStatus.RESTORE_STATUS_RECORDED);

        // Check behavior with no preference set
        ChromeBackupAgent.recordRestoreHistogram();
        Assert.assertEquals(1, noRestoreDelta.getDelta());
        Assert.assertEquals(0, restoreCompletedDelta.getDelta());
        Assert.assertEquals(ChromeBackupAgent.RestoreStatus.RESTORE_STATUS_RECORDED,
                ChromeBackupAgent.getRestoreStatus());

        // Check behavior with a restore status
        ChromeBackupAgent.setRestoreStatus(ChromeBackupAgent.RestoreStatus.RESTORE_COMPLETED);
        ChromeBackupAgent.recordRestoreHistogram();
        Assert.assertEquals(1, noRestoreDelta.getDelta());
        Assert.assertEquals(1, restoreCompletedDelta.getDelta());
        Assert.assertEquals(ChromeBackupAgent.RestoreStatus.RESTORE_STATUS_RECORDED,
                ChromeBackupAgent.getRestoreStatus());

        // Second call should record nothing (note this assumes it doesn't record something totally
        // random)
        ChromeBackupAgent.recordRestoreHistogram();
        Assert.assertEquals(0, restoreStatusRecorded.getDelta());
    }

    /**
     * Test that the histogram is written during Chrome first run.
     *
     * @throws InterruptedException
     */
    @Test
    @DisabledTest(message = "Test is flaky. crbug.com/875372")
    @SmallTest
    public void testWritingHistogramAtStartup() throws InterruptedException {
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
        MetricsUtils.HistogramDelta noRestoreDelta =
                new MetricsUtils.HistogramDelta(ChromeBackupAgent.HISTOGRAM_ANDROID_RESTORE_RESULT,
                        ChromeBackupAgent.RestoreStatus.NO_RESTORE);

        // Histogram should be written the first time the activity is started.
        mActivityTestRule.startMainActivityOnBlankPage();
        Assert.assertEquals(1, noRestoreDelta.getDelta());
    }
}
