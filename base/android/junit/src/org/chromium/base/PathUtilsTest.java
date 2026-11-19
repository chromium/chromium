// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.content.ContextWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.io.File;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

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

        assertEquals(expectedDataDir, dataDir);
        assertEquals(expectedCacheDir, cacheDir);
        assertEquals(expectedThumbnailDir, thumbnailDir);
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

        assertEquals(expectedDataDir, dataDir);
        assertEquals(expectedCacheDir, cacheDir);
        assertEquals(expectedThumbnailDir, thumbnailDir);
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

        assertEquals(expectedDataDir, dataDir);
        assertEquals(expectedCacheDir, cacheDir);
        assertEquals(expectedThumbnailDir, thumbnailDir);
    }

    @Test
    public void testGetDownloadsDirectoryCaching() {
        AtomicInteger callCount = new AtomicInteger();
        AtomicReference<File[]> externalDirsRef = new AtomicReference<>(new File[0]);
        Context wrapper =
                new ContextWrapper(ApplicationProvider.getApplicationContext()) {
                    @Override
                    public File[] getExternalFilesDirs(String type) {
                        callCount.incrementAndGet();
                        return externalDirsRef.get();
                    }
                };
        ContextUtils.initApplicationContextForTests(wrapper);

        // Unmounted storage returning empty array should not be cached.
        assertEquals("", PathUtils.getDownloadsDirectory());
        assertEquals(1, callCount.get());

        // Once mounted, the primary downloads directory is resolved and cached.
        externalDirsRef.set(
                new File[] {
                    new File("/storage/emulated/0/Download"), new File("/storage/sdcard1/Download")
                });
        assertEquals("/storage/emulated/0/Download", PathUtils.getDownloadsDirectory());
        assertEquals(2, callCount.get());

        // Subsequent getDownloadsDirectory() calls hit the cache without invoking
        // getExternalFilesDirs(), while getAllPrivateDownloadsDirectories() remains live.
        externalDirsRef.set(
                new File[] {
                    new File("/storage/emulated/0/Download2"), new File("/storage/sdcard2/Download")
                });
        assertEquals("/storage/emulated/0/Download", PathUtils.getDownloadsDirectory());
        assertEquals(2, callCount.get());
        assertArrayEquals(
                new String[] {"/storage/emulated/0/Download2", "/storage/sdcard2/Download"},
                PathUtils.getAllPrivateDownloadsDirectories());
        assertEquals(3, callCount.get());

        // resetForTesting() clears the cached downloads directory.
        PathUtils.resetForTesting();
        assertEquals("/storage/emulated/0/Download2", PathUtils.getDownloadsDirectory());
        assertEquals(4, callCount.get());

        // setAllPrivateDownloadsDirectoriesForTesting() overrides the populated cache.
        PathUtils.setAllPrivateDownloadsDirectoriesForTesting(
                new String[] {"/storage/override/Download"});
        assertEquals("/storage/override/Download", PathUtils.getDownloadsDirectory());
        PathUtils.setAllPrivateDownloadsDirectoriesForTesting(new String[0]);
        assertEquals("", PathUtils.getDownloadsDirectory());
        assertEquals(4, callCount.get());
    }
}
