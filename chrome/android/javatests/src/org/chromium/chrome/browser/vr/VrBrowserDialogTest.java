// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE;

import android.graphics.PointF;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr.rules.HeadTrackingMode;
import org.chromium.chrome.browser.vr.util.NativeUiUtils;
import org.chromium.chrome.browser.vr.util.VrBrowserTransitionUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.File;
import java.util.concurrent.TimeoutException;

/**
 * End-to-End test for capturing and comparing screen images for VR Browsering Dialogs
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE)
public class VrBrowserDialogTest {
    // A long enough sleep after entering VR to ensure that the VR entry animations are complete.
    private static final int VR_ENTRY_SLEEP_MS = 1000;
    // We need to make sure the port is constant, otherwise the URL changes between test runs, which
    // is really bad for image diff tests. There's nothing special about this port other than that
    // it shouldn't be in use by anything.
    private static final int SERVER_PORT = 39558;
    // A long enough sleep after triggering/interacting with a dialog to ensure that the interaction
    // has propagated through the render pipeline, i.e. the result of the interaction will actually
    // be visible on the screen.
    private static final String TEST_IMAGE_DIR = "chrome/test/data/vr/UiCapture";
    private static final File sBaseDirectory =
            new File(UrlUtils.getIsolatedTestFilePath(TEST_IMAGE_DIR));

    // We explicitly instantiate a rule here instead of using parameterization since this class
    // only ever runs in ChromeTabbedActivity.
    @Rule
    public ChromeTabbedActivityVrTestRule mVrTestRule = new ChromeTabbedActivityVrTestRule();

    private VrBrowserTestFramework mVrBrowserTestFramework;
    private EmbeddedTestServer mServer;

    @Before
    public void setUp() throws Exception {
        mVrBrowserTestFramework = new VrBrowserTestFramework(mVrTestRule);

        // Create UiCapture image directory.
        if (!sBaseDirectory.exists() && !sBaseDirectory.isDirectory()) {
            Assert.assertTrue("Failed to make image capture directory", sBaseDirectory.mkdirs());
        }
    }

    @After
    public void tearDown() throws Exception {
        if (mServer != null) {
            mServer.stopAndDestroyServer();
        }
    }

    private void captureScreen(String filename) throws InterruptedException {
        // Ensure that any UI changes that have been rendered and submitted have actually propogated
        // to the screen.
        NativeUiUtils.waitNumFrames(2);
        File baseFilename = new File(sBaseDirectory, filename);
        NativeUiUtils.dumpNextFramesFrameBuffers(baseFilename.getPath());
    }

    private void navigateAndDisplayPermissionPrompt(String page, String promptCommand)
            throws InterruptedException, TimeoutException {
        // Trying to grant permissions on file:// URLs ends up hitting DCHECKS, so load from a local
        // server instead.
        if (mServer == null) {
            mServer = EmbeddedTestServer.createAndStartServerWithPort(
                    InstrumentationRegistry.getContext(), SERVER_PORT);
        }
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                mServer.getURL(VrBrowserTestFramework.getEmbeddedServerPathForHtmlTestFile(page)),
                PAGE_LOAD_TIMEOUT_S);

        // Display the given permission prompt.
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        NativeUiUtils.enableMockedInput();
        mVrBrowserTestFramework.runJavaScriptOrFail(promptCommand, POLL_TIMEOUT_LONG_MS);
        VrBrowserTransitionUtils.waitForNativeUiPrompt(POLL_TIMEOUT_LONG_MS);

        // There is currently no way to know whether a dialog has been drawn yet,
        // so sleep long enough for it to show up.
        Thread.sleep(VR_ENTRY_SLEEP_MS);
    }

    private void navigateAndDisplayJavaScriptDialog(String page, String dialogCommand)
            throws InterruptedException, TimeoutException {
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile(page), PAGE_LOAD_TIMEOUT_S);

        // Display the JavaScript dialog.
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        NativeUiUtils.enableMockedInput();
        // We can't use runJavaScriptOrFail here because JavaScript execution is blocked while a
        // JS dialog is visible, so runJavaScriptOrFail will always time out.
        JavaScriptUtils.executeJavaScript(
                mVrBrowserTestFramework.getFirstTabWebContents(), dialogCommand);
        VrBrowserTransitionUtils.waitForNativeUiPrompt(POLL_TIMEOUT_LONG_MS);

        // There is currently no way to know whether a dialog has been drawn yet,
        // so sleep long enough for it to show up.
        Thread.sleep(VR_ENTRY_SLEEP_MS);
    }

    private void clickElement(String initialPage, int elementName)
            throws InterruptedException, TimeoutException {
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile(initialPage), PAGE_LOAD_TIMEOUT_S);
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        Thread.sleep(VR_ENTRY_SLEEP_MS);
        NativeUiUtils.clickElementAndWaitForUiQuiescence(elementName, new PointF(0, 0));
    }

    /**
     * Test navigate to 2D page and launch the Microphone dialog.
     */
    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testMicrophonePermissionPrompt() throws InterruptedException, TimeoutException {
        // Display audio permissions prompt.
        navigateAndDisplayPermissionPrompt(
                "test_navigation_2d_page", "navigator.getUserMedia({audio: true}, ()=>{}, ()=>{})");

        // Capture image
        captureScreen("MicrophonePermissionPrompt_Visible");
        NativeUiUtils.clickFallbackUiPositiveButton();
        captureScreen("MicrophonePermissionPrompt_Granted");
        mVrBrowserTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Test navigate to 2D page and launch the Camera dialog. Not valid on standalones because
     * there is no camera permission.
     */
    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testCameraPermissionPrompt() throws InterruptedException, TimeoutException {
        // Display Camera permissions prompt.
        navigateAndDisplayPermissionPrompt(
                "test_navigation_2d_page", "navigator.getUserMedia({video: true}, ()=>{}, ()=>{})");

        // Capture image
        captureScreen("CameraPermissionPrompt_Visible");
        mVrBrowserTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Test navigate to 2D page and launch the Location dialog.
     */
    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testLocationPermissionPrompt() throws InterruptedException, TimeoutException {
        // Display Location permissions prompt.
        navigateAndDisplayPermissionPrompt("test_navigation_2d_page",
                "navigator.geolocation.getCurrentPosition(()=>{}, ()=>{})");

        // Capture image
        captureScreen("LocationPermissionPrompt_Visible");
        mVrBrowserTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Test navigate to 2D page and launch the Notifications dialog.
     */
    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testNotificationPermissionPrompt() throws InterruptedException, TimeoutException {
        // Display Notification permissions prompt.
        navigateAndDisplayPermissionPrompt(
                "test_navigation_2d_page", "Notification.requestPermission(()=>{})");

        // Capture image
        captureScreen("NotificationPermissionPrompt_Visible");
        mVrBrowserTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Test navigate to 2D page and launch the MIDI dialog.
     */
    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testMidiPermisionPrompt() throws InterruptedException, TimeoutException {
        // Display MIDI permissions prompt.
        navigateAndDisplayPermissionPrompt(
                "test_navigation_2d_page", "navigator.requestMIDIAccess({sysex: true})");

        // Capture image
        captureScreen("MidiPermissionPrompt_Visible");
        mVrBrowserTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Test navigate to 2D page and display a JavaScript alert().
     */
    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testJavaScriptAlert() throws InterruptedException, TimeoutException {
        // Display a JavaScript alert()
        navigateAndDisplayJavaScriptDialog(
                "test_navigation_2d_page", "alert('538 perf regressions detected')");

        // Capture image
        captureScreen("JavaScriptAlert_Visible");
        // No assertNoJavaScriptErrors since the alert is still visible, preventing further
        // JavaScript execution.
    }

    /**
     * Test navigate to 2D page and display a JavaScript confirm();
     */
    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testJavaScriptConfirm() throws InterruptedException, TimeoutException {
        // Display a JavaScript confirm()
        navigateAndDisplayJavaScriptDialog(
                "test_navigation_2d_page", "var c = confirm('This is a confirmation dialog')");

        // Capture image
        captureScreen("JavaScriptConfirm_Visible");

        NativeUiUtils.clickFallbackUiNegativeButton();
        // Ensure the cancel button was clicked.
        Assert.assertTrue("JavaScript Confirm's cancel button was not clicked",
                mVrBrowserTestFramework.runJavaScriptOrFail("c", POLL_TIMEOUT_SHORT_MS)
                        .equals("false"));
        captureScreen("JavaScriptConfirm_Dismissed");
        mVrBrowserTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Test navigate to 2D page and display a JavaScript prompt(). Then confirm that it behaves as
     * it would outside of VR.
     */
    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testJavaScriptPrompt() throws InterruptedException, TimeoutException {
        // Display a JavaScript prompt()
        String expectedString = "Probably most likely yes";
        navigateAndDisplayJavaScriptDialog("test_navigation_2d_page",
                "var p = prompt('Are the Chrome controls broken?', '" + expectedString + "')");

        // Capture image
        captureScreen("JavaScriptPrompt_Visible");
        NativeUiUtils.clickFallbackUiPositiveButton();
        // This JavaScript will only run once the prompt has been dismissed, and the return value
        // will only be what we expect if the positive button was actually clicked (as opposed to
        // canceled).
        Assert.assertTrue("JavaScript Prompt's OK button was not clicked",
                mVrBrowserTestFramework
                        .runJavaScriptOrFail("p == '" + expectedString + "'", POLL_TIMEOUT_SHORT_MS)
                        .equals("true"));
        captureScreen("JavaScriptPrompt_Dismissed");
        mVrBrowserTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Tests that the keyboard appears when clicking on the URL bar.
     * Also contains a regression test for https://crbug.com/874671 where inputting text into the
     * URL bar would cause a browser crash.
     */
    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testKeyboardAppearsOnUrlBarClick() throws InterruptedException, TimeoutException {
        clickElement("test_navigation_2d_page", UserFriendlyElementName.URL);
        captureScreen("KeyboardAppearsOnUrlBarClick_Visible");
        // Regression test for https://crbug.com/874671
        // We need to use the VrCore-side emulated controller because the keyboard isn't a UI
        // element, meaning we can't specify it as a click target for the Chrome-side controller.
        // We also can't use the MockBrowserKeyboardInterface like we do for web input testing, as
        // that does not seem to work with the omnibox.
        NativeUiUtils.revertToRealInput();
        // Point at the keyboard and click an arbitrary key
        EmulatedVrController controller = new EmulatedVrController(mVrTestRule.getActivity());
        controller.recenterView();
        controller.moveControllerInstant(0.0f, -0.259f, -0.996f, -0.0f);
        // Spam clicks to ensure we're getting one in.
        for (int i = 0; i < 5; i++) {
            controller.performControllerClick();
        }
        mVrBrowserTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Tests that the overflow menu appears when the overflow menu button is clicked.
     */
    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testOverflowMenuAppears() throws InterruptedException, TimeoutException {
        clickElement("test_navigation_2d_page", UserFriendlyElementName.OVERFLOW_MENU);
        captureScreen("OverflowMenuAppears_Visible");
        mVrBrowserTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Tests that the page info popup appears when the security token in the URL bar is clicked.
     */
    @Test
    @LargeTest
    @HeadTrackingMode(HeadTrackingMode.SupportedMode.FROZEN)
    public void testPageInfoAppearsOnSecurityTokenClick()
            throws InterruptedException, TimeoutException {
        clickElement("test_navigation_2d_page", UserFriendlyElementName.PAGE_INFO_BUTTON);
        captureScreen("PageInfoAppearsOnSecurityTokenClick_Visible");
        mVrBrowserTestFramework.assertNoJavaScriptErrors();
    }
}
