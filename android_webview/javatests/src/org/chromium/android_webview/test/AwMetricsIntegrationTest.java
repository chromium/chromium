// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;

import android.os.Process;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import org.hamcrest.Description;
import org.hamcrest.Matchers;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.metrics.AwMetricsServiceClient;
import org.chromium.android_webview.metrics.MetricsFilteringDecorator;
import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.components.metrics.AndroidMetricsLogConsumer;
import org.chromium.components.metrics.AndroidMetricsLogUploader;
import org.chromium.components.metrics.AndroidMetricsServiceClient;
import org.chromium.components.metrics.ChromeUserMetricsExtensionProtos.ChromeUserMetricsExtension;
import org.chromium.components.metrics.InstallerPackageType;
import org.chromium.components.metrics.MetricsSwitches;
import org.chromium.components.metrics.StabilityEventType;
import org.chromium.components.metrics.SystemProfileProtos.SystemProfileProto;
import org.chromium.components.metrics.SystemProfileProtos.SystemProfileProto.ChromeComponent;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.EmbeddedTestServer;

import java.net.HttpURLConnection;
import java.util.concurrent.TimeUnit;

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
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@CommandLineFlags.Add({MetricsSwitches.FORCE_ENABLE_METRICS_REPORTING}) // Override sampling logic
public class AwMetricsIntegrationTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mRule;

    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestAwContentsClient mContentsClient;
    private MetricsTestPlatformServiceBridge mPlatformServiceBridge;

    // Some short interval, arbitrarily chosen.
    private static final long UPLOAD_INTERVAL_MS = 10;

    public AwMetricsIntegrationTest(AwSettingsMutation param) {
        this.mRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        // Kick off the metrics consent-fetching process. MetricsTestPlatformServiceBridge mocks out
        // user consent for when we query it with
        // AwBrowserProcess.handleMinidumpsAndSetMetricsConsent(), so metrics consent is guaranteed
        // to be granted.
        mPlatformServiceBridge = new MetricsTestPlatformServiceBridge();
        PlatformServiceBridge.injectInstance(mPlatformServiceBridge);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Explicitly send the data to PlatformServiceBridge and avoid sending the data
                    // via MetricsUploadService to avoid unexpected failures due to service
                    // connections, IPCs ... etc in tests as testing the service behaviour is
                    // outside the scope of these integeration tests.
                    AndroidMetricsLogConsumer directUploader =
                            data -> {
                                PlatformServiceBridge.getInstance().logMetrics(data);
                                return HttpURLConnection.HTTP_OK;
                            };
                    AndroidMetricsLogUploader.setConsumer(
                            new MetricsFilteringDecorator(directUploader));

                    // Need to configure the metrics delay first, because
                    // handleMinidumpsAndSetMetricsConsent() triggers MetricsService initialization.
                    // The first upload for each test case will be triggered with minimal latency,
                    // and subsequent uploads (for tests cases which need them) will be scheduled
                    // every UPLOAD_INTERVAL_MS. We use programmatic hooks (instead of commandline
                    // flags) because:
                    //  * We don't want users in the wild to upload reports to Google which are
                    // recorded
                    //    immediately after startup: these records would be very unusual in that
                    // they don't
                    //    contain (many) histograms.
                    //  * The interval for subsequent uploads is rate-limited to mitigate
                    // accidentally
                    //    DOS'ing the metrics server. We want to keep that protection for clients in
                    // the
                    //    wild, but don't need the same protection for the test because it doesn't
                    // upload
                    //    reports.
                    AwMetricsServiceClient.setFastStartupForTesting(true);
                    AwMetricsServiceClient.setUploadIntervalForTesting(UPLOAD_INTERVAL_MS);

                    AwBrowserProcess.handleMinidumpsAndSetMetricsConsent(
                            /* updateMetricsConsent= */ true);
                });
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_basicInfo() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        assertEquals(
                ChromeUserMetricsExtension.Product.ANDROID_WEBVIEW,
                ChromeUserMetricsExtension.Product.forNumber(log.getProduct()));
        assertTrue("Should have some client_id", log.hasClientId());
        assertTrue("Should have some session_id", log.hasSessionId());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_buildInfo() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        SystemProfileProto systemProfile = log.getSystemProfile();
        assertTrue("Should have some build_timestamp", systemProfile.hasBuildTimestamp());
        assertTrue("Should have some app_version", systemProfile.hasAppVersion());
        assertTrue("Should have some channel", systemProfile.hasChannel());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_miscellaneousSystemProfileInfo() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        SystemProfileProto systemProfile = log.getSystemProfile();
        assertTrue("Should have some uma_enabled_date", systemProfile.hasUmaEnabledDate());
        assertTrue("Should have some install_date", systemProfile.hasInstallDate());
        // Don't assert application_locale's value, because we don't want to enforce capitalization
        // requirements on the metrics service (ex. in case it switches from "en-US" to "en-us" for
        // some reason).
        assertTrue("Should have some application_locale", systemProfile.hasApplicationLocale());

        assertEquals(Process.is64Bit(), systemProfile.getAppVersion().contains("-64"));
        assertTrue("Should have some low_entropy_source", systemProfile.hasLowEntropySource());
        assertTrue(
                "Should have some old_low_entropy_source", systemProfile.hasOldLowEntropySource());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_osData() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        SystemProfileProto systemProfile = log.getSystemProfile();
        assertEquals("Android", systemProfile.getOs().getName());
        assertTrue("Should have some os.version", systemProfile.getOs().hasVersion());
        assertTrue(
                "Should have some os.build_fingerprint",
                systemProfile.getOs().hasBuildFingerprint());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_hardwareMiscellaneous() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        SystemProfileProto systemProfile = log.getSystemProfile();
        assertTrue(
                "Should have some hardware.system_ram_mb",
                systemProfile.getHardware().hasSystemRamMb());
        assertTrue(
                "Should have some hardware.hardware_class",
                systemProfile.getHardware().hasHardwareClass());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_hardwareScreen() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        SystemProfileProto systemProfile = log.getSystemProfile();
        assertTrue(
                "Should have some hardware.screen_count",
                systemProfile.getHardware().hasScreenCount());
        assertTrue(
                "Should have some hardware.primary_screen_width",
                systemProfile.getHardware().hasPrimaryScreenWidth());
        assertTrue(
                "Should have some hardware.primary_screen_height",
                systemProfile.getHardware().hasPrimaryScreenHeight());
        assertTrue(
                "Should have some hardware.primary_screen_scale_factor",
                systemProfile.getHardware().hasPrimaryScreenScaleFactor());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_hardwareCpu() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        SystemProfileProto systemProfile = log.getSystemProfile();
        assertTrue(
                "Should have some hardware.cpu_architecture",
                systemProfile.getHardware().hasCpuArchitecture());
        assertTrue(
                "Should have some hardware.cpu.vendor_name",
                systemProfile.getHardware().getCpu().hasVendorName());
        assertTrue(
                "Should have some hardware.cpu.signature",
                systemProfile.getHardware().getCpu().hasSignature());
        assertTrue(
                "Should have some hardware.cpu.num_cores",
                systemProfile.getHardware().getCpu().hasNumCores());
        assertTrue(
                "Should have some hardware.cpu.is_hypervisor",
                systemProfile.getHardware().getCpu().hasIsHypervisor());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_hardwareGpu() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        SystemProfileProto systemProfile = log.getSystemProfile();
        assertTrue(
                "Should have some hardware.gpu.driver_version",
                systemProfile.getHardware().getGpu().hasDriverVersion());
        assertTrue(
                "Should have some hardware.gpu.gl_vendor",
                systemProfile.getHardware().getGpu().hasGlVendor());
        assertTrue(
                "Should have some hardware.gpu.gl_renderer",
                systemProfile.getHardware().getGpu().hasGlRenderer());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_hardwareDrive() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        SystemProfileProto systemProfile = log.getSystemProfile();
        assertTrue(
                "Should have some hardware.app_drive.has_seek_penalty",
                systemProfile.getHardware().getAppDrive().hasHasSeekPenalty());
        assertTrue(
                "Should have some hardware.user_data_drive.has_seek_penalty",
                systemProfile.getHardware().getUserDataDrive().hasHasSeekPenalty());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_network() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        SystemProfileProto systemProfile = log.getSystemProfile();
        assertTrue(
                "Should have some network.connection_type_is_ambiguous",
                systemProfile.getNetwork().hasConnectionTypeIsAmbiguous());
        assertTrue(
                "Should have some network.connection_type",
                systemProfile.getNetwork().hasConnectionType());
        assertTrue(
                "Should have some network.min_effective_connection_type",
                systemProfile.getNetwork().hasMinEffectiveConnectionType());
        assertTrue(
                "Should have some network.max_effective_connection_type",
                systemProfile.getNetwork().hasMaxEffectiveConnectionType());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_stability_pageLoad() throws Throwable {
        EmbeddedTestServer embeddedTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        // Load a page to ensure the renderer process is created.
        mRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                embeddedTestServer.getURL("/android_webview/test/data/hello_world.html"));
        assertEquals(
                "Should have correct stability histogram kPageLoad count",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Stability.Counts2", StabilityEventType.PAGE_LOAD));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=CreateSpareRendererOnBrowserContextCreation"})
    public void testMetadata_stability_rendererLaunchCount() throws Throwable {
        EmbeddedTestServer embeddedTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        // Load a page to ensure the renderer process is created.
        mRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                embeddedTestServer.getURL("/android_webview/test/data/hello_world.html"));
        assertEquals(
                "Should have correct stability histogram kRendererLaunch count",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Stability.Counts2", StabilityEventType.RENDERER_LAUNCH));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @OnlyRunIn(MULTI_PROCESS) // This functionality is specific to the OOP-renderer
    public void testMetadata_stability_rendererCrashCount() throws Throwable {
        TestAwContentsClient.RenderProcessGoneHelper helper =
                mContentsClient.getRenderProcessGoneHelper();
        helper.setResponse(true); // Don't automatically kill the browser process.

        // Ensure that the renderer has started.
        mRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        // Crash the renderer and wait for onRenderProcessGone to be called.
        int callCount = helper.getCallCount();
        mRule.loadUrlAsync(mAwContents, "chrome://crash");
        helper.waitForCallback(
                callCount, 1, CallbackHelper.WAIT_TIMEOUT_SECONDS * 5, TimeUnit.SECONDS);

        assertEquals(
                "Should have correct stability histogram kRendererCrash count",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Stability.Counts2", StabilityEventType.RENDERER_CRASH));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_stability_browserLaunchCount() throws Throwable {
        // This should be triggered simply by initializing the MetricsService. This should be logged
        // (and persisted) even before we start collecting the first metrics log.
        assertEquals(
                "Should have correct stability histogram kLaunch count",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Stability.Counts2", StabilityEventType.LAUNCH));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_androidHistograms() throws Throwable {
        // Wait for a metrics log, since AndroidMetricsProvider logs this histogram once a
        // metrics log is created if the feature is enabled.
        // Do not assert anything about this histogram before this point (ex. do not
        // assert total count == 0), because this would race with the initial metrics log.
        mPlatformServiceBridge.waitForNextMetricsLog();

        // At this point, this histogram should be logged for the initial metrics log
        // and the first ongoing metrics log upon opening.
        assertEquals(
                2, RecordHistogram.getHistogramTotalCountForTesting("MemoryAndroid.LowRamDevice"));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_samplingRate() throws Throwable {
        // Wait for a metrics log, since SamplingMetricsProvider only logs this histogram during log
        // collection. Do not assert anything about this histogram before this point (ex. do not
        // assert total count == 0), because this would race with the initial metrics log.
        mPlatformServiceBridge.waitForNextMetricsLog();

        assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting("UMA.SamplingRatePerMille"));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_accessibility() throws Throwable {
        // Wait for a metrics log, since AccessibilityMetricsProvider only logs this histogram
        // during log collection. Do not assert anything about this histogram before this point (ex.
        // do not assert total count == 0), because this would race with the initial metrics log.
        mPlatformServiceBridge.waitForNextMetricsLog();

        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Accessibility.Android.ScreenReader.EveryReport"));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_debugging() throws Throwable {
        // Wait for a metrics log, since DebuggingMetricsProvider only logs this histogram
        // during log collection. Do not assert anything about this histogram before this point (ex.
        // do not assert total count == 0), because this would race with the initial metrics log.
        mPlatformServiceBridge.waitForNextMetricsLog();

        Assume.assumeTrue(
                "Build type is userdebug in the test environment, so we expect this to pass.",
                BuildInfo.isDebugAndroidOrApp());

        assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.isDebuggable", /* sample=not enabled */ 0));
        assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.isDebuggable", /* sample=enabled by setWebContentsDebuggingEnabled(true) */
                        1));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.isDebuggable", /* sample=enabled by debuggable app or os */
                        2));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetadata_appPackageName() throws Throwable {
        final String appPackageName = ContextUtils.getApplicationContext().getPackageName();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwBrowserProcess.setWebViewPackageName(appPackageName);
                    AndroidMetricsServiceClient.setInstallerPackageTypeForTesting(
                            InstallerPackageType.GOOGLE_PLAY_STORE);
                });

        // Disregard the first UMA log because it's recorded before loading the allowlist.
        mPlatformServiceBridge.waitForNextMetricsLog();

        // Load a blank page to indicate to the MetricsService that the app is "in use" and
        // it's OK to upload the next record.
        mRule.loadUrlAsync(mAwContents, "about:blank");

        // Disregard the second UMA log as well because it is also created very early on
        // (since we have "fast startup for testing" enabled), and is very likely to have
        // been opened before the allowlist was loaded.
        mPlatformServiceBridge.waitForNextMetricsLog();

        // Load a blank page to indicate to the MetricsService that the app is "in use" and
        // it's OK to upload the next record.
        mRule.loadUrlAsync(mAwContents, "about:blank");

        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        SystemProfileProto systemProfile = log.getSystemProfile();
        assertEquals(appPackageName, systemProfile.getAppPackageName());
    }

    private static TypeSafeMatcher<ChromeComponent> matchesChromeComponent(
            ChromeComponent expected) {
        return new TypeSafeMatcher<ChromeComponent>() {
            @Override
            @SuppressWarnings("LiteProtoToString")
            public void describeTo(Description description) {
                description.appendText(expected.toString());
            }

            @Override
            @SuppressWarnings("LiteProtoToString")
            protected void describeMismatchSafely(
                    ChromeComponent item, Description mismatchDescription) {
                mismatchDescription.appendText("Doesn't match " + item.toString());
            }

            @Override
            public boolean matchesSafely(ChromeComponent item) {
                return expected.getComponentId() == item.getComponentId()
                        && expected.getVersion().equals(item.getVersion())
                        && expected.getOmahaFingerprint() == item.getOmahaFingerprint();
            }
        };
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
    @OnlyRunIn(MULTI_PROCESS) // This functionality is specific to the OOP-renderer
    @DisabledTest(message = "https://crbug.com/1524013")
    public void testRendererHistograms() throws Throwable {
        EmbeddedTestServer embeddedTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        // Discard initial log since the renderer process hasn't been created yet.
        mPlatformServiceBridge.waitForNextMetricsLog();
        final CallbackHelper helper = new CallbackHelper();
        int finalMetricsCollectedCount = helper.getCallCount();
        // Load a page and wait for final metrics collection.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwMetricsServiceClient.setOnFinalMetricsCollectedListenerForTesting(
                            () -> {
                                helper.notifyCalled();
                            });
                });
        // Load a page to ensure the renderer process is created.
        mRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
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
        mRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                embeddedTestServer.getURL("/android_webview/test/data/hello_world.html"));
        helper.waitForCallback(finalMetricsCollectedCount, 2);
        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.SeccompStatus.RendererSandbox"));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testScreenCoverageReporting() throws Throwable {
        EmbeddedTestServer embeddedTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        mRule.loadUrlAsync(
                mAwContents,
                embeddedTestServer.getURL("/android_webview/test/data/hello_world.html"));
        // We need to wait for log collection because the histogram is recorded during
        // MetricsProvider::ProvideCurrentSessionData().
        mPlatformServiceBridge.waitForNextMetricsLog();
        final String histogramName = "Android.WebView.VisibleScreenCoverage.Global";
        // The histogram records whole seconds that the WebView has been on screen, we need to
        // leave enough time for something to be recorded.
        CriteriaHelper.pollUiThread(
                () -> {
                    int totalSamples =
                            RecordHistogram.getHistogramTotalCountForTesting(histogramName);
                    Criteria.checkThat(
                            "There were no samples recorded", totalSamples, Matchers.not(0));
                });
        int totalSamples = RecordHistogram.getHistogramTotalCountForTesting(histogramName);
        int zeroBucketSamples = RecordHistogram.getHistogramValueCountForTesting(histogramName, 0);
        assertNotEquals(
                "There should be at least one sample in a non-zero bucket",
                zeroBucketSamples,
                totalSamples);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            1, AwContents.AwWindowCoverageTracker.sWindowCoverageTrackers.size());
                    mAwContents.onDetachedFromWindow();
                    assertEquals(
                            0, AwContents.AwWindowCoverageTracker.sWindowCoverageTrackers.size());
                });
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testServerSideAllowlistFilteringRequired() throws Throwable {
        ChromeUserMetricsExtension log = mPlatformServiceBridge.waitForNextMetricsLog();
        SystemProfileProto.AppPackageNameAllowlistFilter filter =
                log.getSystemProfile().getAppPackageNameAllowlistFilter();
        assertEquals(
                filter,
                SystemProfileProto.AppPackageNameAllowlistFilter.SERVER_SIDE_FILTER_REQUIRED);
    }
}
