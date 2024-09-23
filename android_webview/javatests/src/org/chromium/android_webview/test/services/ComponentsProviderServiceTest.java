// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.app.job.JobScheduler;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.os.ResultReceiver;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.experimental.runners.Enclosed;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.SafeModeController;
import org.chromium.android_webview.common.services.ISafeModeService;
import org.chromium.android_webview.services.ComponentUpdaterResetSafeModeAction;
import org.chromium.android_webview.services.ComponentsProviderService;
import org.chromium.android_webview.services.SafeModeService;
import org.chromium.android_webview.test.AwActivityTestRule;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.android_webview.variations.VariationsSeedSafeModeAction;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.component_updater.IComponentsProviderService;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
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
    public static final String TEST_WEBVIEW_PACKAGE_NAME = "org.chromium.android_webview.shell";
    private static final String TEST_FILE_NAME = "%s_%s_%s_testfile.tmp";
    private static final File sDirectory =
            new File(PathUtils.getDataDirectory(), "components/cps/");

    private static void cleanupFiles() {
        Assert.assertTrue(
                "Failed to delete " + sDirectory.getAbsolutePath(),
                FileUtils.recursivelyDeleteFile(sDirectory, null));
    }

    /**
     * This subclass groups tests that bind the service, unlike tests that manually create the
     * service object (see {@link ServiceOnCreateTests}). These are not batched per class so the
     * service is unbound and killed, and the process is restarted between tests.
     */
    @RunWith(AwJUnit4ClassRunner.class)
    @OnlyRunIn(EITHER_PROCESS) // These tests don't use the renderer process
    public static class AutoBindServiceTests {
        private ServiceConnectionHelper mConnection;
        private IComponentsProviderService mService;

        @Before
        public void setup() {
            Context context = ContextUtils.getApplicationContext();
            mConnection =
                    new ServiceConnectionHelper(
                            new Intent(context, ComponentsProviderService.class),
                            Context.BIND_AUTO_CREATE);
            mService = IComponentsProviderService.Stub.asInterface(mConnection.getBinder());
        }

        @After
        public void tearDown() {
            // Reset component state back to the default.
            final Context context = ContextUtils.getApplicationContext();
            ComponentName safeModeComponent =
                    new ComponentName(
                            TEST_WEBVIEW_PACKAGE_NAME,
                            SafeModeController.SAFE_MODE_STATE_COMPONENT);
            context.getPackageManager()
                    .setComponentEnabledSetting(
                            safeModeComponent,
                            PackageManager.COMPONENT_ENABLED_STATE_DEFAULT,
                            PackageManager.DONT_KILL_APP);

            SafeModeController.getInstance().unregisterActionsForTesting();

            mConnection.close();
            cleanupFiles();
            SafeModeService.clearSharedPrefsForTesting();
            ComponentsProviderService.clearSharedPrefsForTesting();
        }

        @Test
        @SmallTest
        @Feature({"AndroidWebView"})
        public void testInvalidComponent() throws Exception {
            final String componentId = "someInvalidComponentId";
            final Bundle resultBundle = getFilesForComponentSync(componentId);
            Assert.assertNull(componentId + " should return a null result Bundle", resultBundle);
        }

        @Test
        @SmallTest
        @Feature({"AndroidWebView"})
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
        @Feature({"AndroidWebView"})
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

        @Test
        @MediumTest
        @Feature({"AndroidWebView"})
        public void testComponentUpdaterReset() throws Throwable {
            final String componentId = "testComponentA";
            final String sequenceNumber = "1";
            final String version = "2.3.4";
            createComponentFiles(componentId, sequenceNumber, version);
            Bundle resultBundle = getFilesForComponentSync(componentId);
            assertBundleForValidComponent(resultBundle, componentId, sequenceNumber, version);

            Intent intent = new Intent(ContextUtils.getApplicationContext(), SafeModeService.class);
            final String componentUpdaterResetActionId =
                    new ComponentUpdaterResetSafeModeAction().getId();
            try (ServiceConnectionHelper helper =
                    new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
                ISafeModeService service = ISafeModeService.Stub.asInterface(helper.getBinder());
                service.setSafeMode(Arrays.asList(componentUpdaterResetActionId));
            }

            Assert.assertTrue(
                    "SafeMode should be enabled",
                    SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
            Set<String> actions = new HashSet<>();
            actions.add(componentUpdaterResetActionId);
            Assert.assertEquals(
                    "Querying the ContentProvider should yield the action we set",
                    actions,
                    SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));

            resultBundle = getFilesForComponentSync(componentId);
            Assert.assertNull(
                    componentId
                            + " must return a null result Bundle while ComponentUpdaterReset is on",
                    resultBundle);
        }

        @Test
        @MediumTest
        @Feature({"AndroidWebView"})
        public void testCPSIgnoresIrrelevantSafeModeAction() throws Throwable {
            final String componentId = "testComponentA";
            final String sequenceNumber = "1";
            final String version = "2.3.4";
            createComponentFiles(componentId, sequenceNumber, version);
            Bundle resultBundle = getFilesForComponentSync(componentId);
            assertBundleForValidComponent(resultBundle, componentId, sequenceNumber, version);

            Intent intent = new Intent(ContextUtils.getApplicationContext(), SafeModeService.class);
            final String variationsSeedSafeModeActionId =
                    new VariationsSeedSafeModeAction().getId();

            try (ServiceConnectionHelper helper =
                    new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
                ISafeModeService service = ISafeModeService.Stub.asInterface(helper.getBinder());
                service.setSafeMode(Arrays.asList(variationsSeedSafeModeActionId));
            }

            Assert.assertTrue(
                    "SafeMode should be enabled",
                    SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
            Set<String> actions = new HashSet<>();
            actions.add(variationsSeedSafeModeActionId);
            Assert.assertEquals(
                    "Querying the ContentProvider should yield the action we set",
                    actions,
                    SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));

            resultBundle = getFilesForComponentSync(componentId);
            Assert.assertNotNull(
                    componentId
                            + " must return a non-null Bundle while ComponentUpdaterReset is off",
                    resultBundle);
        }

        private Bundle getFilesForComponentSync(String componentId) throws Exception {
            final CountDownLatch latch = new CountDownLatch(1);
            final Bundle result = new Bundle();
            mService.getFilesForComponent(
                    componentId,
                    new ResultReceiver(null) {
                        @Override
                        protected void onReceiveResult(int resultCode, Bundle resultData) {
                            if (resultData != null) {
                                result.putAll(resultData);
                            }
                            latch.countDown();
                        }
                    });
            Assert.assertTrue(
                    "Timeout waiting to receive files for component " + componentId,
                    latch.await(AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));

            return result.isEmpty() ? null : result;
        }

        private void assertBundleForValidComponent(
                Bundle bundle, String componentId, String sequenceNumber, String version) {
            Assert.assertNotNull(componentId + " should not return a null result Bundle", bundle);
            Assert.assertFalse(
                    componentId + " should not return an empty result Bundle", bundle.isEmpty());
            final HashMap<String, ParcelFileDescriptor> map =
                    (HashMap<String, ParcelFileDescriptor>)
                            bundle.getSerializable(ComponentsProviderService.KEY_RESULT);
            Assert.assertNotNull("Map in the result bundle should not be null", map);
            Assert.assertFalse("Map in the result bundle should not be empty", map.isEmpty());

            final String fileName = getComponentTestFileName(componentId, sequenceNumber, version);
            for (Map.Entry<String, ParcelFileDescriptor> entry : map.entrySet()) {
                final String key = entry.getKey();
                if (key.equals(fileName)) {
                    final ParcelFileDescriptor fileDescriptor = entry.getValue();
                    Assert.assertTrue(
                            "Null file descriptor for " + key,
                            fileDescriptor != null && fileDescriptor.getFileDescriptor() != null);
                    Assert.assertTrue(
                            "Invalid file descriptor for " + key,
                            fileDescriptor.getFileDescriptor().valid());
                    return;
                }
            }

            Assert.fail("Map in the result bundle did not contain test file " + TEST_FILE_NAME);
        }
    }

    /**
     * This subclass groups tests that manually create the service object, unlike tests that bind
     * the service (see {@link AutoBindServiceTests}). Since these tests don't rely on binding the
     * service, they can be ran as unit tests.
     */
    @RunWith(AwJUnit4ClassRunner.class)
    @OnlyRunIn(EITHER_PROCESS) // These are unit tests
    @Batch(Batch.UNIT_TESTS)
    public static class ServiceOnCreateTests {
        private final ComponentsProviderService mService = new ComponentsProviderService();

        @After
        public void tearDown() {
            cleanupFiles();
            SafeModeService.clearSharedPrefsForTesting();
            ComponentsProviderService.clearSharedPrefsForTesting();
        }

        @Test
        @SmallTest
        public void testOnCreateCreatesDirectory() throws Exception {
            Assert.assertTrue(
                    "Failed to remove directory " + sDirectory.getAbsolutePath(),
                    !sDirectory.exists() || sDirectory.delete());

            mService.onCreate();

            Assert.assertTrue(
                    "Service didn't create directory " + sDirectory.getAbsolutePath(),
                    sDirectory.exists());
        }

        @Test
        @SmallTest
        public void testOnCreateSchedulesUpdater() throws Exception {
            JobScheduler jobScheduler =
                    (JobScheduler)
                            ContextUtils.getApplicationContext()
                                    .getSystemService(Context.JOB_SCHEDULER_SERVICE);
            jobScheduler.cancelAll();

            mService.onCreate();
            Assert.assertTrue(
                    "Service should schedule updater job",
                    ComponentsProviderService.isJobScheduled(
                            jobScheduler, TaskIds.WEBVIEW_COMPONENT_UPDATE_JOB_ID));
        }

        @Test
        @SmallTest
        public void testScheduleUpdateJob() throws Exception {
            JobScheduler jobScheduler =
                    (JobScheduler)
                            ContextUtils.getApplicationContext()
                                    .getSystemService(Context.JOB_SCHEDULER_SERVICE);
            jobScheduler.cancelAll();

            long currentTime = System.currentTimeMillis();

            ComponentsProviderService.setClockForTesting(() -> currentTime);

            ComponentsProviderService.maybeScheduleComponentUpdateService();
            Assert.assertTrue(
                    "Service should schedule updater job",
                    ComponentsProviderService.isJobScheduled(
                            jobScheduler, TaskIds.WEBVIEW_COMPONENT_UPDATE_JOB_ID));

            jobScheduler.cancelAll();
            Assert.assertFalse(
                    "Updater job should be cancelled",
                    ComponentsProviderService.isJobScheduled(
                            jobScheduler, TaskIds.WEBVIEW_COMPONENT_UPDATE_JOB_ID));

            ComponentsProviderService.setClockForTesting(() -> currentTime + 1000L);
            ComponentsProviderService.maybeScheduleComponentUpdateService();
            Assert.assertFalse(
                    "Updater job shouldn't be scheduled before "
                            + ComponentsProviderService.UPDATE_INTERVAL_MS
                            + " milliseconds pass",
                    ComponentsProviderService.isJobScheduled(
                            jobScheduler, TaskIds.WEBVIEW_COMPONENT_UPDATE_JOB_ID));

            ComponentsProviderService.setClockForTesting(
                    () -> currentTime + ComponentsProviderService.UPDATE_INTERVAL_MS + 1000L);
            ComponentsProviderService.maybeScheduleComponentUpdateService();
            Assert.assertTrue(
                    "Updater job should be scheduled because "
                            + ComponentsProviderService.UPDATE_INTERVAL_MS
                            + " milliseconds passed",
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
                    /* expected= */ 1,
                    /* actual= */ files.length);
            Assert.assertEquals(
                    "Wrong sequence/version number for component " + componentId,
                    /* expected= */ sequenceNumber + "_" + version,
                    /* actual= */ files[0].getName());
        }

        @Test
        @MediumTest
        @Feature({"AndroidWebView"})
        public void testComponentUpdaterResetCancelsUpdaterJob() throws Throwable {
            final String componentUpdaterResetActionId =
                    new ComponentUpdaterResetSafeModeAction().getId();
            Intent intent = new Intent(ContextUtils.getApplicationContext(), SafeModeService.class);
            try (ServiceConnectionHelper helper =
                    new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
                ISafeModeService safeModeService =
                        ISafeModeService.Stub.asInterface(helper.getBinder());
                safeModeService.setSafeMode(Arrays.asList(componentUpdaterResetActionId));
            }

            JobScheduler jobScheduler =
                    (JobScheduler)
                            ContextUtils.getApplicationContext()
                                    .getSystemService(Context.JOB_SCHEDULER_SERVICE);
            mService.onCreate();
            Assert.assertFalse(
                    "Service should have no updater job scheduled",
                    ComponentsProviderService.isJobScheduled(
                            jobScheduler, TaskIds.WEBVIEW_COMPONENT_UPDATE_JOB_ID));
        }
    }

    private static void createComponentFiles(
            String componentId, String sequenceNumber, String version) throws IOException {
        final File versionDirectory =
                new File(sDirectory, componentId + "/" + sequenceNumber + "_" + version);
        Assert.assertTrue(
                "Failed to create directory " + versionDirectory.getAbsolutePath(),
                versionDirectory.mkdirs());

        final File file =
                new File(
                        versionDirectory,
                        getComponentTestFileName(componentId, sequenceNumber, version));
        Assert.assertTrue("Failed to create file " + file.getAbsolutePath(), file.createNewFile());

        FileWriter writer = new FileWriter(file);
        for (int i = 0; i < 100; i++) {
            writer.write("Adding some data to file...\n");
        }
        writer.close();

        Assert.assertTrue(
                "File " + file.getName() + " should not have size 0",
                FileUtils.getFileSizeBytes(file) > 0);
    }

    private static String getComponentTestFileName(
            String componentId, String sequenceNumber, String version) {
        return String.format(TEST_FILE_NAME, componentId, sequenceNumber, version);
    }
}
