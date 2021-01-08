// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;

import android.os.Build;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.metrics.AwMetricsServiceClient;
import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.compat.ApiHelperForM;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.components.metrics.ChromeUserMetricsExtensionProtos.ChromeUserMetricsExtension;
import org.chromium.components.metrics.MetricsSwitches;
import org.chromium.components.metrics.SystemProfileProtos.SystemProfileProto;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

/**
 * Integration test to verify WebView's metrics implementation. This isn't a great spot to verify
 * WebView reports metadata correctly; that should be done with unittests on individual
 * MetricsProviders. This is an opportunity to verify these MetricsProviders (or other components
 * integrating with the MetricsService) are hooked up in WebView's implementation.
 *
 * <p>This configures the initial metrics upload to happen very quickly (so tests don't need to run
 * multiple seconds). This also configures subsequent uploads to happen very frequently (see
 * UPLOAD_INTERVAL_MS), although many test cases won't require this (and since each test case runs
 * in a separate browser process, often we'll never reach subsequent uploads, see
 * https://crbug.com/932582).
 */
@RunWith(AwJUnit4ClassRunner.class)
@CommandLineFlags.Add({MetricsSwitches.FORCE_ENABLE_METRICS_REPORTING}) // Override sampling logic
public class AwMetricsIntegrationTest {
    @Rule
    public AwActivityTestRule mRule = new AwActivityTestRule();

    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestAwContentsClient mContentsClient;
    private TestPlatformServiceBridge mPlatformServiceBridge;

    // Some short interval, arbitrarily chosen.
    private static final long UPLOAD_INTERVAL_MS = 10;

    private static class TestPlatformServiceBridge extends PlatformServiceBridge {
        private final BlockingQueue<byte[]> mQueue;

        public TestPlatformServiceBridge() {
            mQueue = new LinkedBlockingQueue<>();
        }

        @Override
        public boolean canUseGms() {
            return true;
        }

        @Override
        public void queryMetricsSetting(Callback<Boolean> callback) {
            ThreadUtils.assertOnUiThread();
            callback.onResult(true /* enabled */);
        }

        @Override
        public void logMetrics(byte[] data) {
            mQueue.add(data);
        }

        /**
         * Gets the latest metrics log we've received.
         */
        public ChromeUserMetricsExtension waitForNextMetricsLog() throws Exception {
            byte[] data = AwActivityTestRule.waitForNextQueueElement(mQueue);
            return ChromeUserMetricsExtension.parseFrom(data);
        }

        /**
         * Asserts there are no more metrics logs queued up.
         */
        public void assertNoMetricsLogs() throws Exception {
            // Assert the size is zero (rather than the queue is empty), so if this fails we have
            // some hint as to how many logs were queued up.
            Assert.assertEquals("Expected no metrics logs to be in the queue", 0, mQueue.size());
        }
    }

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        // Kick off the metrics consent-fetching process. TestPlatformServiceBridge mocks out user
        // consent for when we query it with AwBrowserProcess.handleMinidumpsAndSetMetricsConsent(),
        // so metrics consent is guaranteed to be granted.
        mPlatformServiceBridge = new TestPlatformServiceBridge();
        PlatformServiceBridge.injectInstance(mPlatformServiceBridge);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Need to configure the metrics delay first, because
            // handleMinidumpsAndSetMetricsConsent() triggers MetricsService initialization. The
            // first upload for each test case will be triggered with minimal latency, and
            // subsequent uploads (for tests cases which need them) will be scheduled every
            // UPLOAD_INTERVAL_MS. We use programmatic hooks (instead of commandline flags) because:
            //  * We don't want users in the wild to upload reports to Google which are recorded
            //    immediately after startup: these records would be very unusual in that they don't
            //    contain (many) histograms.
            //  * The interval for subsequent uploads is rate-limited to mitigate accidentally
            //    DOS'ing the metrics server. We want to keep that protection for clients in the
            //    wild, but don't need the same protection for the test because it doesn't upload
            //    reports.
            AwMetricsServiceClient.setFastStartupForTesting(true);
            AwMetricsServiceClient.setUploadIntervalForTesting(UPLOAD_INTERVAL_MS);

