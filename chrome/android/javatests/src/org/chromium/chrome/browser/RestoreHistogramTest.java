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
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** This test tests the logic for writing the restore histogram at two different levels */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class RestoreHistogramTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    /**
     * Test that the fundamental method for writing the histogram {@link
     * ChromeBackupAgent#recordRestoreHistogram()} works correctly @Note This can't be tested in the
     * ChromeBackupAgent Junit test, since the histograms are written in the C++ code, and because
     * all the functions are static there is no easy way of mocking them in Mockito (one can disable
     * them, but that would spoil the point of the test).
     */
    @Test
    @SmallTest
    public void testHistogramWriter() {
        LibraryLoader.getInstance().ensureInitialized();
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        ChromeBackupAgentImpl.HISTOGRAM_ANDROID_RESTORE_RESULT,
                        ChromeBackupAgentImpl.RestoreStatus.NO_RESTORE);

        // Check behavior with no preference set
        ChromeBackupAgentImpl.recordRestoreHistogram();
        histogram.assertExpected();
        Assert.assertEquals(
                ChromeBackupAgentImpl.RestoreStatus.RESTORE_STATUS_RECORDED,
                ChromeBackupAgentImpl.getRestoreStatus());

        // Check behavior with a restore status
        histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        ChromeBackupAgentImpl.HISTOGRAM_ANDROID_RESTORE_RESULT,
                        ChromeBackupAgentImpl.RestoreStatus.RESTORE_COMPLETED);
        ChromeBackupAgentImpl.setRestoreStatus(
                ChromeBackupAgentImpl.RestoreStatus.RESTORE_COMPLETED);
        ChromeBackupAgentImpl.recordRestoreHistogram();
        histogram.assertExpected();
        Assert.assertEquals(
                ChromeBackupAgentImpl.RestoreStatus.RESTORE_STATUS_RECORDED,
                ChromeBackupAgentImpl.getRestoreStatus());

        // Second call should record nothing
        histogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(ChromeBackupAgentImpl.HISTOGRAM_ANDROID_RESTORE_RESULT)
                        .build();
        ChromeBackupAgentImpl.recordRestoreHistogram();
        histogram.assertExpected();
    }

    /**
     * Test that the histogram is written during Chrome first run.
     *
     * @throws InterruptedException
     */
    @Test
    @SmallTest
    public void testWritingHistogramAtStartup() throws InterruptedException {
        LibraryLoader.getInstance().ensureInitialized();
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        ChromeBackupAgentImpl.HISTOGRAM_ANDROID_RESTORE_RESULT,
                        ChromeBackupAgentImpl.RestoreStatus.NO_RESTORE);

        // Histogram should be written the first time the activity is started.
        mActivityTestRule.startMainActivityOnBlankPage();
        histogram.assertExpected();
    }
}
