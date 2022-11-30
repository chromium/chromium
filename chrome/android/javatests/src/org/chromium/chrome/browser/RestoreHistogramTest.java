// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MetricsUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
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
        LibraryLoader.getInstance().ensureInitialized();
        MetricsUtils.HistogramDelta noRestoreDelta = new MetricsUtils.HistogramDelta(
                ChromeBackupAgentImpl.HISTOGRAM_ANDROID_RESTORE_RESULT,
                ChromeBackupAgentImpl.RestoreStatus.NO_RESTORE);
        MetricsUtils.HistogramDelta restoreCompletedDelta = new MetricsUtils.HistogramDelta(
                ChromeBackupAgentImpl.HISTOGRAM_ANDROID_RESTORE_RESULT,
                ChromeBackupAgentImpl.RestoreStatus.RESTORE_COMPLETED);
        MetricsUtils.HistogramDelta restoreStatusRecorded = new MetricsUtils.HistogramDelta(
                ChromeBackupAgentImpl.HISTOGRAM_ANDROID_RESTORE_RESULT,
                ChromeBackupAgentImpl.RestoreStatus.RESTORE_STATUS_RECORDED);

        // Check behavior with no preference set
        ChromeBackupAgentImpl.recordRestoreHistogram();
        Assert.assertEquals(1, noRestoreDelta.getDelta());
        Assert.assertEquals(0, restoreCompletedDelta.getDelta());
        Assert.assertEquals(ChromeBackupAgentImpl.RestoreStatus.RESTORE_STATUS_RECORDED,
                ChromeBackupAgentImpl.getRestoreStatus());

        // Check behavior with a restore status
        ChromeBackupAgentImpl.setRestoreStatus(
                ChromeBackupAgentImpl.RestoreStatus.RESTORE_COMPLETED);
        ChromeBackupAgentImpl.recordRestoreHistogram();
        Assert.assertEquals(1, noRestoreDelta.getDelta());
        Assert.assertEquals(1, restoreCompletedDelta.getDelta());
        Assert.assertEquals(ChromeBackupAgentImpl.RestoreStatus.RESTORE_STATUS_RECORDED,
                ChromeBackupAgentImpl.getRestoreStatus());

        // Second call should record nothing (note this assumes it doesn't record something totally
        // random)
        ChromeBackupAgentImpl.recordRestoreHistogram();
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
        LibraryLoader.getInstance().ensureInitialized();
        MetricsUtils.HistogramDelta noRestoreDelta = new MetricsUtils.HistogramDelta(
                ChromeBackupAgentImpl.HISTOGRAM_ANDROID_RESTORE_RESULT,
                ChromeBackupAgentImpl.RestoreStatus.NO_RESTORE);

        // Histogram should be written the first time the activity is started.
        mActivityTestRule.startMainActivityOnBlankPage();
        Assert.assertEquals(1, noRestoreDelta.getDelta());
    }
}
