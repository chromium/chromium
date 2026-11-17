// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Context;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.FileObserver;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FileUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;

/** Tests for {@link DexLoader}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class DexLoaderTest {
    /** Package of APK to load dex file from. */
    private static final String PACKAGE_WITH_DEX_TO_EXTRACT =
            "org.chromium.webapk.shell_apk.test.dex_optimizer";

    /** Name of dex files in DexOptimizer.apk. */
    private static final String DEX_ASSET_NAME = "canary.dex";

    private static final String DEX_ASSET_NAME2 = "canary2.dex";

    /** Classes to load to check whether dex is valid. */
    private static final String CANARY_CLASS_NAME =
            "org.chromium.webapk.shell_apk.test.canary.Canary";

    private static final String CANARY_CLASS_NAME2 =
            "org.chromium.webapk.shell_apk.test.canary.Canary2";

    private Context mContext;
    private Context mRemoteContext;
    private DexLoader mDexLoader;
    private File mLocalDexDir;

    /**
     * Monitors read files and modified files in the directory passed to the constructor. The lists
     * are thread-safe, because FileObserver calls onEvent() on its own thread.
     */
    private static class FileMonitor extends FileObserver {
        public final List<String> mReadPaths = new CopyOnWriteArrayList<>();
        public final List<String> mModifiedPaths = new CopyOnWriteArrayList<>();

        public FileMonitor(File directory) {
            super(directory.getPath());
        }

        @Override
        public void onEvent(int event, String path) {
            switch (event) {
                case FileObserver.ACCESS:
                    mReadPaths.add(path);
                    break;
                case FileObserver.CREATE:
                case FileObserver.DELETE:
                case FileObserver.DELETE_SELF:
                case FileObserver.MODIFY:
                // DexLoader writes the .dex file to a temporary file and then renames it.
                case FileObserver.MOVED_TO:
                    mModifiedPaths.add(path);
                    break;
                default:
                    break;
            }
        }
    }

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mRemoteContext = getRemoteContext(mContext);
        mDexLoader = new DexLoader();

        mLocalDexDir = mContext.getDir("dex", Context.MODE_PRIVATE);
        if (mLocalDexDir.exists()) {
            FileUtils.recursivelyDeleteFile(mLocalDexDir, FileUtils.DELETE_ALL);
            if (mLocalDexDir.exists()) {
                Assert.fail("Could not delete local dex directory.");
            }
        }
    }

    @Test
    @MediumTest
    public void testBasic() {
        ClassLoader loader =
                mDexLoader.load(mRemoteContext, DEX_ASSET_NAME, CANARY_CLASS_NAME, mLocalDexDir);
        Assert.assertNotNull(loader);
        Assert.assertTrue(canLoadCanaryClass(loader));

        // Check that the dex file was extracted to the local data directory and that a directory
        // was created for the optimized dex.
        Assert.assertTrue(mLocalDexDir.exists());
        File[] localDexDirFiles = mLocalDexDir.listFiles();
        Assert.assertNotNull(localDexDirFiles);

        boolean foundDexFile = false;
        boolean foundOptimizedDir = false;
        for (File f : localDexDirFiles) {
            if (f.isDirectory()) {
                if (f.getName().equals("optimized")) {
                    foundOptimizedDir = true;
                }
            } else if (f.getName().equals(DEX_ASSET_NAME)) {
                foundDexFile = true;
            }
        }

        Assert.assertTrue(foundDexFile);
        Assert.assertTrue(foundOptimizedDir);
    }

    /**
     * Test that {@link DexLoader#load()} does not extract the dex file from the APK if the dex file
     * was extracted in a previous call to {@link DexLoader#load()}
     */
    @Test
    @MediumTest
    public void testPreviouslyLoadedFromLocalDataDir() {
        Assert.assertTrue(mLocalDexDir.mkdir());

        {
            // Load dex the first time. This should extract the dex file from the APK's assets and
            // generate the optimized dex file.
            FileMonitor localDexDirMonitor = new FileMonitor(mLocalDexDir);
            localDexDirMonitor.startWatching();
            ClassLoader loader =
                    mDexLoader.load(
                            mRemoteContext, DEX_ASSET_NAME, CANARY_CLASS_NAME, mLocalDexDir);

            // FileObserver delivers events on its own thread, after load() returns. Wait for the
            // events before stopWatching(), or they can be lost. inotify keeps the event order:
            // the rename (MOVED_TO) comes before the read (ACCESS) of the class load. So after
            // the ACCESS event, no event of this block can reach the monitor of the next block.
            CriteriaHelper.pollInstrumentationThread(
                    () -> localDexDirMonitor.mModifiedPaths.contains(DEX_ASSET_NAME),
                    "No create, modify, or move event for " + DEX_ASSET_NAME);
            CriteriaHelper.pollInstrumentationThread(
                    () -> localDexDirMonitor.mReadPaths.contains(DEX_ASSET_NAME),
                    "No read event for " + DEX_ASSET_NAME);
            localDexDirMonitor.stopWatching();

            Assert.assertNotNull(loader);
            Assert.assertTrue(canLoadCanaryClass(loader));
        }
        {
            // Load dex a second time. We should use the already extracted dex file.
            FileMonitor localDexDirMonitor = new FileMonitor(mLocalDexDir);
            localDexDirMonitor.startWatching();
            ClassLoader loader =
                    mDexLoader.load(
                            mRemoteContext, DEX_ASSET_NAME, CANARY_CLASS_NAME, mLocalDexDir);
            localDexDirMonitor.stopWatching();

            // The returned ClassLoader should be valid.
            Assert.assertNotNull(loader);
            Assert.assertTrue(canLoadCanaryClass(loader));

            // The modified files (if any) should be .flock lock files.
            for (String modifiedPath : localDexDirMonitor.mModifiedPaths) {
                Assert.assertTrue(modifiedPath.endsWith(".flock"));
            }
        }
    }

    /**
     * Test loading a dex file from a directory which was previously used for loading a different
     * dex file.
     */
    @Test
    @MediumTest
    public void testLoadDifferentDexInLocalDataDir() {
        Assert.assertTrue(mLocalDexDir.mkdir());

        // Load canary.dex
        ClassLoader loader1 =
                mDexLoader.load(mRemoteContext, DEX_ASSET_NAME, CANARY_CLASS_NAME, mLocalDexDir);
        Assert.assertNotNull(loader1);
        Assert.assertTrue(canLoadCanaryClass(loader1));

        File canaryDexFile1 = new File(mLocalDexDir, DEX_ASSET_NAME);
        Assert.assertTrue(canaryDexFile1.exists());

        mDexLoader.deleteCachedDexes(mLocalDexDir);

        ClassLoader loader2 =
                mDexLoader.load(mRemoteContext, DEX_ASSET_NAME2, CANARY_CLASS_NAME2, mLocalDexDir);
        Assert.assertNotNull(loader2);
        Assert.assertTrue(canLoadClass(loader2, CANARY_CLASS_NAME2));

        // canary2.dex should have been extracted and the previously extracted canary.dex file
        // should have been deleted.
        Assert.assertTrue(new File(mLocalDexDir, DEX_ASSET_NAME2).exists());
        Assert.assertFalse(canaryDexFile1.exists());
    }

    /**
     * Test that {@link DexLoader#load()} extracts the dex file as a read-only file. Android 14+
     * refuses to load a dex file which the app can write to.
     */
    @Test
    @MediumTest
    public void testExtractedDexIsReadOnly() {
        ClassLoader loader =
                mDexLoader.load(mRemoteContext, DEX_ASSET_NAME, CANARY_CLASS_NAME, mLocalDexDir);
        Assert.assertNotNull(loader);
        Assert.assertTrue(canLoadCanaryClass(loader));

        File localDexFile = new File(mLocalDexDir, DEX_ASSET_NAME);
        Assert.assertTrue(localDexFile.exists());
        Assert.assertFalse(localDexFile.canWrite());

        // The temporary file which was used for the extraction should be gone.
        File[] localDexDirFiles = mLocalDexDir.listFiles();
        Assert.assertNotNull(localDexDirFiles);
        for (File f : localDexDirFiles) {
            Assert.assertFalse(f.getName(), f.getName().endsWith(".tmp"));
        }
    }

    /**
     * Test that {@link DexLoader#load()} makes a writable dex file from a previous WebAPK shell
     * version read-only in place, and that the dex file loads. The pre-existing file has the
     * content of canary2.dex, which is different from the asset with the same name. So the test
     * fails if load() extracts the asset again instead of reusing the file.
     */
    @Test
    @MediumTest
    public void testPreExistingWritableDexIsMadeReadOnlyAndLoads() throws IOException {
        Assert.assertTrue(mLocalDexDir.mkdir());

        // Extract the dex file the way that older WebAPK shells did: without making it
        // read-only. Use the content of canary2.dex to show if load() reuses this file.
        File localDexFile = new File(mLocalDexDir, DEX_ASSET_NAME);
        copyAssetToFile(mRemoteContext, DEX_ASSET_NAME2, localDexFile);
        Assert.assertTrue(localDexFile.canWrite());

        ClassLoader loader =
                mDexLoader.load(mRemoteContext, DEX_ASSET_NAME, CANARY_CLASS_NAME2, mLocalDexDir);
        Assert.assertNotNull(loader);
        Assert.assertTrue(canLoadClass(loader, CANARY_CLASS_NAME2));
        Assert.assertFalse(localDexFile.canWrite());
    }

    /**
     * Test that {@link DexLoader#load()} replaces an empty read-only dex file. The rename needs
     * write permission on the directory, not on the replaced file.
     */
    @Test
    @MediumTest
    public void testEmptyReadOnlyDexIsReplaced() throws IOException {
        Assert.assertTrue(mLocalDexDir.mkdir());
        File localDexFile = new File(mLocalDexDir, DEX_ASSET_NAME);
        Assert.assertTrue(localDexFile.createNewFile());
        Assert.assertTrue(localDexFile.setReadOnly());

        ClassLoader loader =
                mDexLoader.load(mRemoteContext, DEX_ASSET_NAME, CANARY_CLASS_NAME, mLocalDexDir);
        Assert.assertNotNull(loader);
        Assert.assertTrue(canLoadCanaryClass(loader));
        Assert.assertTrue(localDexFile.length() > 0);
        Assert.assertFalse(localDexFile.canWrite());
    }

    /** Copies an asset from {@code context}'s APK to a writable file. */
    private static void copyAssetToFile(Context context, String assetName, File destFile)
            throws IOException {
        try (InputStream inputStream = context.getAssets().open(assetName);
                OutputStream outputStream = new FileOutputStream(destFile)) {
            FileUtils.copyStream(inputStream, outputStream);
        }
    }

    /**
     * Returns the Context of the APK which contains dex with canary class implementation.
     *
     * @param context The test application's Context.
     * @return Context of the APK whcih provide DexOptimizerService.
     */
    private Context getRemoteContext(Context context) {
        try {
            return context.getApplicationContext()
                    .createPackageContext(
                            PACKAGE_WITH_DEX_TO_EXTRACT,
                            Context.CONTEXT_IGNORE_SECURITY | Context.CONTEXT_INCLUDE_CODE);
        } catch (NameNotFoundException e) {
            e.printStackTrace();
            Assert.fail("Could not get remote context");
            return null;
        }
    }

    /** Returns whether the ClassLoader can load {@link CANARY_CLASS_NAME} */
    private boolean canLoadCanaryClass(ClassLoader loader) {
        return canLoadClass(loader, CANARY_CLASS_NAME);
    }

    /** Returns whether the ClassLoader can load a class */
    private boolean canLoadClass(ClassLoader loader, String className) {
        try {
            loader.loadClass(className);
            return true;
        } catch (Exception e) {
            return false;
        }
    }
}
