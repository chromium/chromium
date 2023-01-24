// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_NON_DAYDREAM;

import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.filters.MediumTest;

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
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.VrTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;

import java.util.List;
import java.util.Locale;
import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * End-to-end tests for sending input while using WebXR.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
// TODO(crbug.com/1192004): Remove --allow-pre-commit-input once the root cause of the
// failures has been fixed.
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=LogJsConsoleMessages", "allow-pre-commit-input"})
public class WebXrVrInputTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            VrTestRuleUtils.generateDefaultTestRuleParameters();
    @Rule
    public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrVrTestFramework mWebXrVrTestFramework;

    public WebXrVrInputTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = VrTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrVrTestFramework = new WebXrVrTestFramework(mTestRule);
    }

    private void assertAppButtonEffect(boolean shouldHaveExited, WebXrVrTestFramework framework) {
        Assert.assertEquals("App button effect matched expectation", shouldHaveExited,
                mWebXrVrTestFramework.pollJavaScriptBoolean(
                        "sessionInfos[sessionTypes.IMMERSIVE].currentSession == null",
                        POLL_TIMEOUT_SHORT_MS));
    }

    /**
     * Tests that screen touches are not registered when in an immersive session. Disabled on
     * standalones because they don't have touchscreens.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @DisabledTest(message = "https://crbug.com/1409794")
    public void testScreenTapsNotRegistered_WebXr() throws InterruptedException {
        screenTapsNotRegisteredImpl("webxr_test_screen_taps_not_registered", mWebXrVrTestFramework);
    }

    private void screenTapsNotRegisteredImpl(String url, final WebXrVrTestFramework framework)
            throws InterruptedException {
        framework.loadFileAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        framework.executeStepAndWait("stepVerifyNoInitialTaps()");
        framework.enterSessionWithUserGestureOrFail();
        // Wait on VrShell to say that its parent consumed the touch event.
        // Set to 2 because there's an ACTION_DOWN followed by ACTION_UP
        final CountDownLatch touchRegisteredLatch = new CountDownLatch(2);
        TestVrShellDelegate.getVrShellForTesting().setOnDispatchTouchEventForTesting(
                new OnDispatchTouchEventCallback() {
                    @Override
                    public void onDispatchTouchEvent(boolean parentConsumed) {
                        if (!parentConsumed) Assert.fail("Parent did not consume event");
                        touchRegisteredLatch.countDown();
                    }
                });
        TouchCommon.singleClickView(mTestRule.getActivity().getWindow().getDecorView());
        Assert.assertTrue("VrShell did not dispatch touches",
                touchRegisteredLatch.await(POLL_TIMEOUT_LONG_MS * 10, TimeUnit.MILLISECONDS));
        framework.executeStepAndWait("stepVerifyNoAdditionalTaps()");
        framework.endTest();
    }

    /**
     * Tests that Daydream controller clicks are registered as XR input in an immersive session.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @CommandLineFlags.Add({"enable-features=WebXR"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testControllerClicksRegisteredOnDaydream_WebXr() {
        EmulatedVrController controller = new EmulatedVrController(mTestRule.getActivity());
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "test_webxr_input", PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();

        int numIterations = 10;
        mWebXrVrTestFramework.runJavaScriptOrFail(
                "stepSetupListeners(" + String.valueOf(numIterations) + ")", POLL_TIMEOUT_SHORT_MS);

        // Click the touchpad a bunch of times and make sure they're all registered.
        for (int i = 0; i < numIterations; i++) {
            controller.sendClickButtonToggleEvent();
            controller.sendClickButtonToggleEvent();
            // The controller emulation can sometimes deliver controller input at weird times such
            // that we only register 8 or 9 of the 10 press/release pairs. So, send a press/release
            // and wait for it to register before doing another.
            mWebXrVrTestFramework.waitOnJavaScriptStep();
        }

        mWebXrVrTestFramework.endTest();
    }

    /**
     * Tests that Daydream controller is exposed as a Gamepad on an
     * XRInputSource in an immersive session and that button clicks and touchpad
     * movements are registered.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @CommandLineFlags.Add({"enable-features=WebXR"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testControllerExposedAsGamepadOnDaydream_WebXr() {
        EmulatedVrController controller = new EmulatedVrController(mTestRule.getActivity());
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "test_webxr_gamepad_support", PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();

        // There must be interaction with the controller before an XRInputSource
        // is recognized. Wait for JS to register the select event to avoid a
        // race condition.
        mWebXrVrTestFramework.runJavaScriptOrFail("stepSetupListeners()", POLL_TIMEOUT_SHORT_MS);
        controller.sendClickButtonToggleEvent();
        controller.sendClickButtonToggleEvent();
        mWebXrVrTestFramework.pollJavaScriptBooleanOrFail("selectCount > 0", POLL_TIMEOUT_SHORT_MS);

        // Daydream controller should not be 'xr-standard' mapping since it does
        // not meet the requirements.
        mWebXrVrTestFramework.pollJavaScriptBooleanOrFail(
                "isMappingEqualTo('')", POLL_TIMEOUT_SHORT_MS);

        // Daydream controller should only expose a single button and set of
        // input axes (the pressable touchpad). It should not expose any of the
        // platform buttons such as "home" or "back".
        mWebXrVrTestFramework.pollJavaScriptBooleanOrFail(
                "isButtonCountEqualTo(1)", POLL_TIMEOUT_SHORT_MS);
        mWebXrVrTestFramework.pollJavaScriptBooleanOrFail(
                "isAxisPairCountEqualTo(1)", POLL_TIMEOUT_SHORT_MS);

        // Initially, the touchpad should not be touched.
        validateTouchpadNotTouched();

        // Make sure pressing the touchpad button works.
        controller.sendClickButtonToggleEvent();
        mWebXrVrTestFramework.pollJavaScriptBooleanOrFail(
                "isButtonPressedEqualTo(0, true)", POLL_TIMEOUT_SHORT_MS);
        controller.sendClickButtonToggleEvent();
        mWebXrVrTestFramework.pollJavaScriptBooleanOrFail(
                "isButtonPressedEqualTo(0, false)", POLL_TIMEOUT_SHORT_MS);

        // Make sure setting the touchpad position works.
        setAndValidateTouchpadPosition(controller, 0.0f, 1.0f);
        setAndValidateTouchpadPosition(controller, 1.0f, 0.0f);
        setAndValidateTouchpadPosition(controller, 0.5f, 0.5f);

        controller.stopTouchingTouchpad(0.5f, 0.5f);
        validateTouchpadNotTouched();

        mWebXrVrTestFramework.runJavaScriptOrFail("done()", POLL_TIMEOUT_SHORT_MS);
        mWebXrVrTestFramework.endTest();
    }

    // When touchpad is not touched, it should not be reported as pressed
    // either. The input axis values should be at the origin: (0, 0).
    private void validateTouchpadNotTouched() {
        mWebXrVrTestFramework.pollJavaScriptBooleanOrFail(
                "isButtonTouchedEqualTo(0, false)", POLL_TIMEOUT_SHORT_MS);
        mWebXrVrTestFramework.pollJavaScriptBooleanOrFail(
                "isButtonPressedEqualTo(0, false)", POLL_TIMEOUT_SHORT_MS);
        mWebXrVrTestFramework.pollJavaScriptBooleanOrFail(
                "areAxesValuesEqualTo(0, 0.0, 0.0)", POLL_TIMEOUT_SHORT_MS);
    }

    // Device code reports touchpad position in range [0.0, 1.0].
    // WebXR reports Gamepad input axis position in range [-1.0, 1.0].
    private float rawToWebXRTouchpadPosition(float raw) {
        return (raw * 2.0f) - 1.0f;
    }

    private void setAndValidateTouchpadPosition(EmulatedVrController controller, float x, float y) {
        controller.setTouchpadPosition(x, y);
        float xExpected = rawToWebXRTouchpadPosition(x);
        float yExpected = rawToWebXRTouchpadPosition(y);
        String js =
                String.format(Locale.US, "areAxesValuesEqualTo(0, %f, %f)", xExpected, yExpected);
        mWebXrVrTestFramework.pollJavaScriptBooleanOrFail(js, POLL_TIMEOUT_SHORT_MS);
        mWebXrVrTestFramework.pollJavaScriptBooleanOrFail(
                "isButtonTouchedEqualTo(0, true)", POLL_TIMEOUT_SHORT_MS);
    }

    private long sendScreenTouchDown(final View view, final int x, final int y) {
        long downTime = SystemClock.uptimeMillis();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            view.dispatchTouchEvent(
                    MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN, x, y, 0));
        });
        return downTime;
    }

    private void sendScreenTouchUp(final View view, final int x, final int y, final long downTime) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            long now = SystemClock.uptimeMillis();
            view.dispatchTouchEvent(
                    MotionEvent.obtain(downTime, now, MotionEvent.ACTION_UP, x, y, 0));
        });
    }

    private void spamScreenTaps(final View view, final int x, final int y, final int iterations) {
        // Tap the screen a bunch of times.
        // Android doesn't seem to like sending touch events too quickly, so have a short delay
        // between events.
        for (int i = 0; i < iterations; i++) {
            sendScreenTap(view, x, y);
        }
    }

    private void sendScreenTap(final View view, final int x, final int y) {
        long downTime = sendScreenTouchDown(view, x, y);
        SystemClock.sleep(100);
        sendScreenTouchUp(view, x, y, downTime);
        SystemClock.sleep(100);
    }

    /**
     * Tests that screen touches are registered as XR input in immersive sessions,
     *  when the viewer is Cardboard.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_NON_DAYDREAM)
    @CommandLineFlags.Add({"enable-features=WebXR"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testScreenTapsRegisteredOnCardboard_WebXr() {
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "test_webxr_input", PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();

        // TODO(mthiesse, https://crbug.com/758374): Injecting touch events into the root GvrLayout
        // (VrShell) is flaky. Sometimes the events just don't get routed to the presentation
        // view for no apparent reason. We should figure out why this is and see if it's fixable.
        // Note that we only use 5 iterations, vs the 10 used in the inline test, because it seems
        // that on M, we get enough memory pressure that the yet-to-be-processed taps get GC'd.
        // See https://crbug.com/1194906 for more details.
        testScreenTapsRegisteredOnCardboardImpl(
                TestVrShellDelegate.getVrShellForTesting().getPresentationViewForTesting(), 5);
    }

    /**
     * Tests that screen touches are registered as transient XR input in inline sessions,
     * when the viewer is Cardboard.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_NON_DAYDREAM)
    @CommandLineFlags.Add({"enable-features=WebXR"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testTransientScreenTapsRegisteredOnCardboard_WebXr() {
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "test_webxr_transient_input", PAGE_LOAD_TIMEOUT_S);

        testScreenTapsRegisteredOnCardboardImpl(mWebXrVrTestFramework.getCurrentContentView(), 10);
    }

    // Note that the page should load the appropriate test page and enter any relevant session
    // before calling this.
    private void testScreenTapsRegisteredOnCardboardImpl(View presentationView, int numIterations) {
        mWebXrVrTestFramework.runJavaScriptOrFail(
                "stepSetupListeners(" + String.valueOf(numIterations) + ")", POLL_TIMEOUT_SHORT_MS);

        int x = presentationView.getWidth() / 2;
        int y = presentationView.getHeight() / 2;

        // Tap the screen a bunch of times and make sure that they're all registered. Ideally, we
        // shouldn't have to ack each one, but it's possible for inputs to get eaten by garbage
        // collection if there are multiple in flight, so only send one at a time.
        for (int i = 0; i < numIterations; i++) {
            sendScreenTap(presentationView, x, y);
            mWebXrVrTestFramework.waitOnJavaScriptStep();
        }

        mWebXrVrTestFramework.endTest();
    }

    /**
     * Tests that focus is locked to the device with an immersive session for the purposes of
     * VR input.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=WebXR"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testPresentationLocksFocus_WebXr() {
        presentationLocksFocusImpl("webxr_test_presentation_locks_focus", mWebXrVrTestFramework);
    }

    private void presentationLocksFocusImpl(String url, WebXrVrTestFramework framework) {
        framework.loadFileAndAwaitInitialization(url, PAGE_LOAD_TIMEOUT_S);
        framework.enterSessionWithUserGestureOrFail();
        framework.executeStepAndWait("stepSetupFocusLoss()");
        framework.endTest();
    }

    /**
     * Verifies that a Gamepad API gamepad is returned on the XRSession's input
     * source instead of the navigator array when using WebXR and a Daydream
     * headset.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    @CommandLineFlags.Add({"enable-features=WebXR"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testWebXrInputSourceHasGamepad() {
        webxrGamepadSupportImpl(true /* daydream */);
    }

    /**
     * Verifies that the XRSession has an input source when using WebXR and
     * Cardboard. There should be no gamepads on the input source or navigator
     * array.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_NON_DAYDREAM)
    @CommandLineFlags.Add({"enable-features=WebXR"})
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    public void testWebXrInputSourceWithoutGamepad_Cardboard() {
        webxrGamepadSupportImpl(false /* daydream */);
    }

    private void webxrGamepadSupportImpl(boolean daydream) {
        mWebXrVrTestFramework.loadFileAndAwaitInitialization(
                "test_webxr_gamepad_support", PAGE_LOAD_TIMEOUT_S);
        mWebXrVrTestFramework.enterSessionWithUserGestureOrFail();

        // Spam input to make sure the Gamepad API registers the gamepad if it should.
        int numIterations = 10;
        if (daydream) {
            EmulatedVrController controller = new EmulatedVrController(mTestRule.getActivity());
            for (int i = 0; i < numIterations; i++) {
                controller.performControllerClick();
            }

            // Verify that there is a gamepad on the XRInputSource and that it
            // has the expected mapping of '' (the Daydream controller does not
            // meet the 'xr-standard' mapping requirements).
            mWebXrVrTestFramework.pollJavaScriptBooleanOrFail(
                    "isMappingEqualTo('')", POLL_TIMEOUT_SHORT_MS);
        } else {
            int x = mWebXrVrTestFramework.getCurrentContentView().getWidth() / 2;
            int y = mWebXrVrTestFramework.getCurrentContentView().getHeight() / 2;
            View presentationView =
                    TestVrShellDelegate.getVrShellForTesting().getPresentationViewForTesting();
            spamScreenTaps(presentationView, x, y, numIterations);

            mWebXrVrTestFramework.pollJavaScriptBooleanOrFail(
                    "inputSourceHasNoGamepad()", POLL_TIMEOUT_SHORT_MS);
        }

        mWebXrVrTestFramework.runJavaScriptOrFail("done()", POLL_TIMEOUT_SHORT_MS);
        mWebXrVrTestFramework.endTest();
    }
}
