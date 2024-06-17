// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;
import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.os.ParcelFileDescriptor;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.nonembedded.crash.SystemWideCrashDirectories;
import org.chromium.android_webview.services.AwMinidumpUploaderDelegate;
import org.chromium.android_webview.services.AwMinidumpUploaderDelegate.SamplingDelegate;
import org.chromium.android_webview.services.CrashReceiverService;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.Callback;
import org.chromium.base.FileUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.version_info.Channel;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.components.minidump_uploader.CrashTestRule;
import org.chromium.components.minidump_uploader.CrashTestRule.MockCrashReportingPermissionManager;
import org.chromium.components.minidump_uploader.MinidumpUploadJob;
import org.chromium.components.minidump_uploader.MinidumpUploadJobImpl;
import org.chromium.components.minidump_uploader.MinidumpUploaderDelegate;
import org.chromium.components.minidump_uploader.MinidumpUploaderTestConstants;
import org.chromium.components.minidump_uploader.TestMinidumpUploadJobImpl;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Instrumentation tests for WebView's implementation of MinidumpUploaderDelegate, and the
 * interoperability of WebView's minidump-copying and minidump-uploading logic.
 *
 * <p>These tests load the native library and mark the process as a browser process, so it's safer
 * to leave them unbatched to avoid possible state leaking between tests.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(EITHER_PROCESS) // These tests don't use the renderer process
@DoNotBatch(reason = "These tests load the native library so batching might leak state")
public class MinidumpUploadJobTest {
    @Rule
    public CrashTestRule mTestRule =
            new CrashTestRule() {
                @Override
                public File getExistingCacheDir() {
                    return SystemWideCrashDirectories.getOrCreateWebViewCrashDir();
                }
            };

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

    private static class TestSamplingDelegate implements SamplingDelegate {
        private final int mChannel;
        private final int mRandomSampling;

        TestSamplingDelegate(int channel, int randomSampling) {
            mChannel = channel;
            mRandomSampling = randomSampling;
        }

        @Override
        public int getChannel() {
            return mChannel;
        }

        @Override
        public int getRandomSample() {
            return mRandomSampling;
        }
    }

