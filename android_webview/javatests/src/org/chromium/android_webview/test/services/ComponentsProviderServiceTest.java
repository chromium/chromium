// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import android.app.job.JobScheduler;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.os.ResultReceiver;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.experimental.runners.Enclosed;
import org.junit.runner.RunWith;

import org.chromium.android_webview.services.ComponentsProviderService;
import org.chromium.android_webview.test.AwActivityTestRule;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.component_updater.IComponentsProviderService;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link ComponentsProviderService}.
 *
 * <p>This class has two static subclasses with tests: {@link AutoBindServiceTests} and
 * {@link ServiceOnCreateTests}. These are testing different things (the former, functionality of
 * service methods; the latter, service's onCreate), but because they share some code, they live in
 * the same file.
 */
@RunWith(Enclosed.class)
public class ComponentsProviderServiceTest {
    private static final String TEST_FILE_NAME = "%s_%s_%s_testfile.tmp";
    private static final File sDirectory =
            new File(PathUtils.getDataDirectory(), "components/cps/");

    private static void cleanupFiles() {
        Assert.assertTrue("Failed to delete " + sDirectory.getAbsolutePath(),
                FileUtils.recursivelyDeleteFile(sDirectory, null));
    }

    /**
     * This subclass groups tests that bind the service, unlike tests that manually create the
     * service object (see {@link ServiceOnCreateTests}). These are not batched per class so the
     * service is unbound and killed, and the process is restarted between tests.
     */
    @RunWith(AwJUnit4ClassRunner.class)
    public static class AutoBindServiceTests {
        private ServiceConnectionHelper mConnection;
        private IComponentsProviderService mService;

        @Before
        public void setup() {
            Context context = ContextUtils.getApplicationContext();
            mConnection = new ServiceConnectionHelper(
                    new Intent(context, ComponentsProviderService.class), Context.BIND_AUTO_CREATE);
            mService = IComponentsProviderService.Stub.asInterface(mConnection.getBinder());
        }

        @After
        public void tearDown() {
            mConnection.close();
            cleanupFiles();
        }

        @Test
        @SmallTest
        public void testInvalidComponent() throws Exception {
            final String componentId = "someInvalidComponentId";
            final Bundle resultBundle = getFilesForComponentSync(componentId);
            Assert.assertNull(componentId + " should return a null result Bundle", resultBundle);
        }

        @Test
        @SmallTest
        public void testValidComponent() throws Exception {
            final String componentId = "testComponentA";
            final String sequenceNumber = "1";
            final String version = "2.3.4";
            createComponentFiles(componentId, sequenceNumber, version);

            final Bundle resultBundle = getFilesForComponentSync(componentId);
            assertBundleForValidComponent(resultBundle, componentId, sequenceNumber, version);
        }

        @Test
        @SmallTest
        public void testMultipleVersions() throws Exception {
            final String componentId = "testComponentB";
            final String sequenceNumber1 = "1";
            final String sequenceNumber2 = "2";
            final String version1 = "10.2.1";
            final String version2 = "11.0.4";

            // Version 1
            createComponentFiles(componentId, sequenceNumber1, version1);
            final Bundle resultBundle1 = getFilesForComponentSync(componentId);
            assertBundleForValidComponent(resultBundle1, componentId, sequenceNumber1, version1);

            // Version 2
            createComponentFiles(componentId, sequenceNumber2, version2);
            final Bundle resultBundle2 = getFilesForComponentSync(componentId);
            assertBundleForValidComponent(resultBundle2, componentId, sequenceNumber2, version2);
        }

        private Bundle getFilesForComponentSync(String componentId) throws Exception {
            final CountDownLatch latch = new CountDownLatch(1);
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
            Assert.assertTrue("Timeout waiting to receive files for component " + componentId,
                    latch.await(AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));

            return result.isEmpty() ? null : result;
        }

        private void assertBundleForValidComponent(
                Bundle bundle, String componentId, String sequenceNumber, String version) {
            Assert.assertNotNull(componentId + " should not return a null result Bundle", bundle);
            Assert.assertFalse(
                    componentId + " should not return an empty result Bundle", bundle.isEmpty());
            final HashMap<String, ParcelFileDescriptor> map =
                    (HashMap<String, ParcelFileDescriptor>) bundle.getSerializable(
                            ComponentsProviderService.KEY_RESULT);
            Assert.assertNotNull("Map in the result bundle should not be null", map);
            Assert.assertFalse("Map in the result bundle should not be empty", map.isEmpty());

            final String fileName = getComponentTestFileName(componentId, sequenceNumber, version);
            for (Map.Entry<String, ParcelFileDescriptor> entry : map.entrySet()) {
                final String key = entry.getKey();
                if (key.equals(fileName)) {
                    final ParcelFileDescriptor fileDescriptor = entry.getValue();
                    Assert.assertTrue("Null file descriptor for " + key,
                            fileDescriptor != null && fileDescriptor.getFileDescriptor() != null);
                    Assert.assertTrue("Invalid file descriptor for " + key,
                            fileDescriptor.getFileDescriptor().valid());
                    return;
                }
            }

            Assert.fail("Map in the result bundle did not contain test file " + TEST_FILE_NAME);
        }
    }

    /**
     * This subclass groups tests that manually create the service object, unlike tests that
     * bind the service (see {@link AutoBindServiceTests}). Since these tests don't rely on
     * binding the service, they can be ran as unit tests.
     */
    @RunWith(AwJUnit4ClassRunner.class)
    @Batch(Batch.UNIT_TESTS)
    public static class ServiceOnCreateTests {
        private final ComponentsProviderService mService = new ComponentsProviderService();

        @After
        public void tearDown() {
            cleanupFiles();
        }

        @Test
        @SmallTest
        public void testOnCreateCreatesDirectory() throws Exception {
            Assert.assertTrue("Failed to remove directory " + sDirectory.getAbsolutePath(),
                    !sDirectory.exists() || sDirectory.delete());

            mService.onCreate();

            Assert.assertTrue("Service didn't create directory " + sDirectory.getAbsolutePath(),
                    sDirectory.exists());
        }

        @Test
        @SmallTest
        public void testOnCreateSchedulesUpdater() throws Exception {
            JobScheduler jobScheduler =
                    (JobScheduler) ContextUtils.getApplicationContext().getSystemService(
                            Context.JOB_SCHEDULER_SERVICE);
            jobScheduler.cancelAll();

            mService.onCreate();

            Assert.assertTrue("Service should schedule updater job",
                    ComponentsProviderService.isJobScheduled(
                            jobScheduler, TaskIds.WEBVIEW_COMPONENT_UPDATE_JOB_ID));
        }

        @Test
        @SmallTest
        public void testOnCreateDeletesOlderVersions() throws Exception {
            final String componentId = "component001";

            // Create older versions.
            createComponentFiles(componentId, "3", "1.0.1");
            createComponentFiles(componentId, "4", "1.0.2");

            // Create newest version.
            final String sequenceNumber = "5";
            final String version = "1.1.0";
            createComponentFiles(componentId, sequenceNumber, version);

            mService.onCreate();
            AwActivityTestRule.waitForFuture(mService.getDeleteTaskForTesting());

            // Check onCreate deleted older versions.
            File component = new File(sDirectory, componentId + "/");
            File[] files = component.listFiles();
            Assert.assertNotNull(
                    componentId + " has no installed versions, but should have 1", files);
            Assert.assertEquals(
                    componentId + " has " + files.length + " installed versions, but should have 1",
                    /* expected = */ 1, /* actual = */ files.length);
            Assert.assertEquals("Wrong sequence/version number for component " + componentId,
                    /* expected = */ sequenceNumber + "_" + version,
                    /* actual = */ files[0].getName());
        }
    }

    private static void createComponentFiles(
            String componentId, String sequenceNumber, String version) throws IOException {
        final File versionDirectory =
                new File(sDirectory, componentId + "/" + sequenceNumber + "_" + version);
        Assert.assertTrue("Failed to create directory " + versionDirectory.getAbsolutePath(),
                versionDirectory.mkdirs());

        final File file = new File(
                versionDirectory, getComponentTestFileName(componentId, sequenceNumber, version));
        Assert.assertTrue("Failed to create file " + file.getAbsolutePath(), file.createNewFile());

        FileWriter writer = new FileWriter(file);
        for (int i = 0; i < 100; i++) {
            writer.write("Adding some data to file...\n");
        }
        writer.close();

        Assert.assertTrue("File " + file.getName() + " should not have size 0",
                FileUtils.getFileSizeBytes(file) > 0);
    }

    private static String getComponentTestFileName(
            String componentId, String sequenceNumber, String version) {
        return String.format(TEST_FILE_NAME, componentId, sequenceNumber, version);
    }
}
