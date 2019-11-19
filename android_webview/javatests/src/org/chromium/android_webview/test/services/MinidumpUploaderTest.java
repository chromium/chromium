// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.os.ParcelFileDescriptor;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.crash.SystemWideCrashDirectories;
import org.chromium.android_webview.services.AwMinidumpUploaderDelegate;
import org.chromium.android_webview.services.CrashReceiverService;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.Callback;
import org.chromium.base.FileUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.components.minidump_uploader.CrashTestRule;
import org.chromium.components.minidump_uploader.CrashTestRule.MockCrashReportingPermissionManager;
import org.chromium.components.minidump_uploader.MinidumpUploadTestUtility;
import org.chromium.components.minidump_uploader.MinidumpUploader;
import org.chromium.components.minidump_uploader.MinidumpUploaderDelegate;
import org.chromium.components.minidump_uploader.MinidumpUploaderImpl;
import org.chromium.components.minidump_uploader.TestMinidumpUploaderImpl;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;

/**
 * Instrumentation tests for WebView's implementation of MinidumpUploaderDelegate, and the
 * interoperability of WebView's minidump-copying and minidump-uploading logic.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS)
public class MinidumpUploaderTest {
    @Rule
    public CrashTestRule mTestRule = new CrashTestRule() {
        @Override
        public File getExistingCacheDir() {
            return SystemWideCrashDirectories.getOrCreateWebViewCrashDir();
        }
    };

    private static final String BOUNDARY = "TESTBOUNDARY";

    private static class TestPlatformServiceBridge extends PlatformServiceBridge {
        private final boolean mEnabled;

        public TestPlatformServiceBridge(boolean enabled) {
            mEnabled = enabled;
        }

        @Override
        public boolean canUseGms() {
            return true;
        }

        @Override
        public void queryMetricsSetting(Callback<Boolean> callback) {
            ThreadUtils.assertOnUiThread();
            callback.onResult(mEnabled);
        }
    }

    /**
     * Ensure MinidumpUploaderImpl doesn't crash even if the WebView Crash dir doesn't exist (could
     * happen e.g. if a Job persists across WebView-updates?
     *
     * MinidumpUploaderImpl should automatically recreate the directory.
     */
    @Test
    @MediumTest
    public void testUploadingWithoutCrashDir() {
        File webviewCrashDir = mTestRule.getExistingCacheDir();
        // Delete the WebView crash directory to ensure MinidumpUploader doesn't crash without it.
        FileUtils.recursivelyDeleteFile(webviewCrashDir);
        Assert.assertFalse(webviewCrashDir.exists());

        PlatformServiceBridge.injectInstance(new TestPlatformServiceBridge(true));
        final CrashReportingPermissionManager permManager =
                new MockCrashReportingPermissionManager() {
                    { mIsEnabledForTests = true; }
                };
        MinidumpUploader minidumpUploader =
                // Use AwMinidumpUploaderDelegate instead of TestMinidumpUploaderDelegate here
                // since AwMinidumpUploaderDelegate defines the WebView crash directory.
                new TestMinidumpUploaderImpl(new AwMinidumpUploaderDelegate() {
                    @Override
                    public CrashReportingPermissionManager createCrashReportingPermissionManager() {
                        return permManager;
                    }
                });

        // Ensure that we don't crash when trying to upload minidumps without a crash directory.
        MinidumpUploadTestUtility.uploadMinidumpsSync(
                minidumpUploader, false /* expectReschedule */);
    }

    /**
     * Ensures that the minidump copying works together with the minidump uploading.
     */
    @Test
    @MediumTest
    public void testCopyAndUploadWebViewMinidump() throws IOException {
        final CrashFileManager fileManager =
                new CrashFileManager(SystemWideCrashDirectories.getWebViewCrashDir());
        // Note that these minidump files are set up directly in the cache dir - not in the WebView
        // crash dir. This is to ensure the CrashFileManager doesn't see these minidumps without us
        // first copying them.
        File minidumpToCopy = new File(mTestRule.getExistingCacheDir(), "toCopy.dmp.try0");
        CrashTestRule.setUpMinidumpFile(minidumpToCopy, BOUNDARY, "browser");
        final String expectedFileContent = readEntireFile(minidumpToCopy);

        File[] uploadedFiles = copyAndUploadMinidumpsSync(
                fileManager, new File[][] {{minidumpToCopy}}, new int[] {0});

        // CrashReceiverService will rename the minidumps to some globally unique file name
        // meaning that we have to check the contents of the minidump rather than the file
        // name.
        try {
            Assert.assertEquals(expectedFileContent, readEntireFile(uploadedFiles[0]));
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
        File webviewTmpDir = SystemWideCrashDirectories.getWebViewTmpCrashDir();
        Assert.assertEquals(0, webviewTmpDir.listFiles().length);
    }

    /**
     * Ensure that when PlatformServiceBridge returns true we do upload minidumps.
     */
    @Test
    @MediumTest
    public void testPlatformServicesBridgeIsUsedUserConsent() throws IOException {
        testPlatformServicesBridgeIsUsed(true);
    }

    /**
     * Ensure that when PlatformServiceBridge returns false we do not upload minidumps.
     */
    @Test
    @MediumTest
    public void testPlatformServicesBridgeIsUsedNoUserConsent() throws IOException {
        testPlatformServicesBridgeIsUsed(false);
    }

    /**
     * MinidumpUploaderDelegate sub-class that uses MinidumpUploaderDelegate's implementation of
     * CrashReportingPermissionManager.isUsageAndCrashReportingPermittedByUser().
     */
    private static class WebViewUserConsentMinidumpUploaderDelegate
            extends AwMinidumpUploaderDelegate {
        private final boolean mUserConsent;
        WebViewUserConsentMinidumpUploaderDelegate(boolean userConsent) {
            super();
            mUserConsent = userConsent;
        }
        @Override
        public CrashReportingPermissionManager createCrashReportingPermissionManager() {
            final CrashReportingPermissionManager realPermissionManager =
                    super.createCrashReportingPermissionManager();
            return new MockCrashReportingPermissionManager() {
                {
                    // This setup ensures we depend on
                    // isUsageAndCrashReportingPermittedByUser().
                    mIsInSample = true;
                    mIsNetworkAvailable = true;
                    mIsEnabledForTests = false;
                }
                @Override
                public boolean isUsageAndCrashReportingPermittedByUser() {
                    // Ensure that we use the real implementation of
                    // isUsageAndCrashReportingPermittedByUser.
                    boolean userPermitted =
                            realPermissionManager.isUsageAndCrashReportingPermittedByUser();
                    Assert.assertEquals(mUserConsent, userPermitted);
                    return userPermitted;
                }
            };
        }
    }

    private void testPlatformServicesBridgeIsUsed(final boolean userConsent) throws IOException {
        PlatformServiceBridge.injectInstance(new TestPlatformServiceBridge(userConsent));
        MinidumpUploaderDelegate delegate =
                new WebViewUserConsentMinidumpUploaderDelegate(userConsent);
        MinidumpUploader minidumpUploader = new TestMinidumpUploaderImpl(delegate);

        File firstFile = createMinidumpFileInCrashDir("1_abc.dmp0.try0");
        File secondFile = createMinidumpFileInCrashDir("12_abcd.dmp0.try0");
        File expectedFirstFile = new File(mTestRule.getCrashDir(),
                firstFile.getName().replace(".dmp", userConsent ? ".up" : ".skipped"));
        File expectedSecondFile = new File(mTestRule.getCrashDir(),
                secondFile.getName().replace(".dmp", userConsent ? ".up" : ".skipped"));

        MinidumpUploadTestUtility.uploadMinidumpsSync(
                minidumpUploader, false /* expectReschedule */);

        Assert.assertFalse(firstFile.exists());
        Assert.assertTrue(expectedFirstFile.exists());
        Assert.assertFalse(secondFile.exists());
        Assert.assertTrue(expectedSecondFile.exists());
    }

    private static String readEntireFile(File file) throws IOException {
        try (FileInputStream fileInputStream = new FileInputStream(file)) {
            byte[] data = new byte[(int) file.length()];
            fileInputStream.read(data);
            return new String(data);
        }
    }

    /**
     * Ensure we can copy and upload several batches of files (i.e. emulate several copying-calls in
     * a row without the copying-service being destroyed in between).
     */
    @Test
    @MediumTest
    public void testCopyAndUploadSeveralMinidumpBatches() throws IOException {
        final CrashFileManager fileManager =
                new CrashFileManager(SystemWideCrashDirectories.getWebViewCrashDir());
        // Note that these minidump files are set up directly in the cache dir - not in the WebView
        // crash dir. This is to ensure the CrashFileManager doesn't see these minidumps without us
        // first copying them.
        File firstMinidumpToCopy =
                new File(mTestRule.getExistingCacheDir(), "firstToCopy.dmp.try0");
        File secondMinidumpToCopy =
                new File(mTestRule.getExistingCacheDir(), "secondToCopy.dmp.try0");
        CrashTestRule.setUpMinidumpFile(firstMinidumpToCopy, BOUNDARY, "browser");
        CrashTestRule.setUpMinidumpFile(secondMinidumpToCopy, BOUNDARY, "renderer");
        final String expectedFirstFileContent = readEntireFile(firstMinidumpToCopy);
        final String expectedSecondFileContent = readEntireFile(secondMinidumpToCopy);

        File[] uploadedFiles = copyAndUploadMinidumpsSync(fileManager,
                new File[][] {{firstMinidumpToCopy}, {secondMinidumpToCopy}}, new int[] {0, 0});

        // CrashReceiverService will rename the minidumps to some globally unique file name
        // meaning that we have to check the contents of the minidump rather than the file
        // name.
        try {
            final String actualFileContent0 = readEntireFile(uploadedFiles[0]);
            final String actualFileContent1 = readEntireFile(uploadedFiles[1]);
            if (expectedFirstFileContent.equals(actualFileContent0)) {
                Assert.assertEquals(expectedSecondFileContent, actualFileContent1);
            } else {
                Assert.assertEquals(expectedFirstFileContent, actualFileContent1);
                Assert.assertEquals(expectedSecondFileContent, actualFileContent0);
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Copy and upload {@param minidumps} by one array at a time - i.e. the minidumps in a single
     * array in {@param minidumps} will all be copied in the same call into CrashReceiverService.
     * @param fileManager the CrashFileManager to use when copying/renaming minidumps.
     * @param minidumps an array of arrays of minidumps to copy and upload, by copying one array at
     * a time.
     * @param uids an array of uids declaring the uids used when calling into CrashReceiverService.
     * @return the uploaded files.
     */
    private File[] copyAndUploadMinidumpsSync(CrashFileManager fileManager, File[][] minidumps,
            int[] uids) throws FileNotFoundException {
        CrashReceiverService crashReceiverService = new CrashReceiverService();
        Assert.assertEquals(minidumps.length, uids.length);
        // Ensure the upload service minidump directory is empty before we start copying files.
        File[] initialMinidumps = fileManager.getMinidumpsReadyForUpload(
                MinidumpUploaderImpl.MAX_UPLOAD_TRIES_ALLOWED);
        Assert.assertEquals(0, initialMinidumps.length);

        // Open file descriptors to the files and then delete the files.
        ParcelFileDescriptor[][] fileDescriptors = new ParcelFileDescriptor[minidumps.length][];
        int numMinidumps = 0;
        for (int n = 0; n < minidumps.length; n++) {
            File[] currentMinidumps = minidumps[n];
            numMinidumps += currentMinidumps.length;
            fileDescriptors[n] = new ParcelFileDescriptor[currentMinidumps.length];
            for (int m = 0; m < currentMinidumps.length; m++) {
                fileDescriptors[n][m] = ParcelFileDescriptor.open(
                        currentMinidumps[m], ParcelFileDescriptor.MODE_READ_ONLY);
                Assert.assertTrue(currentMinidumps[m].delete());
            }
            crashReceiverService.performMinidumpCopyingSerially(
                    uids[n] /* uid */, fileDescriptors[n], null, false /* scheduleUploads */);
        }

        final CrashReportingPermissionManager permManager =
                new MockCrashReportingPermissionManager() {
                    { mIsEnabledForTests = true; }
                };
        MinidumpUploader minidumpUploader =
                // Use AwMinidumpUploaderDelegate instead of TestMinidumpUploaderDelegate to ensure
                // AwMinidumpUploaderDelegate works well together with the minidump-copying methods
                // of CrashReceiverService.
                new TestMinidumpUploaderImpl(new AwMinidumpUploaderDelegate() {
                    @Override
                    public CrashReportingPermissionManager createCrashReportingPermissionManager() {
                        return permManager;
                    }
                });

        MinidumpUploadTestUtility.uploadMinidumpsSync(
                minidumpUploader, false /* expectReschedule */);
        // Ensure there are no minidumps left to upload.
        File[] nonUploadedMinidumps = fileManager.getMinidumpsReadyForUpload(
                MinidumpUploaderImpl.MAX_UPLOAD_TRIES_ALLOWED);
        Assert.assertEquals(0, nonUploadedMinidumps.length);

        File[] uploadedFiles = fileManager.getAllUploadedFiles();
        Assert.assertEquals(numMinidumps, uploadedFiles.length);
        return uploadedFiles;
    }

    private File createMinidumpFileInCrashDir(String name) throws IOException {
        File minidumpFile = new File(mTestRule.getCrashDir(), name);
        CrashTestRule.setUpMinidumpFile(minidumpFile, BOUNDARY);
        return minidumpFile;
    }
}
