// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_CHECK_INTERVAL_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_CHECK_INTERVAL_SHORT_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DEVICE_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_SVR;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VR_SETTINGS_SERVICE;

import android.annotation.TargetApi;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.uiautomator.UiDevice;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr.rules.VrSettingsFile;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.NativeUiUtils;
import org.chromium.chrome.browser.vr.util.PermissionUtils;
import org.chromium.chrome.browser.vr.util.VrSettingsServiceUtils;
import org.chromium.chrome.browser.vr.util.VrTestRuleUtils;
import org.chromium.chrome.browser.vr.util.VrTransitionUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.io.File;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * End-to-end tests for transitioning between WebXR's magic window and
 * presentation modes.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=LogJsConsoleMessages"})
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP) // WebXR is only supported on L+
@TargetApi(Build.VERSION_CODES.KITKAT) // Necessary to allow taking screenshots with UiAutomation
public class WebXrVrTransitionTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            VrTestRuleUtils.generateDefaultTestRuleParameters();
    @Rule
    public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrVrTestFramework mWebXrVrTestFramework;

    public WebXrVrTransitionTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = VrTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrVrTestFramework = new WebXrVrTestFramework(mTestRule);
    }

    /**
     * Tests that a successful request for an immersive session actually enters VR.
     */
    @Test
    @MediumTest
            @CommandLineFlags.Add({"enable-features=WebXR"})
            @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
            public void testRequestSessionEntersVr() {
        testPresentationEntryImpl(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("generic_webxr_page"),
                mWebXrVrTestFramework);
    }

    private void testPresentationEntryImpl(String url, WebXrVrTestFramework framework) {
        framework.loadUrlAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        framework.enterSessionWithUserGestureOrFail();
        Assert.assertTrue("Browser did not enter VR", VrShellDelegate.isInVr());

        // Verify that we're actually rendering WebXR/VR content and that it's blue (the canvas
        // is set to blue while presenting). This could be a proper RenderTest, but it's less
        // overhead to just directly check a pixel.
        // TODO(https://crbug.com/947252): Run this part unconditionally once the cause of the
        // flakiness on older devices is fixed.
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.M) {
            CriteriaHelper.pollInstrumentationThread(
                    ()
                            -> {
                        // Creating temporary directories doesn't seem to work, so use a fixed
                        // location that we know we can write to.
                        File dumpDirectory = new File(UrlUtils.getIsolatedTestFilePath(
                                "chrome/test/data/vr/framebuffer_dumps"));
                        if (!dumpDirectory.exists() && !dumpDirectory.isDirectory()) {
                            Assert.assertTrue("Failed to make framebuffer dump directory",
                                    dumpDirectory.mkdirs());
                        }
                        File baseImagePath = new File(dumpDirectory, "dump");
                        NativeUiUtils.dumpNextFramesFrameBuffers(baseImagePath.getPath());
                        String filepath = baseImagePath.getPath()
                                + NativeUiUtils.FRAME_BUFFER_SUFFIX_WEB_XR_CONTENT + ".png";
                        BitmapFactory.Options options = new BitmapFactory.Options();
                        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
                        Bitmap bitmap = BitmapFactory.decodeFile(filepath, options);
                        return bitmap != null && Color.BLUE == bitmap.getPixel(0, 0);
                    },
                    "Immersive session started, but browser not visibly in VR",
                    POLL_TIMEOUT_LONG_MS, POLL_CHECK_INTERVAL_LONG_MS);
        }

        framework.assertNoJavaScriptErrors();
    }

    /**
     * Tests that WebXR is not exposed if the flag is not on and the page does
     * not have an origin trial token.
     */
    @Test
    @MediumTest
    @CommandLineFlags
            .Add({"disable-features=WebXR"})
            @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
            public void testWebXrDisabledWithoutFlagSet() {
        apiDisabledWithoutFlagSetImpl(WebXrVrTestFramework.getFileUrlForHtmlTestFile(
                                              "test_webxr_disabled_without_flag_set"),
                mWebXrVrTestFramework);
    }

    private void apiDisabledWithoutFlagSetImpl(String url, WebXrVrTestFramework framework) {
        framework.loadUrlAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        framework.waitOnJavaScriptStep();
        framework.endTest();
    }

    /**
     * Tests that the immersive session promise doesn't resolve if the DON flow is
     * not completed.
     */
    @Test
    @MediumTest
    @Restriction({RESTRICTION_TYPE_DEVICE_DAYDREAM, RESTRICTION_TYPE_VR_SETTINGS_SERVICE})
    @VrSettingsFile(VrSettingsServiceUtils.FILE_DDVIEW_DONENABLED)
            @CommandLineFlags.Add({"enable-features=WebXR"})
            @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
            public void testPresentationPromiseUnresolvedDuringDon_WebXr() {
        presentationPromiseUnresolvedDuringDonImpl(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile(
                        "webxr_test_presentation_promise_unresolved_during_don"),
                mWebXrVrTestFramework);
    }

    private void presentationPromiseUnresolvedDuringDonImpl(
            String url, WebXrVrTestFramework framework) {
        framework.loadUrlAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        framework.enterSessionWithUserGestureAndWait();
        framework.endTest();
    }

    /**
     * Tests that the immersive session promise is rejected if the DON flow is canceled.
     */
    @Test
    @MediumTest
    @Restriction({RESTRICTION_TYPE_DEVICE_DAYDREAM, RESTRICTION_TYPE_VR_SETTINGS_SERVICE})
    @VrSettingsFile(VrSettingsServiceUtils.FILE_DDVIEW_DONENABLED)
            @CommandLineFlags.Add({"enable-features=WebXR"})
            @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
            public void testPresentationPromiseRejectedIfDonCanceled_WebXr() {
        presentationPromiseRejectedIfDonCanceledImpl(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile(
                        "webxr_test_presentation_promise_rejected_if_don_canceled"),
                mWebXrVrTestFramework);
    }

    private void presentationPromiseRejectedIfDonCanceledImpl(
            String url, WebXrVrTestFramework framework) {
        framework.loadUrlAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        final UiDevice uiDevice =
                UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        framework.enterSessionWithUserGesture();
        // Wait until the DON flow appears to be triggered
        // TODO(bsheedy): Make this less hacky if there's ever an explicit way to check if the
        // DON flow is currently active https://crbug.com/758296
        CriteriaHelper.pollUiThread(() -> {
            return uiDevice.getCurrentPackageName().equals("com.google.vr.vrcore");
        }, "DON flow did not start", POLL_TIMEOUT_LONG_MS, POLL_CHECK_INTERVAL_SHORT_MS);
        uiDevice.pressBack();
        framework.waitOnJavaScriptStep();
        framework.endTest();
    }

    /**
     * Tests that the omnibox reappears after exiting an immersive session.
     */
    @Test
    @MediumTest
            @CommandLineFlags.Add({"enable-features=WebXR"})
            @Restriction(RESTRICTION_TYPE_SVR)
            public void testControlsVisibleAfterExitingVr_WebXr() throws InterruptedException {
        controlsVisibleAfterExitingVrImpl(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("generic_webxr_page"),
                mWebXrVrTestFramework);
    }

    private void controlsVisibleAfterExitingVrImpl(String url, final WebXrVrTestFramework framework)
            throws InterruptedException {
        framework.loadUrlAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        framework.enterSessionWithUserGestureOrFail();
        VrTransitionUtils.forceExitVr();
        // The hiding of the controls may only propagate after VR has exited, so give it a chance
        // to propagate. In the worst case this test will erroneously pass, but should never
        // erroneously fail, and should only be flaky if omnibox showing is broken.
        Thread.sleep(100);
        CriteriaHelper.pollUiThread(
                ()
                        -> {
                    ChromeActivity activity = framework.getRule().getActivity();
                    return activity.getFullscreenManager().getBrowserControlHiddenRatio() == 0.0;
                },
                "Browser controls did not unhide after exiting VR", POLL_TIMEOUT_SHORT_MS,
                POLL_CHECK_INTERVAL_SHORT_MS);
        framework.assertNoJavaScriptErrors();
    }

    /**
     * Tests that window.requestAnimationFrame stops firing while in a WebXR immersive session, but
     * resumes afterwards.
     */
    @Test
    @MediumTest
            @CommandLineFlags.Add({"enable-features=WebXR"})
            @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
            public void testWindowRafStopsFiringWhilePresenting_WebXr()
            throws InterruptedException {
        windowRafStopsFiringWhilePresentingImpl(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile(
                        "webxr_test_window_raf_stops_firing_during_immersive_session"),
                mWebXrVrTestFramework);
    }

    private void windowRafStopsFiringWhilePresentingImpl(String url, WebXrVrTestFramework framework)
            throws InterruptedException {
        framework.loadUrlAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        framework.executeStepAndWait("stepVerifyBeforePresent()");
        // Pausing of window.rAF is done asynchronously, so wait until that's done.
        final CountDownLatch vsyncPausedLatch = new CountDownLatch(1);
        TestVrShellDelegate.getInstance().setVrShellOnVSyncPausedCallback(
                () -> { vsyncPausedLatch.countDown(); });
        framework.enterSessionWithUserGestureOrFail();
        vsyncPausedLatch.await(POLL_TIMEOUT_SHORT_MS, TimeUnit.MILLISECONDS);
        framework.executeStepAndWait("stepVerifyDuringPresent()");
        VrTransitionUtils.forceExitVr();
        framework.executeStepAndWait("stepVerifyAfterPresent()");
        framework.endTest();
    }

    /**
     * Tests renderer crashes while in WebXR presentation stay in VR.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE)
            @CommandLineFlags.Add({"enable-features=WebXR"})
            public void testRendererKilledInWebXrStaysInVr() throws IllegalArgumentException {
        rendererKilledInVrStaysInVrImpl(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("generic_webxr_page"),
                mWebXrVrTestFramework);
    }

    private void rendererKilledInVrStaysInVrImpl(String url, WebXrVrTestFramework framework) {
        framework.loadUrlAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        framework.enterSessionWithUserGestureOrFail();
        framework.simulateRendererKilled();
        Assert.assertTrue("Browser did not enter VR", VrShellDelegate.isInVr());
        framework.assertNoJavaScriptErrors();
    }

    /**
     * Tests that window.rAF continues to fire when we have a non-immersive session.
     */
    @Test
    @MediumTest
            @CommandLineFlags.Add({"enable-features=WebXR"})
            @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
            public void testWindowRafFiresDuringNonImmersiveSession() {
        mWebXrVrTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile(
                        "test_window_raf_fires_during_non_immersive_session"),
                PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.waitOnJavaScriptStep();
        mWebXrVrTestFramework.endTest();
    }

    /**
     * Tests that non-immersive sessions stop receiving rAFs during an immersive session, but resume
     * once the immersive session ends.
     */
    @Test
    @MediumTest
            @CommandLineFlags.Add({"enable-features=WebXR"})
            @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
            @DisableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF)
            public void testNonImmersiveStopsDuringImmersive() {
        mWebXrVrTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile(
                        "test_non_immersive_stops_during_immersive"),
                PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.executeStepAndWait("stepBeforeImmersive()");
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrVrTestFramework.executeStepAndWait("stepDuringImmersive()");
        VrTransitionUtils.forceExitVr();
        mWebXrVrTestFramework.executeStepAndWait("stepAfterImmersive()");
        mWebXrVrTestFramework.endTest();
    }

    /**
     * Tests that the "Press app button to exit" toast appears when entering an immersive WebXR for
     * VR session with Daydream View paired.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE)
            @CommandLineFlags.Add({"enable-features=WebXR"})
            @XrActivityRestriction({XrActivityRestriction.SupportedActivity.CTA})
            public void testAppButtonExitToast() {
        mWebXrVrTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("generic_webxr_page"),
                PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.APP_BUTTON_EXIT_TOAST, true /* visible*/, () -> {});
    }

    /**
     * Tests that a consent dialog dismisses by itself when the page navigates away from
     * the current page.
     */
    @Test
    @MediumTest
            @CommandLineFlags.Add({"enable-features=WebXR"})
            @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
            public void testConsentDialogIsDismissedWhenPageNavigatesAwayInMainFrame() {
        mWebXrVrTestFramework.setConsentDialogAction(
                WebXrVrTestFramework.CONSENT_DIALOG_ACTION_DO_NOTHING);
        mWebXrVrTestFramework.loadUrlAndAwaitInitialization(
                WebXrVrTestFramework.getFileUrlForHtmlTestFile("generic_webxr_page"),
                PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.enterSessionWithUserGesture();
        mWebXrVrTestFramework.runJavaScriptOrFail(
                "window.location.href = 'https://google.com'", POLL_TIMEOUT_SHORT_MS);
        PermissionUtils.waitForConsentPromptDismissal(
                mWebXrVrTestFramework.getRule().getActivity());
    }
}
