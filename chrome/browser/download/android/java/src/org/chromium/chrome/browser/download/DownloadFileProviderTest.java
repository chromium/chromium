// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.mockito.Mockito.doReturn;

import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.download.DownloadDirectoryProvider.SecondaryStorageInfo;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

/** Test content URI can be generated correctly by {@link DownloadFileProvider}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class DownloadFileProviderTest {
    private static final String PRIMARY_STORAGE_DOWNLOAD_DIRECTORY_PATH =
            "/storage/emulated/1234/Download";

    private static final String EXTERNAL_SD_CARD_DOWNLOAD_DIRECTORY_PATH_LEGACY =
            "/storage/724E-59EE/Android/data/com.android.chrome/files/Download";

    private static final String EXTERNAL_SD_CARD_DOWNLOAD_DIRECTORY_PATH =
            "/storage/1AEF-1A1E/Download";

    private static final String PRIMARY_STORAGE_DOWNLOAD_PATH =
            PRIMARY_STORAGE_DOWNLOAD_DIRECTORY_PATH + "/app-wise-release.apk";

    private static final String EXTERNAL_SD_CARD_DOWNLOAD_PATH_LEGACY =
            EXTERNAL_SD_CARD_DOWNLOAD_DIRECTORY_PATH_LEGACY + "/app-wise-release.apk";

    private static final String EXTERNAL_SD_CARD_DOWNLOAD_PATH =
            EXTERNAL_SD_CARD_DOWNLOAD_DIRECTORY_PATH + "/app-wise-release.apk";

    @Mock private DownloadDirectoryProvider.Delegate mMockDirectoryDelegate;

    private SecondaryStorageInfo mSecondaryStorageInfo;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(new File(PRIMARY_STORAGE_DOWNLOAD_DIRECTORY_PATH))
                .when(mMockDirectoryDelegate)
                .getPrimaryDownloadDirectory();
        setUpSecondaryStorageInfo(
                EXTERNAL_SD_CARD_DOWNLOAD_DIRECTORY_PATH,
                EXTERNAL_SD_CARD_DOWNLOAD_DIRECTORY_PATH_LEGACY);
    }

    private void setUpSecondaryStorageInfo(String directory, String directoryPreR) {
        List<File> directories = new ArrayList<>();
        if (directory != null) directories.add(new File(directory));
        List<File> directoriesPreR = new ArrayList<>();
        if (directoryPreR != null) directoriesPreR.add(new File(directoryPreR));
        mSecondaryStorageInfo = new SecondaryStorageInfo(directories, directoriesPreR);
        doReturn(mSecondaryStorageInfo)
                .when(mMockDirectoryDelegate)
                .getSecondaryStorageDownloadDirectories();
    }

    /**
     * Verifies content URI generation. The URI result in the callback should match expected output.
     *
     * @param filePath The file path of a file.
     * @param expected The expected content URI.
     */
    private void verifyContentUri(String filePath, Uri expected) {
        Assert.assertEquals(
                expected, DownloadFileProvider.createContentUri(filePath, mMockDirectoryDelegate));
    }

    /**
     * Verifies the content URI can be parsed to a file path.
     *
     * @param uri Input content URI.
     * @param expectedFilePath The expected file path parsed from content URI.
     */
    private void verifyParseContentUri(Uri uri, String expectedFilePath) {
        String filePath = DownloadFileProvider.getFilePathFromUri(uri, mMockDirectoryDelegate);
        Assert.assertEquals(expectedFilePath, filePath);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testGenerateContentUri() {
        String packageName = ContextUtils.getApplicationContext().getPackageName();
        verifyContentUri(
                PRIMARY_STORAGE_DOWNLOAD_PATH,
                Uri.parse(
                        "content://"
                                + packageName
                                + ".DownloadFileProvider/download?file=app-wise-release.apk"));
        verifyContentUri("", Uri.EMPTY);
        verifyContentUri(PRIMARY_STORAGE_DOWNLOAD_DIRECTORY_PATH, Uri.EMPTY);
        verifyContentUri(
                EXTERNAL_SD_CARD_DOWNLOAD_PATH_LEGACY,
                Uri.parse(
                        "content://"
                                + packageName
                                + ".DownloadFileProvider/download_external?file=app-wise-release.apk"));
        verifyContentUri(
                EXTERNAL_SD_CARD_DOWNLOAD_PATH,
                Uri.parse(
                        "content://"
                                + packageName
                                + ".DownloadFileProvider/external_volume?file=app-wise-release.apk"));

        // Simulate download directories pre R.
        setUpSecondaryStorageInfo(null, EXTERNAL_SD_CARD_DOWNLOAD_DIRECTORY_PATH_LEGACY);
        verifyContentUri(
                EXTERNAL_SD_CARD_DOWNLOAD_PATH_LEGACY,
                Uri.parse(
                        "content://"
                                + packageName
                                + ".DownloadFileProvider/download_external?file=app-wise-release.apk"));
        verifyContentUri(EXTERNAL_SD_CARD_DOWNLOAD_PATH, Uri.EMPTY);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testParseContentUri() {
        String packageName = ContextUtils.getApplicationContext().getPackageName();
        verifyParseContentUri(
                Uri.parse(
                        "content://"
                                + packageName
                                + ".DownloadFileProvider/download?file=app-wise-release.apk"),
                PRIMARY_STORAGE_DOWNLOAD_PATH);
        verifyParseContentUri(Uri.EMPTY, null);
        verifyParseContentUri(
                Uri.parse(
                        "content://"
                                + packageName
                                + ".DownloadFileProvider/download?file=../../../app-wise-release.apk"),
                null);
        verifyParseContentUri(
                Uri.parse(
                        "content://"
                                + packageName
                                + ".DownloadFileProvider/download_external?file=app-wise-release.apk"),
                EXTERNAL_SD_CARD_DOWNLOAD_PATH_LEGACY);
        verifyParseContentUri(
                Uri.parse(
                        "content://"
                                + packageName
                                + ".DownloadFileProvider/external_volume?file=app-wise-release.apk"),
                EXTERNAL_SD_CARD_DOWNLOAD_PATH);

        // Simulate download directories pre R.
        setUpSecondaryStorageInfo(null, EXTERNAL_SD_CARD_DOWNLOAD_DIRECTORY_PATH_LEGACY);
        verifyParseContentUri(
                Uri.parse(
                        "content://"
                                + packageName
                                + ".DownloadFileProvider/download_external?file=app-wise-release.apk"),
                EXTERNAL_SD_CARD_DOWNLOAD_PATH_LEGACY);
        verifyParseContentUri(
                Uri.parse(
                        "content://"
                                + packageName
                                + ".DownloadFileProvider/external_volume?file=app-wise-release.apk"),
                null);
    }
}
