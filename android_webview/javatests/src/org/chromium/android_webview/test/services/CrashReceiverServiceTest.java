// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.os.ParcelFileDescriptor;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.nonembedded.crash.SystemWideCrashDirectories;
import org.chromium.android_webview.services.CrashReceiverService;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.test.util.Batch;

import java.io.File;
import java.io.IOException;
import java.util.Arrays;
import java.util.Collections;

/**
 * Instrumentation tests for CrashReceiverService. These tests are batched as UNIT_TESTS because
 * they don't actually launch any services or other components.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(EITHER_PROCESS) // These are unit tests
@Batch(Batch.UNIT_TESTS)
public class CrashReceiverServiceTest {
    /**
     * Ensure that the minidump copying doesn't trigger when we pass it invalid file descriptors.
     */
    @Test
    @MediumTest
    public void testCopyingAbortsForInvalidFds() {
        Assert.assertFalse(
                CrashReceiverService.copyMinidumps(
                        /* uid= */ 0,
                        new ParcelFileDescriptor[] {null, null},
                        Arrays.asList(null, null)));
        Assert.assertFalse(
                CrashReceiverService.copyMinidumps(
                        /* uid= */ 0, new ParcelFileDescriptor[0], Collections.emptyList()));
    }

    /** Ensure deleting temporary files used when copying minidumps works correctly. */
    @Test
    @MediumTest
    public void testDeleteFilesInDir() throws IOException {
        File webviewTmpDir = SystemWideCrashDirectories.getWebViewTmpCrashDir();
        if (!webviewTmpDir.isDirectory()) {
            Assert.assertTrue(webviewTmpDir.mkdir());
        }
        File testFile1 = new File(webviewTmpDir, "testFile1");
        File testFile2 = new File(webviewTmpDir, "testFile2");
        Assert.assertTrue(testFile1.createNewFile());
        Assert.assertTrue(testFile2.createNewFile());
        Assert.assertTrue(testFile1.exists());
        Assert.assertTrue(testFile2.exists());
        CrashReceiverService.deleteFilesInWebViewTmpDirIfExists();
        Assert.assertFalse(testFile1.exists());
        Assert.assertFalse(testFile2.exists());
    }
}
