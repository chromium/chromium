// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui.util;

import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.hamcrest.Matchers.empty;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.android_webview.devui.util.WebViewCrashLogParser;
import org.chromium.android_webview.nonembedded.crash.CrashInfo;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for WebViewCrashLogParser. */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(EITHER_PROCESS) // These are unit tests
public class WebViewCrashLogParserTest {
    private static final String TEST_LOG_ENTRY =
            "{'crash-local-id':'123456abc','crash-capture-time':1234567890,"
                + "'crash-keys':{'app-package-name':'test.package','variations':'123456,7890'}}";

    @Rule public TemporaryFolder mTestLogDir = new TemporaryFolder();

    // Write the given string to a new file in the temp test folder.
    private File writeLogFile(String fileName, String content) throws IOException {
        File logFile = mTestLogDir.newFile(fileName);
        FileWriter writer = new FileWriter(logFile);
        writer.write(content);
        writer.close();
        return logFile;
    }

    @Test
    @MediumTest
    public void testParseMultipleFiles() throws Exception {
        final String[] expectedLocalIds = new String[] {"crash1", "crash2", "crash3", "crash4"};
        for (String localId : expectedLocalIds) {
            writeLogFile(
                    "crash_file_" + localId + ".json",
                    "{'crash-local-id':'" + localId + "','app-package-name':'test.package'}");
        }

        List<CrashInfo> crashInfoList =
                new WebViewCrashLogParser(mTestLogDir.getRoot()).loadCrashesInfo();
        List<String> actualLocalIds = new ArrayList<>();
        for (CrashInfo crashInfo : crashInfoList) actualLocalIds.add(crashInfo.localId);
        // Only asserting local Ids to make sure it's loaded correctly.
        Assert.assertThat(actualLocalIds, containsInAnyOrder(expectedLocalIds));
    }

    @Test
    @MediumTest
    public void testParseInvalidFiles() throws Exception {
        writeLogFile("crash_file_json.log", TEST_LOG_ENTRY);
        writeLogFile("crash_file_json", TEST_LOG_ENTRY);
        writeLogFile("crash_log", TEST_LOG_ENTRY);
        writeLogFile("crash_log.txt", TEST_LOG_ENTRY);
        writeLogFile("crash_log.json", "{'invalid_json':'value'");
        List<CrashInfo> crashInfoList =
                new WebViewCrashLogParser(mTestLogDir.getRoot()).loadCrashesInfo();
        Assert.assertThat(crashInfoList, empty());
    }

    @Test
    @MediumTest
    public void testParseNonExistDir() {
        List<CrashInfo> crashInfoList =
                new WebViewCrashLogParser(new File("non_exsiting_dir")).loadCrashesInfo();
        Assert.assertThat(crashInfoList, empty());
    }

    @Test
    @MediumTest
    public void testdeleteOldFiles() throws Exception {
        final long oldTimeStamp =
                System.currentTimeMillis() - TimeUnit.MILLISECONDS.convert(30, TimeUnit.DAYS);
        File oldFile1 =
                writeLogFile(
                        "crash_file_crash1.json",
                        "{'crash-local-id':'crash1','app-package-name':'test.package'}");
        oldFile1.setLastModified(oldTimeStamp);
        File oldFile2 =
                writeLogFile(
                        "crash_file_crash2.json",
                        "{'crash-local-id':'crash2','app-package-name':'test.package'}");
        oldFile2.setLastModified(oldTimeStamp - 1000);
        File newFile =
                writeLogFile(
                        "crash_file_crash3.json",
                        "{'crash-local-id':'crash3','app-package-name':'test.package'}");

        List<CrashInfo> crashInfoList =
                new WebViewCrashLogParser(mTestLogDir.getRoot()).loadCrashesInfo();
        Assert.assertEquals(1, crashInfoList.size());
        Assert.assertEquals("crash3", crashInfoList.get(0).localId);

        Assert.assertFalse(
                "Log file should be deleted because it's more than 30 days old", oldFile1.exists());
        Assert.assertFalse(
                "Log file should be deleted because it's more than 30 days old", oldFile2.exists());
        Assert.assertTrue(
                "Log file should not be deleted because it's less than 30 days old",
                newFile.exists());
    }
}
