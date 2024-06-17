// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.content.Context;
import android.content.Intent;
import android.os.ParcelFileDescriptor;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.common.services.INetLogService;
import org.chromium.android_webview.services.AwNetLogService;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;

import java.io.File;
import java.io.IOException;

/**
 * Instrumentation tests AwNetLogServiceTest. These tests are not batched to make sure all unbinded
 * services are properly killed between tests.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(EITHER_PROCESS) // These tests don't use the renderer process
@Batch(Batch.PER_CLASS)
public class AwNetLogServiceTest {
    private static final String TAG = "AwNetLogServiceTest";
    private static final String JSON_TAG = ".json";
    private static final String MOCK_PID = "1234_";
    private static final String PACKAGE_NAME =
            ContextUtils.getApplicationContext().getPackageName();

    @After
    public void tearDown() {
        File directory = AwNetLogService.getNetLogFileDirectory();
        File[] files = directory.listFiles();
        if (files != null) {
            for (File file : files) {
                file.delete();
            }
        }
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add(AwSwitches.NET_LOG)
    public void testStreamLog() throws Throwable {
        final long currentTime = System.currentTimeMillis();
        Intent intent = new Intent(ContextUtils.getApplicationContext(), AwNetLogService.class);
        try (ServiceConnectionHelper helper =
                new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            INetLogService service = INetLogService.Stub.asInterface(helper.getBinder());
            ParcelFileDescriptor parcelFileDescriptor =
                    service.streamLog(currentTime, PACKAGE_NAME);
            Assert.assertTrue(parcelFileDescriptor.getFileDescriptor().valid());
            try {
                parcelFileDescriptor.close();
            } catch (IOException e) {
                Log.e(TAG, e.getMessage(), e);
            }
        }

        File directory = AwNetLogService.getNetLogFileDirectory();
        Assert.assertEquals(1, directory.listFiles().length);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add(AwSwitches.NET_LOG)
    public void testExpiredFilesDeleted() throws Throwable {
        File directory = AwNetLogService.getNetLogFileDirectory();
        Assert.assertEquals(0, directory.listFiles().length);
        final long expiredTime = 100000L;
        Intent intent = new Intent(ContextUtils.getApplicationContext(), AwNetLogService.class);
        try (ServiceConnectionHelper helper =
                new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            INetLogService service = INetLogService.Stub.asInterface(helper.getBinder());
            ParcelFileDescriptor parcelFileDescriptor =
                    service.streamLog(expiredTime, PACKAGE_NAME);
            Assert.assertTrue(parcelFileDescriptor.getFileDescriptor().valid());
            try {
                parcelFileDescriptor.close();
            } catch (IOException e) {
                Log.e(TAG, e.getMessage(), e);
            }
        }
        Assert.assertEquals(1, directory.listFiles().length);

        final long currentTime = System.currentTimeMillis();
        try (ServiceConnectionHelper helper =
                new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            INetLogService service = INetLogService.Stub.asInterface(helper.getBinder());
            ParcelFileDescriptor parcelFileDescriptor =
                    service.streamLog(currentTime, PACKAGE_NAME);
            Assert.assertTrue(parcelFileDescriptor.getFileDescriptor().valid());
            try {
                parcelFileDescriptor.close();
            } catch (IOException e) {
                Log.e(TAG, e.getMessage(), e);
            }
        }
        Assert.assertEquals(1, directory.listFiles().length);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add(AwSwitches.NET_LOG)
    public void testFileContainsCorrectTime() throws Throwable {
        final long currentTime = System.currentTimeMillis();
        Intent intent = new Intent(ContextUtils.getApplicationContext(), AwNetLogService.class);
        try (ServiceConnectionHelper helper =
                new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            INetLogService service = INetLogService.Stub.asInterface(helper.getBinder());
            ParcelFileDescriptor parcelFileDescriptor =
                    service.streamLog(currentTime, PACKAGE_NAME);
            Assert.assertTrue(parcelFileDescriptor.getFileDescriptor().valid());
            try {
                parcelFileDescriptor.close();
            } catch (IOException e) {
                Log.e(TAG, e.getMessage(), e);
            }
        }

        File directory = AwNetLogService.getNetLogFileDirectory();
        File[] files = directory.listFiles();

        Assert.assertEquals(1, files.length);

        String fileName = files[0].getName();
        long fileTime = AwNetLogService.getCreationTimeFromFileName(fileName);

        Assert.assertEquals(currentTime, fileTime);
    }
}