    @Before
    public void setUp() {
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_WEBVIEW);
        LibraryLoader.getInstance().ensureInitialized();
    }

    // randomSampl < CRASH_DUMP_PERCENTAGE_FOR_STABLE to always sample-in crashes.
    private static final SamplingDelegate TEST_SAMPLING_DELEGATE =
            new TestSamplingDelegate(Channel.UNKNOWN, 0);

    /**
     * Ensure MinidumpUploadJobImpl doesn't crash even if the WebView Crash dir doesn't exist (could
     * happen e.g. if a Job persists across WebView-updates?
     *
     * MinidumpUploadJobImpl should automatically recreate the directory.
     */
    @Test
    @MediumTest
    public void testUploadingWithoutCrashDir() {
        File webviewCrashDir = mTestRule.getExistingCacheDir();
        // Delete the WebView crash directory to ensure MinidumpUploadJob doesn't crash without it.
        FileUtils.recursivelyDeleteFile(webviewCrashDir, FileUtils.DELETE_ALL);
        Assert.assertFalse(webviewCrashDir.exists());

        PlatformServiceBridge.injectInstance(new TestPlatformServiceBridge(true));
        final CrashReportingPermissionManager permManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsEnabledForTests = true;
                    }
                };
        MinidumpUploadJob minidumpUploadJob =
                // Use AwMinidumpUploaderDelegate instead of TestMinidumpUploaderDelegate here
                // since AwMinidumpUploaderDelegate defines the WebView crash directory.
                new TestMinidumpUploadJobImpl(
                        new AwMinidumpUploaderDelegate(TEST_SAMPLING_DELEGATE) {
                            @Override
                            public CrashReportingPermissionManager
                                    createCrashReportingPermissionManager() {
                                return permManager;
                            }
                        });

        // Ensure that we don't crash when trying to upload minidumps without a crash directory.
        uploadMinidumpsSync(minidumpUploadJob, /* expectReschedule= */ false);
    }

    /** Ensures that the minidump copying works together with the minidump uploading. */
    @Test
    @MediumTest
    public void testCopyAndUploadWebViewMinidump() throws IOException {
        final CrashFileManager fileManager =
                new CrashFileManager(SystemWideCrashDirectories.getWebViewCrashDir());
        // Note that these minidump files are set up directly in the cache dir - not in the WebView
        // crash dir. This is to ensure the CrashFileManager doesn't see these minidumps without us
        // first copying them.
        File minidumpToCopy = new File(mTestRule.getExistingCacheDir(), "toCopy.dmp.try0");
        CrashTestRule.setUpMinidumpFile(
                minidumpToCopy, MinidumpUploaderTestConstants.BOUNDARY, "browser");
        final String expectedFileContent = readEntireFile(minidumpToCopy);

        File[] uploadedFiles =
                copyAndUploadMinidumpsSync(
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

    private void helperCrashSamplingForStable(@Channel int channel) throws IOException {
        // When samplePercentage is < CRASH_DUMP_PERCENTAGE_FOR_STABLE, crashes are sampled-in.
        testSampleCrashesByChannel(
                channel, /* samplePercentage= */ 0, /* expectedSamplingVerdict= */ true);

        // When samplePercentage is >= CRASH_DUMP_PERCENTAGE_FOR_STABLE, crashes are sampled-out.
        testSampleCrashesByChannel(
                channel, /* samplePercentage= */ 1, /* expectedSamplingVerdict= */ false);
        testSampleCrashesByChannel(
                channel, /* samplePercentage= */ 99, /* expectedSamplingVerdict= */ false);
    }

    /** Verify sampling behavior for the STABLE channel. */
    @Test
    @MediumTest
    public void testCrashSamplingStableChannel() throws IOException {
        helperCrashSamplingForStable(Channel.STABLE);
    }

    /** DEFAULT channel should behave the same as STABLE channel. */
    @Test
    @MediumTest
    public void testCrashSamplingDefaultChannel() throws IOException {
        helperCrashSamplingForStable(Channel.DEFAULT);
    }

    /** UNKNOWN channel should behave the same as STABLE channel. */
    @Test
    @MediumTest
    public void testCrashSamplingUnknownChannel() throws IOException {
        helperCrashSamplingForStable(Channel.UNKNOWN);
    }

    @Test
    @MediumTest
    public void testCrashSamplingBetaChannel() throws IOException {
        // Crashes on Beta channel are sampled-in regardless of samplePercentage.
        testSampleCrashesByChannel(
                Channel.BETA, /* samplePercentage= */ 0, /* expectedSamplingVerdict= */ true);
        testSampleCrashesByChannel(
                Channel.BETA, /* samplePercentage= */ 1, /* expectedSamplingVerdict= */ true);
        testSampleCrashesByChannel(
                Channel.BETA, /* samplePercentage= */ 99, /* expectedSamplingVerdict= */ true);
    }

    @Test
    @MediumTest
    public void testCrashSamplingDevChannel() throws IOException {
        // Crashes on Dev channel are sampled-in regardless of samplePercentage.
        testSampleCrashesByChannel(
                Channel.DEV, /* samplePercentage= */ 0, /* expectedSamplingVerdict= */ true);
        testSampleCrashesByChannel(
                Channel.DEV, /* samplePercentage= */ 1, /* expectedSamplingVerdict= */ true);
        testSampleCrashesByChannel(
                Channel.DEV, /* samplePercentage= */ 99, /* expectedSamplingVerdict= */ true);
    }

    @Test
    @MediumTest
    public void testCrashSamplingCanaryChannel() throws IOException {
        // Crashes on Canary channel are sampled-in regardless of samplePercentage.
        testSampleCrashesByChannel(
                Channel.CANARY, /* samplePercentage= */ 0, /* expectedSamplingVerdict= */ true);
        testSampleCrashesByChannel(
                Channel.CANARY, /* samplePercentage= */ 1, /* expectedSamplingVerdict= */ true);
        testSampleCrashesByChannel(
                Channel.CANARY, /* samplePercentage= */ 99, /* expectedSamplingVerdict= */ true);
    }

    /**
     * MinidumpUploaderDelegate sub-class that uses MinidumpUploaderDelegate's implementation of
     * {@see CrashReportingPermissionManager#isUsageAndCrashReportingPermitted()}.
     */
    private static class TestCrashSamplingMinidumpUploaderDelegate
            extends AwMinidumpUploaderDelegate {
        private final boolean mExpectedSamplingVerdict;
        private final int mSamplePercentage;

        TestCrashSamplingMinidumpUploaderDelegate(
                SamplingDelegate samplingDelegate, boolean expectedSamplingVerdict) {
            super(samplingDelegate);
            mExpectedSamplingVerdict = expectedSamplingVerdict;
            mSamplePercentage = samplingDelegate.getRandomSample();
        }

        @Override
        public CrashReportingPermissionManager createCrashReportingPermissionManager() {
            final CrashReportingPermissionManager realPermissionManager =
                    super.createCrashReportingPermissionManager();

            return new MockCrashReportingPermissionManager() {
                {
                    // This setup ensures we depend on isClientInSampleForCrashes().
                    mIsUserPermitted = true;
                    mIsNetworkAvailable = true;
                    mIsEnabledForTests = false;
                }

                @Override
                public boolean isClientInSampleForCrashes() {
                    // Ensure that we use the real implementation of isClientInSampleForCrashes.
                    boolean isSampled = realPermissionManager.isClientInSampleForCrashes();
                    Assert.assertEquals(
                            "Wrong sampling verdict when the samplePercentage is "
                                    + mSamplePercentage,
                            mExpectedSamplingVerdict,
                            isSampled);
                    return isSampled;
                }
            };
        }
    }

    private void testSampleCrashesByChannel(
            @Channel int channel, int samplePercentage, boolean expectedSamplingVerdict)
            throws IOException {
        PlatformServiceBridge.injectInstance(new TestPlatformServiceBridge(/* enabled= */ true));
        MinidumpUploaderDelegate delegate =
                new TestCrashSamplingMinidumpUploaderDelegate(
                        new TestSamplingDelegate(channel, samplePercentage),
                        expectedSamplingVerdict);
        MinidumpUploadJob minidumpUploadJob = new TestMinidumpUploadJobImpl(delegate);

        File firstFile = createMinidumpFileInCrashDir("1_abc.dmp0.try0");
        File secondFile = createMinidumpFileInCrashDir("12_abcd.dmp0.try0");
        File expectedFirstFile =
                new File(
                        mTestRule.getCrashDir(),
                        firstFile
                                .getName()
                                .replace(".dmp", expectedSamplingVerdict ? ".up" : ".skipped"));
        File expectedSecondFile =
                new File(
                        mTestRule.getCrashDir(),
                        secondFile
                                .getName()
                                .replace(".dmp", expectedSamplingVerdict ? ".up" : ".skipped"));

        uploadMinidumpsSync(minidumpUploadJob, /* expectReschedule= */ false);

        Assert.assertFalse(firstFile.exists());
        Assert.assertTrue(expectedFirstFile.exists());
        Assert.assertFalse(secondFile.exists());
        Assert.assertTrue(expectedSecondFile.exists());
    }

    /** Ensure that when PlatformServiceBridge returns true we do upload minidumps. */
    @Test
    @MediumTest
    public void testPlatformServicesBridgeIsUsedUserConsent() throws IOException {
        testPlatformServicesBridgeIsUsed(true);
    }

    /** Ensure that when PlatformServiceBridge returns false we do not upload minidumps. */
    @Test
    @MediumTest
    public void testPlatformServicesBridgeIsUsedNoUserConsent() throws IOException {
        testPlatformServicesBridgeIsUsed(false);
    }

    /**
     * MinidumpUploaderDelegate sub-class that uses MinidumpUploaderDelegate's implementation of
     * {@see CrashReportingPermissionManager#isUsageAndCrashReportingPermitted()}.
     */
    private static class WebViewUserConsentMinidumpUploaderDelegate
            extends AwMinidumpUploaderDelegate {
        private final boolean mUserConsent;

        WebViewUserConsentMinidumpUploaderDelegate(boolean userConsent) {
            super(TEST_SAMPLING_DELEGATE);
            mUserConsent = userConsent;
        }

        @Override
        public CrashReportingPermissionManager createCrashReportingPermissionManager() {
            final CrashReportingPermissionManager realPermissionManager =
                    super.createCrashReportingPermissionManager();
            return new MockCrashReportingPermissionManager() {
                {
                    // This setup ensures we depend on isUsageAndCrashReportingPermitted().
                    mIsInSample = true;
                    mIsNetworkAvailable = true;
                    mIsEnabledForTests = false;
                }

                @Override
                public boolean isUsageAndCrashReportingPermitted() {
                    // Ensure that we use the real implementation of
                    // isUsageAndCrashReportingPermitted.
                    boolean userPermitted =
                            realPermissionManager.isUsageAndCrashReportingPermitted();
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
        MinidumpUploadJob minidumpUploadJob = new TestMinidumpUploadJobImpl(delegate);

        File firstFile = createMinidumpFileInCrashDir("1_abc.dmp0.try0");
        File secondFile = createMinidumpFileInCrashDir("12_abcd.dmp0.try0");
        File expectedFirstFile =
                new File(
                        mTestRule.getCrashDir(),
                        firstFile.getName().replace(".dmp", userConsent ? ".up" : ".skipped"));
        File expectedSecondFile =
                new File(
                        mTestRule.getCrashDir(),
                        secondFile.getName().replace(".dmp", userConsent ? ".up" : ".skipped"));

        uploadMinidumpsSync(minidumpUploadJob, /* expectReschedule= */ false);

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
        CrashTestRule.setUpMinidumpFile(
                firstMinidumpToCopy, MinidumpUploaderTestConstants.BOUNDARY, "browser");
        CrashTestRule.setUpMinidumpFile(
                secondMinidumpToCopy, MinidumpUploaderTestConstants.BOUNDARY, "renderer");
        final String expectedFirstFileContent = readEntireFile(firstMinidumpToCopy);
        final String expectedSecondFileContent = readEntireFile(secondMinidumpToCopy);

        File[] uploadedFiles =
                copyAndUploadMinidumpsSync(
                        fileManager,
                        new File[][] {{firstMinidumpToCopy}, {secondMinidumpToCopy}},
                        new int[] {0, 0});

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
     * Utility method for uploading minidumps, and waiting for the uploads to finish.
     * @param minidumpUploadJob the implementation to use to upload minidumps.
     * @param expectReschedule value used to assert whether the uploads should be rescheduled,
     *                         e.g. when uploading succeeds we should normally not expect to
     *                         reschedule.
     */
    private static void uploadMinidumpsSync(
            MinidumpUploadJob minidumpUploadJob, boolean expectReschedule) {
        final CountDownLatch uploadsFinishedLatch = new CountDownLatch(1);
        AtomicBoolean wasRescheduled = new AtomicBoolean();
        ThreadUtils.runOnUiThread(
                () -> {
                    minidumpUploadJob.uploadAllMinidumps(
                            reschedule -> {
                                wasRescheduled.set(reschedule);
                                uploadsFinishedLatch.countDown();
                            });
                });
        try {
            Assert.assertTrue(
                    uploadsFinishedLatch.await(scaleTimeout(3000), TimeUnit.MILLISECONDS));
            Assert.assertEquals(expectReschedule, wasRescheduled.get());
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Copy and upload {@param minidumps} by one array at a time - i.e. the minidumps in a single
     * array in {@param minidumps} will all be copied in the same call into CrashReceiverService.
     *
     * @param fileManager the CrashFileManager to use when copying/renaming minidumps.
     * @param minidumps an array of arrays of minidumps to copy and upload, by copying one array at
     *     a time.
     * @param uids an array of uids declaring the uids used when calling into CrashReceiverService.
     * @return the uploaded files.
     */
    private File[] copyAndUploadMinidumpsSync(
            CrashFileManager fileManager, File[][] minidumps, int[] uids)
            throws FileNotFoundException {
        CrashReceiverService crashReceiverService = new CrashReceiverService();
        Assert.assertEquals(minidumps.length, uids.length);
        // Ensure the upload service minidump directory is empty before we start copying files.
        File[] initialMinidumps =
                fileManager.getMinidumpsReadyForUpload(
                        MinidumpUploadJobImpl.MAX_UPLOAD_TRIES_ALLOWED);
        Assert.assertEquals(0, initialMinidumps.length);

        // Open file descriptors to the files and then delete the files.
        ParcelFileDescriptor[][] fileDescriptors = new ParcelFileDescriptor[minidumps.length][];
        int numMinidumps = 0;
        for (int n = 0; n < minidumps.length; n++) {
            File[] currentMinidumps = minidumps[n];
            ArrayList<Map<String, String>> crashInfos = new ArrayList<>();
            numMinidumps += currentMinidumps.length;
            fileDescriptors[n] = new ParcelFileDescriptor[currentMinidumps.length];
            for (int m = 0; m < currentMinidumps.length; m++) {
                fileDescriptors[n][m] =
                        ParcelFileDescriptor.open(
                                currentMinidumps[m], ParcelFileDescriptor.MODE_READ_ONLY);
                Assert.assertTrue(currentMinidumps[m].delete());
                crashInfos.add(null);
            }
            crashReceiverService.performMinidumpCopyingSerially(
                    /* uid= */ uids[n],
                    fileDescriptors[n],
                    crashInfos,
                    /* scheduleUploads= */ false);
        }

        final CrashReportingPermissionManager permManager =
                new MockCrashReportingPermissionManager() {
                    {
                        mIsEnabledForTests = true;
                    }
                };
        MinidumpUploadJob minidumpUploadJob =
                // Use AwMinidumpUploaderDelegate instead of TestMinidumpUploaderDelegate to ensure
                // AwMinidumpUploaderDelegate works well together with the minidump-copying methods
                // of CrashReceiverService.
                new TestMinidumpUploadJobImpl(
                        new AwMinidumpUploaderDelegate(TEST_SAMPLING_DELEGATE) {
                            @Override
                            public CrashReportingPermissionManager
                                    createCrashReportingPermissionManager() {
                                return permManager;
                            }
                        });

        uploadMinidumpsSync(minidumpUploadJob, /* expectReschedule= */ false);
        // Ensure there are no minidumps left to upload.
        File[] nonUploadedMinidumps =
                fileManager.getMinidumpsReadyForUpload(
                        MinidumpUploadJobImpl.MAX_UPLOAD_TRIES_ALLOWED);
        Assert.assertEquals(0, nonUploadedMinidumps.length);

        File[] uploadedFiles = fileManager.getAllUploadedFiles();
        Assert.assertEquals(numMinidumps, uploadedFiles.length);
        return uploadedFiles;
    }

    private File createMinidumpFileInCrashDir(String name) throws IOException {
        File minidumpFile = new File(mTestRule.getCrashDir(), name);
        CrashTestRule.setUpMinidumpFile(minidumpFile, MinidumpUploaderTestConstants.BOUNDARY);
        return minidumpFile;
    }
}
