// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;

import com.google.android.gms.gcm.TaskParams;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.init.ServiceManagerStartupUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;

/**
 * Tests background tasks can be run in Service Manager only mode, i.e., without starting the full
 * browser.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
public final class ServicificationBackgroundServiceTest {
    private ServicificationBackgroundService mServicificationBackgroundService;
    private RandomAccessFile mMappedSpareFile;
    private File mSpareFile;

    /**
     The size of the persistent/shared memory to store histograms. It should be consistent with
     the |kAllocSize| in persistent_histograms.cc.
     */
    private static final int ALLOC_SIZE = 4 << 20; // 4 MiB

    private static final String APP_CHROME_DIR = "app_chrome";
    private static final String SPARE_FILE_NAME = "BrowserMetrics-spare.pma";
    private static final String TAG = "ServicificationStartupTest";

    @Before
    public void setUp() {
        mServicificationBackgroundService =
                new ServicificationBackgroundService(true /*supportsServiceManagerOnly*/);
        RecordHistogram.setDisabledForTests(true);
    }

    @After
    public void tearDown() {
        RecordHistogram.setDisabledForTests(false);
        closeBrowserMetricsSpareFile();
    }

    // Creates the memory-mapped file which is used to persist histograms data before native is
    // loaded.
    private void createBrowserMetricsSpareFile() {
        File appChromeDir = new File(
                ContextUtils.getApplicationContext().getApplicationInfo().dataDir, APP_CHROME_DIR);
        if (!appChromeDir.exists()) appChromeDir.mkdir();

        mSpareFile = new File(appChromeDir + File.separator + SPARE_FILE_NAME);
        try {
            mSpareFile.createNewFile();
        } catch (IOException e) {
            Log.d(TAG, "Fail to create file: %s", SPARE_FILE_NAME);
            return;
        }

        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            try {
                mMappedSpareFile = new RandomAccessFile(mSpareFile, "rw");

                MappedByteBuffer mappedByteBuffer = mMappedSpareFile.getChannel().map(
                        FileChannel.MapMode.READ_WRITE, 0, ALLOC_SIZE);

                mappedByteBuffer.put(0, (byte) 0);
                mappedByteBuffer.force();
                Assert.assertTrue(Byte.compare(mappedByteBuffer.get(0), (byte) 0) == 0);
            } catch (Exception e) {
                Log.d(TAG, "Fail to create memory-mapped file: %s", SPARE_FILE_NAME);
            }
        }
    }

    private void closeBrowserMetricsSpareFile() {
        if (mMappedSpareFile == null) return;

        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            try {
                mMappedSpareFile.close();
            } catch (IOException e) {
                Log.d(TAG, "Fail to close memory-mapped file: %s", SPARE_FILE_NAME);
            }
        }
    }

    private static void startServiceAndWaitForNative(
            ServicificationBackgroundService backgroundService) {
        backgroundService.onRunTask(new TaskParams(ServiceManagerStartupUtils.TASK_TAG));
        backgroundService.assertLaunchBrowserCalled();
        backgroundService.waitForNativeLoaded();
    }

    @Test
    @LargeTest
    @Feature({"ServicificationStartup"})
    @CommandLineFlags.Add({"force-fieldtrials=*Foo/Bar", "enable-features=UMABackgroundSessions"})
    public void testHistogramsPersistedWithServiceManagerOnlyStart() {
        createBrowserMetricsSpareFile();
        Assert.assertTrue(mSpareFile.exists());

        startServiceAndWaitForNative(mServicificationBackgroundService);
        ServicificationBackgroundService.assertOnlyServiceManagerStarted();

        mServicificationBackgroundService.assertPersistentHistogramsOnDiskSystemProfile();
        ServicificationBackgroundService.assertOnlyServiceManagerStarted();
    }

    @Test
    @MediumTest
    @Feature({"ServicificationStartup"})
    public void testFullBrowserStartsAfterServiceManager() {
        startServiceAndWaitForNative(mServicificationBackgroundService);
        ServicificationBackgroundService.assertOnlyServiceManagerStarted();

        // Now native is loaded in service manager only mode, lets try and load the full browser to
        // test the transition from service manager only to full browser.
        mServicificationBackgroundService.setSupportsServiceManagerOnly(false);
        startServiceAndWaitForNative(mServicificationBackgroundService);
        ServicificationBackgroundService.assertFullBrowserStarted();
    }
}