            AwBrowserProcess.handleMinidumpsAndSetMetricsConsent(true /* updateMetricsConsent */);
        });
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_basicInfo() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        Assert.assertEquals(ChromeUserMetricsExtension.Product.ANDROID_WEBVIEW,
                ChromeUserMetricsExtension.Product.forNumber(log.getProduct()));
        Assert.assertTrue("Should have some client_id", log.hasClientId());
        Assert.assertTrue("Should have some session_id", log.hasSessionId());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_buildInfo() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        SystemProfileProto systemProfile = log.getSystemProfile();
        Assert.assertTrue("Should have some build_timestamp", systemProfile.hasBuildTimestamp());
        Assert.assertTrue("Should have some app_version", systemProfile.hasAppVersion());
        Assert.assertTrue("Should have some channel", systemProfile.hasChannel());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_miscellaneousSystemProfileInfo() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        SystemProfileProto systemProfile = log.getSystemProfile();
        Assert.assertTrue("Should have some uma_enabled_date", systemProfile.hasUmaEnabledDate());
        Assert.assertTrue("Should have some install_date", systemProfile.hasInstallDate());
        // Don't assert application_locale's value, because we don't want to enforce capitalization
        // requirements on the metrics service (ex. in case it switches from "en-US" to "en-us" for
        // some reason).
        Assert.assertTrue(
                "Should have some application_locale", systemProfile.hasApplicationLocale());

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            Assert.assertEquals(
                    ApiHelperForM.isProcess64Bit(), systemProfile.getAppVersion().contains("-64"));
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_osData() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        SystemProfileProto systemProfile = log.getSystemProfile();
        Assert.assertEquals("Android", systemProfile.getOs().getName());
        Assert.assertTrue("Should have some os.version", systemProfile.getOs().hasVersion());
        Assert.assertTrue("Should have some os.build_fingerprint",
                systemProfile.getOs().hasBuildFingerprint());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_hardwareMiscellaneous() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        SystemProfileProto systemProfile = log.getSystemProfile();
        Assert.assertTrue("Should have some hardware.system_ram_mb",
                systemProfile.getHardware().hasSystemRamMb());
        Assert.assertTrue("Should have some hardware.hardware_class",
                systemProfile.getHardware().hasHardwareClass());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_hardwareScreen() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        SystemProfileProto systemProfile = log.getSystemProfile();
        Assert.assertTrue("Should have some hardware.screen_count",
                systemProfile.getHardware().hasScreenCount());
        Assert.assertTrue("Should have some hardware.primary_screen_width",
                systemProfile.getHardware().hasPrimaryScreenWidth());
        Assert.assertTrue("Should have some hardware.primary_screen_height",
                systemProfile.getHardware().hasPrimaryScreenHeight());
        Assert.assertTrue("Should have some hardware.primary_screen_scale_factor",
                systemProfile.getHardware().hasPrimaryScreenScaleFactor());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_hardwareCpu() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        SystemProfileProto systemProfile = log.getSystemProfile();
        Assert.assertTrue("Should have some hardware.cpu_architecture",
                systemProfile.getHardware().hasCpuArchitecture());
        Assert.assertTrue("Should have some hardware.cpu.vendor_name",
                systemProfile.getHardware().getCpu().hasVendorName());
        Assert.assertTrue("Should have some hardware.cpu.signature",
                systemProfile.getHardware().getCpu().hasSignature());
        Assert.assertTrue("Should have some hardware.cpu.num_cores",
                systemProfile.getHardware().getCpu().hasNumCores());
        Assert.assertTrue("Should have some hardware.cpu.is_hypervisor",
                systemProfile.getHardware().getCpu().hasIsHypervisor());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_hardwareGpu() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        SystemProfileProto systemProfile = log.getSystemProfile();
        Assert.assertTrue("Should have some hardware.gpu.driver_version",
                systemProfile.getHardware().getGpu().hasDriverVersion());
        Assert.assertTrue("Should have some hardware.gpu.gl_vendor",
                systemProfile.getHardware().getGpu().hasGlVendor());
        Assert.assertTrue("Should have some hardware.gpu.gl_renderer",
                systemProfile.getHardware().getGpu().hasGlRenderer());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_hardwareDrive() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        SystemProfileProto systemProfile = log.getSystemProfile();
        Assert.assertTrue("Should have some hardware.app_drive.has_seek_penalty",
                systemProfile.getHardware().getAppDrive().hasHasSeekPenalty());
        Assert.assertTrue("Should have some hardware.user_data_drive.has_seek_penalty",
                systemProfile.getHardware().getUserDataDrive().hasHasSeekPenalty());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_network() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        SystemProfileProto systemProfile = log.getSystemProfile();
        Assert.assertTrue("Should have some network.connection_type_is_ambiguous",
                systemProfile.getNetwork().hasConnectionTypeIsAmbiguous());
        Assert.assertTrue("Should have some network.connection_type",
                systemProfile.getNetwork().hasConnectionType());
        Assert.assertTrue("Should have some network.wifi_phy_layer_protocol_is_ambiguous",
                systemProfile.getNetwork().hasWifiPhyLayerProtocolIsAmbiguous());
        Assert.assertTrue("Should have some network.wifi_phy_layer_protocol",
                systemProfile.getNetwork().hasWifiPhyLayerProtocol());
        Assert.assertTrue("Should have some network.min_effective_connection_type",
                systemProfile.getNetwork().hasMinEffectiveConnectionType());
        Assert.assertTrue("Should have some network.max_effective_connection_type",
                systemProfile.getNetwork().hasMaxEffectiveConnectionType());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_androidHistograms() throws Throwable {
        // Wait for a metrics log, since AndroidMetricsProvider only logs this histogram during log
        // collection. Do not assert anything about this histogram before this point (ex. do not
        // assert total count == 0), because this would race with the initial metrics log.
        mPlatformServiceBridge.waitForNextMetricsLog();

        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting("MemoryAndroid.LowRamDevice"));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testPageLoadsEnableMultipleUploads() throws Throwable {
        mPlatformServiceBridge.waitForNextMetricsLog();

        // At this point, the MetricsService should be asleep, and should not have created any more
        // metrics logs.
        mPlatformServiceBridge.assertNoMetricsLogs();

        // The start of a page load should be enough to indicate to the MetricsService that the app
        // is "in use" and it's OK to upload the next record. No need to wait for onPageFinished,
        // since this behavior should be gated on NOTIFICATION_LOAD_START.
        mRule.loadUrlAsync(mAwContents, "about:blank");

        // This may take slightly longer than UPLOAD_INTERVAL_MS, due to the time spent processing
        // the metrics log, but should be well within the timeout (unless something is broken).
        mPlatformServiceBridge.waitForNextMetricsLog();

        // If we get here, we got a second metrics log (and the test may pass). If there was no
        // second metrics log, then the above call will fail with TimeoutException. We should not
        // assertNoMetricsLogs() however, because it's possible we got a metrics log between
        // onPageStarted & onPageFinished, in which case onPageFinished would *also* wake up the
        // metrics service, and we might potentially have a third metrics log in the queue.
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @OnlyRunIn(MULTI_PROCESS) // This test is specific to the OOP-renderer
    public void testRendererHistograms() throws Throwable {
        EmbeddedTestServer embeddedTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        try {
            // Discard initial log since the renderer process hasn't been created yet.
            mPlatformServiceBridge.waitForNextMetricsLog();

            final CallbackHelper helper = new CallbackHelper();
            int finalMetricsCollectedCount = helper.getCallCount();

            // Load a page and wait for final metrics collection.
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                AwMetricsServiceClient.setOnFinalMetricsCollectedListenerForTesting(
                        () -> { helper.notifyCalled(); });
            });

            // Load a page to ensure the renderer process is created.
            mRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                    embeddedTestServer.getURL("/android_webview/test/data/hello_world.html"));
            helper.waitForCallback(finalMetricsCollectedCount, 1);

            // At this point we know one of two things must be true:
            //
            // 1. The renderer process completed startup (logging the expected histogram) before
            //    subprocess histograms were collected. In this case, we know the desired histogram
            //    has been copied into the browser process.
            // 2. Subprocess histograms were collected before the renderer process completed
            //    startup. While we don't know if our histogram was copied over, we do know the
            //    page load has finished and this woke up the metrics service, so MetricsService
            //    will collect subprocess metrics again.
            //
            // Load a page and wait for another final log collection. We know this log collection
            // must be triggered by either the second page load start (scenario 1) or the first page
            // load finish (scenario 2), either of which ensures the renderer startup histogram must
            // have been copied into the browser process.

            mRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                    embeddedTestServer.getURL("/android_webview/test/data/hello_world.html"));
            helper.waitForCallback(finalMetricsCollectedCount, 2);

            Assert.assertEquals(1,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            "Android.SeccompStatus.RendererSandbox"));
        } finally {
            embeddedTestServer.stopAndDestroyServer();
        }
    }
}
