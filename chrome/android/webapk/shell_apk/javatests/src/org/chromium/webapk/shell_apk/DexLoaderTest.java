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

import java.io.File;
import java.util.ArrayList;

/** Tests for {@link DexLoader}. */
@RunWith(BaseJUnit4ClassRunner.class)
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

    /** Monitors read files and modified files in the directory passed to the constructor. */
    private static class FileMonitor extends FileObserver {
        public ArrayList<String> mReadPaths = new ArrayList<String>();
        public ArrayList<String> mModifiedPaths = new ArrayList<String>();

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
            localDexDirMonitor.stopWatching();

            Assert.assertNotNull(loader);
            Assert.assertTrue(canLoadCanaryClass(loader));

            Assert.assertTrue(localDexDirMonitor.mReadPaths.contains(DEX_ASSET_NAME));
            Assert.assertTrue(localDexDirMonitor.mModifiedPaths.contains(DEX_ASSET_NAME));
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
