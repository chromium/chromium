// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui.util;

import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.hamcrest.Matchers.empty;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.crash.CrashInfo;
import org.chromium.android_webview.devui.util.WebViewCrashLogParser;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for WebViewCrashLogParser.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class WebViewCrashLogParserTest {
    private static final String TEST_LOG_ENTRY =
            "{\"crash-local-id\":\"123456abc\",\"crash-capture-time\":1234567890,"
            + "\"app-package-name\":\"test.package\",\"variations\":[\"123456\",\"7890\"]}";

    @Rule
    public TemporaryFolder mTestLogDir = new TemporaryFolder();

    // Write the given string to a new file in the temp test folder.
    private void writeLogFile(String fileName, String content) throws IOException {
        File logFile = mTestLogDir.newFile(fileName);
        FileWriter writer = new FileWriter(logFile);
        writer.write(content);
        writer.close();
    }

    @Test
    @MediumTest
    public void testParseMultipleFiles() throws Exception {
        final String[] expectedLocalIds = new String[] {"crash1", "crash2", "crash3", "crash4"};
        for (String localId : expectedLocalIds) {
            writeLogFile("crash_file_" + localId + ".json",
                    "{\"crash-local-id\":\"" + localId
                            + "\",\"app-package-name\":\"test.package\"}");
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
        writeLogFile("crash_log.json", "{\"invalid_json\":\"value\"");
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
}
