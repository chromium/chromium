// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import androidx.test.filters.MediumTest;

import org.json.JSONException;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.android_webview.nonembedded.crash.CrashInfo;
import org.chromium.android_webview.services.CrashLoggingUtils;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.test.util.Batch;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

/** Tests for {@link CrashLoggingUtils}. */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(EITHER_PROCESS) // These are unit tests
@Batch(Batch.UNIT_TESTS)
public class CrashLoggingUtilsTest {
    private static final String TEST_CRASH_LOCAL_ID = "abc1234";
    private static final String TEST_CRASH_FILE_NAME =
            "pkg-process-" + TEST_CRASH_LOCAL_ID + ".dmp";
    private static final String TEST_CRASH_PACKAGE = "org.test.package";

    @Rule public TemporaryFolder mTempTestDir = new TemporaryFolder();

    /** Test writing crash info to crash logs works correctly and logs the correct info. */
    @Test
    @MediumTest
    public void testWriteToLogFile() throws IOException, JSONException {
        File testLogFile = mTempTestDir.newFile("test_log_file");
        // No need to actually create it since nothing is written to it.
        File testCrashFile = mTempTestDir.newFile(TEST_CRASH_FILE_NAME);

        Map<String, String> crashInfoMap = new HashMap<String, String>();
        crashInfoMap.put("app-package-name", TEST_CRASH_PACKAGE);

        CrashLoggingUtils.writeCrashInfoToLogFile(testLogFile, testCrashFile, crashInfoMap);

        CrashInfo crashInfo = CrashInfo.readFromJsonString(readEntireFile(testLogFile));
        // Asserting some fields just to make sure that contents are valid.
        Assert.assertEquals(TEST_CRASH_LOCAL_ID, crashInfo.localId);
        Assert.assertEquals(testCrashFile.lastModified(), crashInfo.captureTime);
        Assert.assertEquals(
                TEST_CRASH_PACKAGE, crashInfo.getCrashKey(CrashInfo.APP_PACKAGE_NAME_KEY));
    }

    private static String readEntireFile(File file) throws IOException {
        try (FileInputStream fileInputStream = new FileInputStream(file)) {
            byte[] data = new byte[(int) file.length()];
            fileInputStream.read(data);
            return new String(data);
        }
    }
}
