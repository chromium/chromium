// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
import android.os.FileObserver;
import android.os.IBinder;
import android.os.RemoteException;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import dalvik.system.DexFile;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FileUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.webapk.shell_apk.test.dex_optimizer.IDexOptimizerService;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;

/**
 * Tests for {@link DexLoader}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class DexLoaderTest {
    /**
     * Package of APK to load dex file from and package which provides DexOptimizerService.
     */
    private static final String DEX_OPTIMIZER_SERVICE_PACKAGE =
            "org.chromium.webapk.shell_apk.test.dex_optimizer";

    /**
     * Class which implements DexOptimizerService.
     */
    private static final String DEX_OPTIMIZER_SERVICE_CLASS_NAME =
            "org.chromium.webapk.shell_apk.test.dex_optimizer.DexOptimizerServiceImpl";

    /**
     * Name of dex files in DexOptimizer.apk.
     */
    private static final String DEX_ASSET_NAME = "canary.dex";
    private static final String DEX_ASSET_NAME2 = "canary2.dex";

    /**
     * Classes to load to check whether dex is valid.
     */
    private static final String CANARY_CLASS_NAME =
            "org.chromium.webapk.shell_apk.test.canary.Canary";
    private static final String CANARY_CLASS_NAME2 =
            "org.chromium.webapk.shell_apk.test.canary.Canary2";

    private Context mContext;
    private Context mRemoteContext;
    private DexLoader mDexLoader;
    private File mLocalDexDir;
    private IDexOptimizerService mDexOptimizerService;
    private ServiceConnection mServiceConnection;

    /**
     * Monitors read files and modified files in the directory passed to the constructor.
     */
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
        mContext = InstrumentationRegistry.getTargetContext();
        mRemoteContext = getRemoteContext(mContext);
        mDexLoader = new DexLoader();

        mLocalDexDir = mContext.getDir("dex", Context.MODE_PRIVATE);
        if (mLocalDexDir.exists()) {
            FileUtils.recursivelyDeleteFile(mLocalDexDir);
            if (mLocalDexDir.exists()) {
                Assert.fail("Could not delete local dex directory.");
            }
        }

        connectToDexOptimizerService();

        try {
            if (!mDexOptimizerService.deleteDexDirectory()) {
                Assert.fail("Could not delete remote dex directory.");
            }
        } catch (RemoteException e) {
            e.printStackTrace();
            Assert.fail("Remote crashed during setup.");
        }
    }

    @After
    public void tearDown() {
        mContext.unbindService(mServiceConnection);
    }

    /**
     * Test that {@DexLoader#load()} can create a ClassLoader from a dex and optimized dex in
     * another app's data directory.
     */
    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.KITKAT)
    @DisabledTest(message = "crbug.com/871920")
    public void testLoadFromRemoteDataDir() {
        // Extract the dex file into another app's data directory and optimize the dex.
        String remoteDexFilePath = null;
        try {
            remoteDexFilePath = mDexOptimizerService.extractAndOptimizeDex();
        } catch (RemoteException e) {
            e.printStackTrace();
            Assert.fail("Remote crashed.");
        }

        if (remoteDexFilePath == null) {
            Assert.fail("Could not extract and optimize dex.");
        }

        // Check that the Android OS knows about the optimized dex file for
        // {@link remoteDexFilePath}.
        File remoteDexFile = new File(remoteDexFilePath);
        Assert.assertFalse(isDexOptNeeded(remoteDexFile));

        ClassLoader loader = mDexLoader.load(
                mRemoteContext, DEX_ASSET_NAME, CANARY_CLASS_NAME, remoteDexFile, mLocalDexDir);
        Assert.assertNotNull(loader);
        Assert.assertTrue(canLoadCanaryClass(loader));

        // Check that {@link DexLoader#load()} did not use the fallback path.
        Assert.assertFalse(mLocalDexDir.exists());
    }

    /**
     * That that {@link DexLoader#load()} falls back to extracting the dex from the APK to the
     * local data directory and creating the ClassLoader from the extracted dex if creating the
     * ClassLoader from the cached data in the remote Context's data directory fails.
     */
    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = 25, message = "crbug.com/999363")
    public void testLoadFromLocalDataDir() {
        ClassLoader loader = mDexLoader.load(
                mRemoteContext, DEX_ASSET_NAME, CANARY_CLASS_NAME, null, mLocalDexDir);
        Assert.assertNotNull(loader);
        Assert.assertTrue(canLoadCanaryClass(loader));

        // Check that the dex file was extracted to the local data directory and that a directory
        // was created for the optimized dex.
        Assert.assertTrue(mLocalDexDir.exists());
        File[] localDexDirFiles = mLocalDexDir.listFiles();
        Assert.assertNotNull(localDexDirFiles);
        Arrays.sort(localDexDirFiles);
        Assert.assertEquals(2, localDexDirFiles.length);
        Assert.assertEquals(DEX_ASSET_NAME, localDexDirFiles[0].getName());
        Assert.assertFalse(localDexDirFiles[0].isDirectory());
        Assert.assertEquals("optimized", localDexDirFiles[1].getName());
        Assert.assertTrue(localDexDirFiles[1].isDirectory());
    }

    /**
     * Test that {@link DexLoader#load()} does not extract the dex file from the APK if the dex file
     * was extracted in a previous call to {@link DexLoader#load()}
     */
    @Test
    @MediumTest
    public void testPreviouslyLoadedFromLocalDataDir() {
        Assert.assertTrue(mLocalDexDir.mkdir());

        // TODO(pkotwicz): fix on Android-Oreo.  See https://crbug.com/779218.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) return;

        {
            // Load dex the first time. This should extract the dex file from the APK's assets and
            // generate the optimized dex file.
            FileMonitor localDexDirMonitor = new FileMonitor(mLocalDexDir);
            localDexDirMonitor.startWatching();
            ClassLoader loader = mDexLoader.load(
                    mRemoteContext, DEX_ASSET_NAME, CANARY_CLASS_NAME, null, mLocalDexDir);
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
            ClassLoader loader = mDexLoader.load(
                    mRemoteContext, DEX_ASSET_NAME, CANARY_CLASS_NAME, null, mLocalDexDir);
            localDexDirMonitor.stopWatching();

            // The returned ClassLoader should be valid.
            Assert.assertNotNull(loader);
            Assert.assertTrue(canLoadCanaryClass(loader));

            // We should not have modified any files.
            Assert.assertTrue(localDexDirMonitor.mModifiedPaths.isEmpty());
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
        ClassLoader loader1 = mDexLoader.load(
                mRemoteContext, DEX_ASSET_NAME, CANARY_CLASS_NAME, null, mLocalDexDir);
        Assert.assertNotNull(loader1);
        Assert.assertTrue(canLoadCanaryClass(loader1));

        File canaryDexFile1 = new File(mLocalDexDir, DEX_ASSET_NAME);
        Assert.assertTrue(canaryDexFile1.exists());

        mDexLoader.deleteCachedDexes(mLocalDexDir);

        ClassLoader loader2 = mDexLoader.load(
                mRemoteContext, DEX_ASSET_NAME2, CANARY_CLASS_NAME2, null, mLocalDexDir);
        Assert.assertNotNull(loader2);
        Assert.assertTrue(canLoadClass(loader2, CANARY_CLASS_NAME2));

        // canary2.dex should have been extracted and the previously extracted canary.dex file
        // should have been deleted.
        Assert.assertTrue(new File(mLocalDexDir, DEX_ASSET_NAME2).exists());
        Assert.assertFalse(canaryDexFile1.exists());
    }

    /**
     * Connects to the DexOptimizerService.
     */
    private void connectToDexOptimizerService() {
        Intent intent = new Intent();
        intent.setComponent(
                new ComponentName(DEX_OPTIMIZER_SERVICE_PACKAGE, DEX_OPTIMIZER_SERVICE_CLASS_NAME));
        final CallbackHelper connectedCallback = new CallbackHelper();

        mServiceConnection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                mDexOptimizerService = IDexOptimizerService.Stub.asInterface(service);
                connectedCallback.notifyCalled();
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {}
        };

        try {
            mContext.bindService(intent, mServiceConnection, Context.BIND_AUTO_CREATE);
        } catch (SecurityException e) {
            e.printStackTrace();
            Assert.fail();
        }

        try {
            connectedCallback.waitForCallback(0);
        } catch (Exception e) {
            e.printStackTrace();
            Assert.fail("Could not connect to remote.");
        }
    }

    /**
     * Returns the Context of the APK which provides DexOptimizerService.
     * @param context The test application's Context.
     * @return Context of the APK whcih provide DexOptimizerService.
     */
    private Context getRemoteContext(Context context) {
        try {
            return context.getApplicationContext().createPackageContext(
                    DEX_OPTIMIZER_SERVICE_PACKAGE,
                    Context.CONTEXT_IGNORE_SECURITY | Context.CONTEXT_INCLUDE_CODE);
        } catch (NameNotFoundException e) {
            e.printStackTrace();
            Assert.fail("Could not get remote context");
            return null;
        }
    }

    /** Returns whether the Android OS thinks that a dex file needs to be re-optimized */
    private boolean isDexOptNeeded(File dexFile) {
        try {
            return DexFile.isDexOptNeeded(dexFile.getPath());
        } catch (Exception e) {
            e.printStackTrace();
            Assert.fail();
            return false;
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
