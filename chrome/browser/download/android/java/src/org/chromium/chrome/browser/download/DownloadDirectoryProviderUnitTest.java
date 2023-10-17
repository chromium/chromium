// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.annotation.SuppressLint;
import android.os.Build;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.util.TempDirectory;

import org.chromium.base.PathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.download.DownloadDirectoryProviderUnitTest.ShadowPathUtils;

import java.nio.file.Path;

/** Unit tests for DownloadDirectoryProvider. It mocks Android API behaviors. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowPathUtils.class})
@SuppressLint("NewApi")
public class DownloadDirectoryProviderUnitTest {
    private static final String PRIVATE_DIR_PRIMARY = "private_dir_primary";
    private static final String PRIVATE_DIR_SECONDARY = "private_dir_secondary";

    @Implements(PathUtils.class)
    static class ShadowPathUtils {
        private static String[] sAllPrivateDirs;
        private static String[] sExternalVolumes;
        private static String sDownloadDirectory;

        static void setAllPrivateDownloadsDirectories(String[] dirs) {
            sAllPrivateDirs = dirs;
        }

        @Implementation
        public static String[] getAllPrivateDownloadsDirectories() {
            return sAllPrivateDirs;
        }

        static void setExternalDownloadVolumesNames(String[] dirs) {
            // TODO(xingliu): Add Android R tests when Robolectric framework supports it.
            sExternalVolumes = dirs;
        }

        @Implementation
        public static String[] getExternalDownloadVolumesNames() {
            return sExternalVolumes;
        }

        static void setDownloadsDirectory(String dir) {
            sDownloadDirectory = dir;
        }

        @Implementation
        public static String getDownloadsDirectory() {
            return sDownloadDirectory;
        }
    }

    private TempDirectory mTempDir;
    private Path mPrimaryDir;
    private Path mSecondaryDir;

    @Before
    public void setUp() {
        ShadowLog.stream = System.out;
        mTempDir = new TempDirectory();
        mPrimaryDir = mTempDir.create(PRIVATE_DIR_PRIMARY);
        mSecondaryDir = mTempDir.create(PRIVATE_DIR_SECONDARY);
    }

    @After
    public void tearDown() {
        mTempDir.destroy();
    }

    @Test
    public void testGetPrimaryDownloadDirectory() {
        ShadowPathUtils.setDownloadsDirectory(mPrimaryDir.toFile().getAbsolutePath());
        Assert.assertEquals(
                mPrimaryDir.toFile().getAbsolutePath(),
                DownloadDirectoryProvider.getPrimaryDownloadDirectory().getAbsolutePath());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void testGetSecondaryDownloadDirectoryOnQ() {
        ShadowPathUtils.setAllPrivateDownloadsDirectories(
                new String[] {
                    mPrimaryDir.toFile().getAbsolutePath(), mSecondaryDir.toFile().getAbsolutePath()
                });
        Assert.assertEquals(
                1,
                DownloadDirectoryProvider.getSecondaryStorageDownloadDirectories()
                        .directoriesPreR
                        .size());
        Assert.assertNull(
                "Pre R the new SD card directory should be null",
                DownloadDirectoryProvider.getSecondaryStorageDownloadDirectories().directories);

        // Simulate no SD card on the device.
        ShadowPathUtils.setAllPrivateDownloadsDirectories(
                new String[] {mPrimaryDir.toFile().getAbsolutePath()});
        Assert.assertEquals(
                0,
                DownloadDirectoryProvider.getSecondaryStorageDownloadDirectories()
                        .directoriesPreR
                        .size());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void testIsDownloadOnSdCardOnQ() {
        ShadowPathUtils.setAllPrivateDownloadsDirectories(
                new String[] {
                    mPrimaryDir.toFile().getAbsolutePath(), mSecondaryDir.toFile().getAbsolutePath()
                });
        Assert.assertTrue(
                DownloadDirectoryProvider.isDownloadOnSDCard(
                        mSecondaryDir.toFile().getAbsolutePath() + "a.png"));
        Assert.assertFalse(
                DownloadDirectoryProvider.isDownloadOnSDCard(
                        mPrimaryDir.toFile().getAbsolutePath() + "a.png"));
        Assert.assertFalse(DownloadDirectoryProvider.isDownloadOnSDCard("content://something"));
        Assert.assertFalse(
                DownloadDirectoryProvider.isDownloadOnSDCard(
                        mTempDir.create("randomDir").toFile().getAbsolutePath()));
        Assert.assertFalse(DownloadDirectoryProvider.isDownloadOnSDCard(""));
        Assert.assertFalse(DownloadDirectoryProvider.isDownloadOnSDCard(null));
    }
}
