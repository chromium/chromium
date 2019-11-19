// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui.util;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.crash.CrashInfo;
import org.chromium.android_webview.common.crash.CrashInfo.UploadState;
import org.chromium.android_webview.devui.util.UnuploadedFilesStateLoader;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.components.minidump_uploader.CrashFileManager;

import java.io.File;
import java.io.IOException;
import java.util.List;

/**
 * Unit tests for UnuploadedFilesStateLoader.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS)
public class UnuploadedFilesStateLoaderTest {
    private static final String LOCAL_ID = "localId1234";
    private static final String TEST_FILE_NAME = "test_file-" + LOCAL_ID;

    @Rule
    public TemporaryFolder mTestCacheDir = new TemporaryFolder();

    private File mTestCrashDir;
    private CrashFileManager mCrashFileManager;
    private UnuploadedFilesStateLoader mCrashInfoLoader;

    @Before
    public void setup() {
        mCrashFileManager = new CrashFileManager(mTestCacheDir.getRoot());
        mCrashInfoLoader = new UnuploadedFilesStateLoader(mCrashFileManager);
        mTestCrashDir = mCrashFileManager.getCrashDirectory();
        mTestCrashDir.mkdirs();
    }

    @Test
    @SmallTest
    public void testParseEmptyDir() {
        List<CrashInfo> crashInfoList = mCrashInfoLoader.loadCrashesInfo();
        Assert.assertEquals(0, crashInfoList.size());
    }

    @Test
    @SmallTest
    public void testParseSkippedFiles() throws IOException {
        new File(mTestCrashDir, TEST_FILE_NAME + ".skipped.try0").createNewFile();
        new File(mTestCrashDir, TEST_FILE_NAME + ".skipped5673.try1").createNewFile();
        new File(mTestCrashDir, TEST_FILE_NAME + ".skipped5678.try30").createNewFile();

        List<CrashInfo> crashInfoList = mCrashInfoLoader.loadCrashesInfo();
        Assert.assertEquals(3, crashInfoList.size());

        for (CrashInfo crashInfo : crashInfoList) {
            Assert.assertEquals(crashInfo.localId, LOCAL_ID);
            Assert.assertEquals(crashInfo.uploadState, UploadState.SKIPPED);
        }
    }

    @Test
    @SmallTest
    public void testParsePendingFiles() throws IOException {
        new File(mTestCrashDir, TEST_FILE_NAME + ".dmp.try0").createNewFile();
        new File(mTestCrashDir, TEST_FILE_NAME + ".dmp5678.try1").createNewFile();
        new File(mTestCrashDir, TEST_FILE_NAME + ".dmp5678.try30").createNewFile();

        List<CrashInfo> crashInfoList = mCrashInfoLoader.loadCrashesInfo();
        Assert.assertEquals(3, crashInfoList.size());

        for (CrashInfo crashInfo : crashInfoList) {
            Assert.assertEquals(crashInfo.localId, LOCAL_ID);
            Assert.assertEquals(crashInfo.uploadState, UploadState.PENDING);
        }
    }

    @Test
    @SmallTest
    public void testParseForcedUploadFiles() throws IOException {
        new File(mTestCrashDir, TEST_FILE_NAME + ".forced.try0").createNewFile();
        new File(mTestCrashDir, TEST_FILE_NAME + ".forced5678.try1").createNewFile();
        new File(mTestCrashDir, TEST_FILE_NAME + ".forced5678.try30").createNewFile();

        List<CrashInfo> crashInfoList = mCrashInfoLoader.loadCrashesInfo();
        Assert.assertEquals(3, crashInfoList.size());

        for (CrashInfo crashInfo : crashInfoList) {
            Assert.assertEquals(crashInfo.localId, LOCAL_ID);
            Assert.assertEquals(crashInfo.uploadState, UploadState.PENDING_USER_REQUESTED);
        }
    }

    @Test
    @SmallTest
    public void testParseFilesWithInvalidSuffixes() throws IOException {
        new File(mTestCrashDir, TEST_FILE_NAME + ".log").createNewFile();
        new File(mTestCrashDir, TEST_FILE_NAME + ".json1232").createNewFile();
        new File(mTestCrashDir, TEST_FILE_NAME + ".log.try30").createNewFile();

        List<CrashInfo> crashInfoList = mCrashInfoLoader.loadCrashesInfo();
        Assert.assertEquals(0, crashInfoList.size());
    }
}
