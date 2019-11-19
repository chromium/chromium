// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui.util;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.crash.CrashInfo;
import org.chromium.android_webview.common.crash.CrashInfo.UploadState;
import org.chromium.android_webview.devui.util.UploadedCrashesInfoLoader;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Unit tests for UploadedCrashesInfoLoader.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS)
public class UploadedCrashesInfoLoaderTest {
    private static final String TEST_UPLOAD_TIME_SEC_STR = "1234567890";
    private static final long TEST_UPLOAD_TIME_MILLI =
            TimeUnit.SECONDS.toMillis(Long.parseLong(TEST_UPLOAD_TIME_SEC_STR));
    private static final String TEST_UPLOAD_ID = "0123456789abcdef";
    private static final String TEST_LOCAL_ID = "fedcba9876543210";

    private File mLogFile;

    private static void writeUploadLogs(File logFile, List<String> logEntries) throws IOException {
        FileWriter writer = new FileWriter(logFile, /* Appending */ false);
        BufferedWriter bw = new BufferedWriter(writer);
        for (String entry : logEntries) {
            bw.write(entry);
            bw.newLine();
        }
        bw.close();
    }

    @Before
    public void setUp() throws IOException {
        mLogFile = File.createTempFile("uploads", "log");
    }

    @After
    public void tearDown() {
        mLogFile.delete();
    }

    @Test
    @SmallTest
    public void testParseSingleEntry() throws IOException {
        List<String> logs = new ArrayList<>();
        logs.add(TEST_UPLOAD_TIME_SEC_STR + "," + TEST_UPLOAD_ID + "," + TEST_LOCAL_ID);
        writeUploadLogs(mLogFile, logs);

        UploadedCrashesInfoLoader crashesInfoLoader = new UploadedCrashesInfoLoader(mLogFile);
        List<CrashInfo> infoList = crashesInfoLoader.loadCrashesInfo();

        Assert.assertEquals(1, infoList.size());

        CrashInfo crashInfo = infoList.get(0);
        Assert.assertEquals(TEST_UPLOAD_TIME_MILLI, crashInfo.uploadTime);
        Assert.assertEquals(TEST_UPLOAD_ID, crashInfo.uploadId);
        Assert.assertEquals(TEST_LOCAL_ID, crashInfo.localId);
        Assert.assertEquals(UploadState.UPLOADED, crashInfo.uploadState);
    }

    @Test
    @SmallTest
    public void testParseMultipleEntries() throws IOException {
        List<String> logs = new ArrayList<>();
        for (int i = 1; i <= 4; ++i) {
            String testEntry = TEST_UPLOAD_TIME_SEC_STR + ","
                    + "upload" + Integer.toString(i) + ","
                    + "local" + Integer.toString(i);
            logs.add(testEntry);
        }
        writeUploadLogs(mLogFile, logs);

        UploadedCrashesInfoLoader crashesInfoLoader = new UploadedCrashesInfoLoader(mLogFile);
        List<CrashInfo> infoList = crashesInfoLoader.loadCrashesInfo();

        Assert.assertEquals(4, infoList.size());
        for (int i = 1; i <= 4; ++i) {
            CrashInfo crashInfo = infoList.get(i - 1);
            Assert.assertEquals(TEST_UPLOAD_TIME_MILLI, crashInfo.uploadTime);
            Assert.assertEquals("upload" + Integer.toString(i), crashInfo.uploadId);
            Assert.assertEquals("local" + Integer.toString(i), crashInfo.localId);
            Assert.assertEquals(UploadState.UPLOADED, crashInfo.uploadState);
        }
    }

    @Test
    @SmallTest
    public void testParseInvalidEntries() throws IOException {
        List<String> logs = new ArrayList<>();
        // Valid logs
        for (int i = 1; i <= 2; ++i) {
            String testEntry = TEST_UPLOAD_TIME_SEC_STR + ","
                    + "upload" + Integer.toString(i) + ","
                    + "local" + Integer.toString(i);
            logs.add(testEntry);
        }
        // Invalid logs
        // invalid upload time
        logs.add("12345678a,1a2b3c,4d5e6f");
        // missing upload time
        logs.add(",1a2b3c,4d5e6f");
        // empty log
        logs.add(",,");
        // missing upload id
        logs.add("123456789,,4d5e6f");
        // missing local id
        logs.add("123456789,1a2b3c,");
        // less components
        logs.add("123456789,1a2b3c");
        // too many components
        logs.add("123456789,1a2b3c,4d5e6f,1011121314");
        for (int i = 3; i <= 4; ++i) {
            String testEntry = TEST_UPLOAD_TIME_SEC_STR + ","
                    + "upload" + Integer.toString(i) + ","
                    + "local" + Integer.toString(i);
            logs.add(testEntry);
        }
        writeUploadLogs(mLogFile, logs);

        UploadedCrashesInfoLoader crashesInfoLoader = new UploadedCrashesInfoLoader(mLogFile);
        List<CrashInfo> infoList = crashesInfoLoader.loadCrashesInfo();

        Assert.assertEquals(4, infoList.size());
        for (int i = 1; i <= 4; ++i) {
            CrashInfo crashInfo = infoList.get(i - 1);
            Assert.assertEquals(TEST_UPLOAD_TIME_MILLI, crashInfo.uploadTime);
            Assert.assertEquals("upload" + Integer.toString(i), crashInfo.uploadId);
            Assert.assertEquals("local" + Integer.toString(i), crashInfo.localId);
            Assert.assertEquals(UploadState.UPLOADED, crashInfo.uploadState);
        }
    }

    @Test
    @SmallTest
    public void testParseNonExistFile() {
        mLogFile.delete();
        UploadedCrashesInfoLoader crashesInfoLoader = new UploadedCrashesInfoLoader(mLogFile);
        List<CrashInfo> infoList = crashesInfoLoader.loadCrashesInfo();
        Assert.assertEquals(0, infoList.size());
    }
}
