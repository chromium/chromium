// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertEquals;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.io.File;

/** junit tests for {@link PathUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PathUtilsTest {
    static final String THUMBNAIL_DIRECTORY_NAME = "textures";

    @Test
    public void testSetPrivateDataDirectorySuffix() {
        Context context = ApplicationProvider.getApplicationContext();
        String dataSuffix = "data_suffix";
        String cacheSubDir = "cache_subdir";
        String expectedDataDir = context.getDir(dataSuffix, Context.MODE_PRIVATE).getPath();
        String expectedThumbnailDir =
                context.getDir(THUMBNAIL_DIRECTORY_NAME, Context.MODE_PRIVATE).getPath();
        String expectedCacheDir = new File(context.getCacheDir(), cacheSubDir).getPath();

        PathUtils.setPrivateDataDirectorySuffix(dataSuffix, cacheSubDir);
        String dataDir = PathUtils.getDataDirectory();
        String cacheDir = PathUtils.getCacheDirectory();
        String thumbnailDir = PathUtils.getThumbnailCacheDirectory();

        assertEquals(dataDir, expectedDataDir);
        assertEquals(cacheDir, expectedCacheDir);
        assertEquals(thumbnailDir, expectedThumbnailDir);
    }

    @Test
    public void testIsPathUnderAppDirFalse() {
        Context context = ApplicationProvider.getApplicationContext();
        String dataPath = "/data_path/a/b/c";

        assertEquals(false, PathUtils.isPathUnderAppDir(dataPath, context));
    }

    @Test
    public void testIsPathUnderAppDirTrue() {
        Context context = ApplicationProvider.getApplicationContext();
        String dataSuffix = "data_suffix";
        String expectedDataDir = context.getDir(dataSuffix, Context.MODE_PRIVATE).getPath();

        assertEquals(true, PathUtils.isPathUnderAppDir(expectedDataDir, context));
    }

    @Test
    public void testSetPrivateDirectoryPath() {
        String dataPath = "/data_path/a/b/c";
        String cachePath = "/cache_path/a/b/c";
        String dataSuffix = "data_suffix";
        String cacheSubDir = "cache_subdir";
        String expectedDataDir = dataPath + "/" + dataSuffix;
        String expectedCacheDir = cachePath + "/" + cacheSubDir;
        String expectedThumbnailDir = dataPath + "/" + THUMBNAIL_DIRECTORY_NAME;

        PathUtils.setPrivateDirectoryPath(dataPath, cachePath, dataSuffix, cacheSubDir);
        String dataDir = PathUtils.getDataDirectory();
        String cacheDir = PathUtils.getCacheDirectory();
        String thumbnailDir = PathUtils.getThumbnailCacheDirectory();

        assertEquals(dataDir, expectedDataDir);
        assertEquals(cacheDir, expectedCacheDir);
        assertEquals(thumbnailDir, expectedThumbnailDir);
    }

    @Test
    public void testSetPrivateDirectoryPathWithoutBasePaths() {
        Context context = ApplicationProvider.getApplicationContext();
        String dataSuffix = "data_suffix";
        String cacheSubDir = "cache_subdir";
        String expectedDataDir = context.getDir(dataSuffix, Context.MODE_PRIVATE).getPath();
        String expectedThumbnailDir =
                context.getDir(THUMBNAIL_DIRECTORY_NAME, Context.MODE_PRIVATE).getPath();
        String expectedCacheDir = new File(context.getCacheDir(), cacheSubDir).getPath();

        PathUtils.setPrivateDirectoryPath(null, null, dataSuffix, cacheSubDir);
        String dataDir = PathUtils.getDataDirectory();
        String cacheDir = PathUtils.getCacheDirectory();
        String thumbnailDir = PathUtils.getThumbnailCacheDirectory();

        assertEquals(dataDir, expectedDataDir);
        assertEquals(cacheDir, expectedCacheDir);
        assertEquals(thumbnailDir, expectedThumbnailDir);
    }
}
