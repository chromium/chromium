// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.WebXrArTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;

import android.os.Build;

import androidx.test.filters.MediumTest;

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
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.vr.rules.ArPlaybackFile;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.ArTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;

import java.util.List;
import java.util.concurrent.Callable;

/** End-to-end tests for testing WebXR for AR's camera access behavior. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
// TODO(crbug.com/40709670) Change this to Build.VERSION_CODES.N once N support is added.
@MinAndroidSdkLevel(Build.VERSION_CODES.O)
public class WebXrArCameraAccessTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            ArTestRuleUtils.generateDefaultTestRuleParameters();

    @Rule public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrArTestFramework mWebXrArTestFramework;

    public WebXrArCameraAccessTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = ArTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrArTestFramework = new WebXrArTestFramework(mTestRule);
    }

    /** Test that getCameraImage(...) returns a valid GLTexture. */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=WebXRIncubations,LogJsConsoleMessages"
    })
    @ArPlaybackFile("chrome/test/data/xr/ar_playback_datasets/floor_session_12s_30fps.mp4")
    public void testCameraAccessImageTextureNotNull() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "webxr_test_camera_access", PAGE_LOAD_TIMEOUT_S);
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail(/* needsCameraPermission= */ true);
        mWebXrArTestFramework.runJavaScriptOrFail(
                "stepStartStoringCameraTexture(1)", POLL_TIMEOUT_SHORT_MS);
        mWebXrArTestFramework.waitOnJavaScriptStep();
        mWebXrArTestFramework.endTest();
    }

    /** Test that multiple consecutive calls to getCameraImage(...) all return a valid GLTexture. */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=WebXRIncubations,LogJsConsoleMessages"
    })
    @ArPlaybackFile("chrome/test/data/xr/ar_playback_datasets/floor_session_12s_30fps.mp4")
    public void testConsecutiveCameraAccessImageTexturesNotNull() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "webxr_test_camera_access", PAGE_LOAD_TIMEOUT_S);
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail(/* needsCameraPermission= */ true);
        mWebXrArTestFramework.runJavaScriptOrFail(
                "stepStartStoringCameraTexture(3)", POLL_TIMEOUT_SHORT_MS);
        mWebXrArTestFramework.waitOnJavaScriptStep();
        mWebXrArTestFramework.endTest();
    }

    /**
     * Test that if a returned WebGL camera image texture is deleted by a site, an exception will
     * not be raised when the renderer deletes the texture during OnFrameEnd().
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=WebXRIncubations,LogJsConsoleMessages"
    })
    @ArPlaybackFile("chrome/test/data/xr/ar_playback_datasets/floor_session_12s_30fps.mp4")
    public void testCameraAccessImageTextureCanBeDeleted() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "webxr_test_camera_access", PAGE_LOAD_TIMEOUT_S);
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail(/* needsCameraPermission= */ true);
        mWebXrArTestFramework.runJavaScriptOrFail(
                "stepStartStoreAndDeleteCameraTexture()", POLL_TIMEOUT_SHORT_MS);
        mWebXrArTestFramework.waitOnJavaScriptStep();
        mWebXrArTestFramework.endTest();
    }

    /** Test the texture lifetime of camera image textures. */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=WebXRIncubations,LogJsConsoleMessages"
    })
    @ArPlaybackFile("chrome/test/data/xr/ar_playback_datasets/floor_session_12s_30fps.mp4")
    public void testCameraAccessImageTextureLifetime() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "webxr_test_camera_access", PAGE_LOAD_TIMEOUT_S);
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail(/* needsCameraPermission= */ true);
        mWebXrArTestFramework.runJavaScriptOrFail(
                "stepCheckCameraTextureLifetimeLimitedToOneFrame()", POLL_TIMEOUT_SHORT_MS);
        mWebXrArTestFramework.waitOnJavaScriptStep();
        mWebXrArTestFramework.endTest();
    }

    /** Test whether opaque texture enforcements work fine. */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=WebXRIncubations,LogJsConsoleMessages"
    })
    @ArPlaybackFile("chrome/test/data/xr/ar_playback_datasets/floor_session_12s_30fps.mp4")
    public void testOpaqueTextures() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "webxr_test_camera_access", PAGE_LOAD_TIMEOUT_S);
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail(/* needsCameraPermission= */ true);
        mWebXrArTestFramework.runJavaScriptOrFail(
                "stepCheckOpaqueTextures()", POLL_TIMEOUT_SHORT_MS);
        mWebXrArTestFramework.waitOnJavaScriptStep();
        mWebXrArTestFramework.endTest();
    }
}
