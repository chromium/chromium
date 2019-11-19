// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.os.ParcelFileDescriptor;
import android.support.test.filters.MediumTest;

import org.json.JSONException;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.crash.CrashInfo;
import org.chromium.android_webview.common.crash.SystemWideCrashDirectories;
import org.chromium.android_webview.services.CrashReceiverService;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

/**
 * Instrumentation tests for CrashReceiverService.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS)
public class CrashReceiverServiceTest {
    private static final String TEST_CRASH_LOCAL_ID = "abc1234";
    private static final String TEST_CRASH_FILE_NAME =
            "pkg-process-" + TEST_CRASH_LOCAL_ID + ".dmp";
    private static final String TEST_CRASH_PACKAGE = "org.test.package";

    @Rule
    public TemporaryFolder mTempTestDir = new TemporaryFolder();

    /**
     * Ensure that the minidump copying doesn't trigger when we pass it invalid file descriptors.
     */
    @Test
    @MediumTest
    public void testCopyingAbortsForInvalidFds() {
        Assert.assertFalse(CrashReceiverService.copyMinidumps(0 /* uid */, null, null));
        Assert.assertFalse(CrashReceiverService.copyMinidumps(
                0 /* uid */, new ParcelFileDescriptor[] {null, null}, null));
        Assert.assertFalse(
                CrashReceiverService.copyMinidumps(0 /* uid */, new ParcelFileDescriptor[0], null));
    }

    /**
     * Ensure deleting temporary files used when copying minidumps works correctly.
     */
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

    /**
     * Test writing crash info to crash logs works correctly and logs the correct info.
     */
    @Test
    @MediumTest
    public void testWriteToLogFile() throws IOException, JSONException {
        File testLogFile = mTempTestDir.newFile("test_log_file");
        // No need to actually create it since nothing is written to it.
        File testCrashFile = mTempTestDir.newFile(TEST_CRASH_FILE_NAME);

        Map<String, String> crashInfoMap = new HashMap<String, String>();
        crashInfoMap.put("app-package-name", TEST_CRASH_PACKAGE);

        CrashReceiverService.writeCrashInfoToLogFile(testLogFile, testCrashFile, crashInfoMap);

        CrashInfo crashInfo = CrashInfo.readFromJsonString(readEntireFile(testLogFile));
        // Asserting some fields just to make sure that contents are valid.
        Assert.assertEquals(TEST_CRASH_LOCAL_ID, crashInfo.localId);
        Assert.assertEquals(testCrashFile.lastModified(), crashInfo.captureTime);
        Assert.assertEquals(TEST_CRASH_PACKAGE, crashInfo.packageName);
    }

    private static String readEntireFile(File file) throws IOException {
        try (FileInputStream fileInputStream = new FileInputStream(file)) {
            byte[] data = new byte[(int) file.length()];
            fileInputStream.read(data);
            return new String(data);
        }
    }
}
