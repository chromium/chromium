// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.os.ResultReceiver;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.services.ComponentsProviderService;
import org.chromium.android_webview.test.AwActivityTestRule;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.components.component_updater.IComponentsProviderService;

import java.io.File;
import java.util.HashMap;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests for {@link ComponentsProviderService}. These are not batched per class so the service is
 * unbound and killed, and the process is restarted between tests.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class ComponentsProviderServiceTest {
    private ServiceConnectionHelper mConnection;
    private IComponentsProviderService mService;
    private Context mContext;
    private File mTempDirectory;

    @Before
    public void setUp() throws TimeoutException {
        mContext = ContextUtils.getApplicationContext();
        mTempDirectory = new File(mContext.getFilesDir(), "tmp/");
        Assert.assertTrue(mTempDirectory.exists() || mTempDirectory.mkdirs());

        mConnection = new ServiceConnectionHelper(
                new Intent(mContext, ComponentsProviderService.class), Context.BIND_AUTO_CREATE);
        mService = IComponentsProviderService.Stub.asInterface(mConnection.getBinder());
    }

    @After
    public void tearDown() {
        mConnection.close();
        Assert.assertTrue("Failed to cleanup temporary test files",
                FileUtils.recursivelyDeleteFile(mTempDirectory, null));
        Assert.assertTrue("Failed to cleanup cps test files",
                FileUtils.recursivelyDeleteFile(
                        new File(mContext.getFilesDir(), "components/cps/"), null));
    }

    @Test
    @SmallTest
    public void testInvalidComponentId() throws Exception {
        Assert.assertNull("Result bundle for an invalid componentId should be null",
                getFilesForComponentSync("anInvalidComponentId"));
    }

    @Test
    @MediumTest
    public void testValidComponentId() throws Exception {
        final String randomDirectoryName = "jaAFih32";
        final String componentId = "testComponentA";
        final String version = "1.0.0";

        File directory = new File(mTempDirectory, randomDirectoryName + "/");
        Assert.assertTrue(directory.exists() || directory.mkdirs());
        File file = new File(directory, "file.test");
        Assert.assertTrue(file.exists() || file.createNewFile());

        Assert.assertTrue(mService.onNewVersion(componentId, directory.getAbsolutePath(), version));

        Bundle resultData = getFilesForComponentSync(componentId);

        Assert.assertNotNull(
                "Bundle resultData for componentId " + componentId + " should not be null",
                resultData);
        HashMap<String, ParcelFileDescriptor> map =
                (HashMap<String, ParcelFileDescriptor>) resultData.getSerializable(
                        ComponentsProviderService.KEY_RESULT);
        Assert.assertNotNull(map);
        Assert.assertEquals(1, map.size());
        Assert.assertTrue(map.containsKey(file.getName()));
        ParcelFileDescriptor fileDescriptor = map.get(file.getName());
        Assert.assertTrue(fileDescriptor.getFileDescriptor().valid());
        fileDescriptor.close();
    }

    @Test
    @SmallTest
    public void testOnNewVersion() throws Exception {
        final String randomDirectoryName = "lJna65aF";
        final String componentId = "testComponentB";
        final String version = "2.0.0";

        File directory = new File(mTempDirectory, randomDirectoryName + "/");
        Assert.assertTrue(directory.exists() || directory.mkdirs());
        File file = new File(directory, "file.test");
        Assert.assertTrue(file.exists() || file.createNewFile());

        Assert.assertTrue(mService.onNewVersion(componentId, directory.getAbsolutePath(), version));
        Assert.assertFalse(file.exists());
    }

    private Bundle getFilesForComponentSync(String componentId) throws Exception {
        CountDownLatch latch = new CountDownLatch(1);
        final Bundle result = new Bundle();
        mService.getFilesForComponent(componentId, new ResultReceiver(null) {
            @Override
            protected void onReceiveResult(int resultCode, Bundle resultData) {
                if (resultData != null) {
                    result.putAll(resultData);
                }
                latch.countDown();
            }
        });
        Assert.assertTrue("Timeout waiting to receive result from getFilesForComponent",
                latch.await(AwActivityTestRule.WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));

        return result.isEmpty() ? null : result;
    }
}
