// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_CHECK_INTERVAL_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_CHECK_INTERVAL_SHORT_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE;

import android.graphics.PointF;
import android.os.SystemClock;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr.keyboard.TextEditAction;
import org.chromium.chrome.browser.vr.mock.MockBrowserKeyboardInterface;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr.util.NativeUiUtils;
import org.chromium.chrome.browser.vr.util.RenderTestUtils;
import org.chromium.chrome.browser.vr.util.VrBrowserTransitionUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;

import java.io.IOException;
import java.util.HashMap;
import java.util.concurrent.TimeoutException;

/**
 * End-to-end tests for interacting with HTML input elements on a webpage.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=LogJsConsoleMessages"})
@Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE)
public class VrBrowserWebInputEditingTest {
    // We explicitly instantiate a rule here instead of using parameterization since this class
    // only ever runs in ChromeTabbedActivity.
    @Rule
    public ChromeTabbedActivityVrTestRule mVrTestRule = new ChromeTabbedActivityVrTestRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("components/test/data/vr_browser_video/render_tests");

    private VrBrowserTestFramework mVrBrowserTestFramework;

    @Before
    public void setUp() {
        mVrBrowserTestFramework = new VrBrowserTestFramework(mVrTestRule);
    }

    /**
     * Verifies that when a web input field is focused, the VrInputMethodManagerWrapper is asked to
     * spawn the keyboard. Moreover, we verify that an edit sent to web contents via the
     * VrInputConnection updates indices on the VrInputMethodManagerWrapper.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add("enable-features=VrLaunchIntents")
    public void testWebInputFocus() throws InterruptedException {
        testWebInputFocusImpl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_web_input_editing"));
    }

    /**
     * Verifies the same thing as testWebInputFocus, but with the input box in a cross-origin
     * iframe.
     * Automation of a manual test in https://crbug.com/862153
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add("enable-features=VrLaunchIntents")
    public void testWebInputFocusIframe() throws InterruptedException {
        testWebInputFocusImpl(VrBrowserTestFramework.getFileUrlForHtmlTestFile(
                "test_web_input_editing_iframe_outer"));
    }

    private void testWebInputFocusImpl(String url) throws InterruptedException {
        mVrTestRule.loadUrl(url, PAGE_LOAD_TIMEOUT_S);
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);

        VrShell vrShell = TestVrShellDelegate.getVrShellForTesting();
        MockBrowserKeyboardInterface keyboard = new MockBrowserKeyboardInterface();
        vrShell.getInputMethodManagerWrapperForTesting().setBrowserKeyboardInterfaceForTesting(
                keyboard);

        // The webpage reacts to the first controller click by focusing its input field. Verify that
        // focus gain spawns the keyboard by clicking in the center of the page.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.CONTENT_QUAD, new PointF());
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> {
                    Boolean visible = keyboard.getLastKeyboardVisibility();
                    return visible != null && visible;
                },
                "Keyboard did not show from focusing a web input box", POLL_TIMEOUT_LONG_MS,
                POLL_CHECK_INTERVAL_SHORT_MS);

        // Add text to the input field via the input connection and verify that the keyboard
        // interface is called to update the indices.
        VrInputConnection ic = vrShell.getVrInputConnectionForTesting();
        TextEditAction[] edits = {new TextEditAction(TextEditActionType.COMMIT_TEXT, "i", 1)};
        ic.onKeyboardEdit(edits);
        // Inserting 'i' should move the cursor by one character and there should be no composition.
        MockBrowserKeyboardInterface.Indices expectedIndices =
                new MockBrowserKeyboardInterface.Indices(1, 1, -1, -1);
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> {
                    MockBrowserKeyboardInterface.Indices indices = keyboard.getLastIndices();
                    return indices == null ? false : indices.equals(expectedIndices);
                },
                "Inputting text did not move cursor the expected amount", POLL_TIMEOUT_LONG_MS,
                POLL_CHECK_INTERVAL_SHORT_MS);

        // The second click should result in a focus loss and should hide the keyboard.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.CONTENT_QUAD, new PointF());
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> {
                    Boolean visible = keyboard.getLastKeyboardVisibility();
                    return visible != null && !visible;
                },
                "Keyboard did not hide from unfocusing a web input box", POLL_TIMEOUT_LONG_MS,
                POLL_CHECK_INTERVAL_SHORT_MS);
    }

    /**
     * Tests that interacting with a <code><select></code> tag on a webpage brings up a working
     * Android selection dialog.
     */
    @Test
    @MediumTest
    public void testSelectTag() throws TimeoutException {
        mVrTestRule.loadUrl(VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_select_tag"),
                PAGE_LOAD_TIMEOUT_S);
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        NativeUiUtils.enableMockedInput();
        // Click on the <select> tag and wait for the resulting modal dialog to appear.
        DOMUtils.clickNode(mVrBrowserTestFramework.getCurrentWebContents(), "selectbox",
                false /* goThroughRootAndroidView */);
        NativeUiUtils.waitForModalDialogStatus(true /* shouldBeShown */, mVrTestRule.getActivity());
        // On fast devices such as the Vega, it's possible to send the first click before the modal
        // dialog starts accepting any input, causing the click to be registered on the web contents
        // and causing the dialog to be dismissed. So, ensure that the dialog is at least done
        // rendering before trying to send input.
        NativeUiUtils.waitForUiQuiescence();
        // Click on whichever option is near the center of the screen. We don't care which, as long
        // as it's not the initial selection, which should be at the top. Clicking in the exact
        // center can sometimes click in the area between two options, so offset slightly to prevent
        // flakes from that.
        // Most of the time, the first click will go through. However, it's possible to send the
        // click while the dialog is technically present, but not fully ready, resulting in the
        // click not registering. Since there doesn't seem to be a good way to wait for the dialog
        // to be ready, try several times.
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> {
                    if (!mVrTestRule.getActivity().getModalDialogManager().isShowing()) return true;
                    NativeUiUtils.clickElement(
                            UserFriendlyElementName.CONTENT_QUAD, new PointF(0f, 0.05f));
                    return false;
                },
                "Could not click on <select> modal dialog", POLL_TIMEOUT_LONG_MS,
                POLL_CHECK_INTERVAL_LONG_MS);

        // Wait on JavaScript to verify that a selection was made.
        mVrBrowserTestFramework.waitOnJavaScriptStep();
        mVrBrowserTestFramework.endTest();
    }

    /**
     * Tests that the keyboard is automatically hidden if a redirect/navigation occurs while it
     * is open.
     */
    @Test
    @MediumTest
    public void testKeyboardAutomaticallyClosesOnNavigation() throws InterruptedException {
        mVrTestRule.loadUrl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_web_input_editing"),
                PAGE_LOAD_TIMEOUT_S);
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);

        VrShell vrShell = TestVrShellDelegate.getVrShellForTesting();
        MockBrowserKeyboardInterface keyboard = new MockBrowserKeyboardInterface();
        vrShell.getInputMethodManagerWrapperForTesting().setBrowserKeyboardInterfaceForTesting(
                keyboard);

        // The webpage reacts to the first controller click by focusing its input field. Verify that
        // focus gain spawns the keyboard by clicking in the center of the page.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.CONTENT_QUAD, new PointF());
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> {
                    Boolean visible = keyboard.getLastKeyboardVisibility();
                    return visible != null && visible;
                },
                "Keyboard did not show from focusing a web input box", POLL_TIMEOUT_LONG_MS,
                POLL_CHECK_INTERVAL_SHORT_MS);

        // Navigate to a different page and ensure the keyboard automatically hides.
        mVrTestRule.loadUrl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_navigation_2d_page"),
                PAGE_LOAD_TIMEOUT_S);
        // The hiding should be done by the time navigation completes, so assert instead of polling.
        Assert.assertFalse(
                "Keyboard did not auto-hide on navigation", keyboard.getLastKeyboardVisibility());
    }

    /**
     * Tests that the cursor can be moved around while inputting text into a webpage and the
     * repositioning affects subsequent text input.
     */
    @Test
    @MediumTest
    public void testCursorReposition() throws InterruptedException, TimeoutException {
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        NativeUiUtils.enableMockedInput();
        NativeUiUtils.enableMockedKeyboard();
        mVrTestRule.loadUrl(VrBrowserTestFramework.getFileUrlForHtmlTestFile(
                                    "test_web_input_cursor_reposition"),
                PAGE_LOAD_TIMEOUT_S);
        NativeUiUtils.clickContentNode(
                "textfield", new PointF(), 1 /* numClicks */, mVrBrowserTestFramework);
        // Wait for the click to be registered by the text box.
        mVrBrowserTestFramework.waitOnJavaScriptStep();
        String initialString = "aaaaaaaaaaaaaaa bbbbbbbbbbbbb";
        NativeUiUtils.inputString(initialString);
        // Wait for the text input to be registered by the text box.
        mVrBrowserTestFramework.waitOnJavaScriptStep();
        mVrBrowserTestFramework.executeStepAndWait(
                "stepVerifyInitialString('" + initialString + "')");
        // Click near the lefthand side of the text box to reposition the cursor near the beginning
        // of the string.
        NativeUiUtils.clickContentNode(
                "textfield", new PointF(-0.45f, 0.0f), 1 /* numClicks */, mVrBrowserTestFramework);
        // Wait for the click to be registered.
        mVrBrowserTestFramework.waitOnJavaScriptStep();
        NativeUiUtils.inputBackspace();
        // Wait for the text change to be registered.
        mVrBrowserTestFramework.waitOnJavaScriptStep();
        // One of the "a"s should have been deleted.
        mVrBrowserTestFramework.executeStepAndWait(
                "stepVerifyDeletedString('" + initialString.substring(1) + "')");
        NativeUiUtils.inputString("c");
        // Wait for the text change to be registered.
        mVrBrowserTestFramework.waitOnJavaScriptStep();
        mVrBrowserTestFramework.executeStepAndWait(
                "stepVerifyInsertedString('" + initialString + "', 'c')");
        mVrBrowserTestFramework.endTest();
    }

    /**
     * Tests that text can be selected with double and triple clicks.
     */
    @Test
    @MediumTest
    public void testTextSelection() throws InterruptedException, TimeoutException {
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        NativeUiUtils.enableMockedInput();
        NativeUiUtils.enableMockedKeyboard();
        mVrTestRule.loadUrl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_web_input_selection"),
                PAGE_LOAD_TIMEOUT_S);
        NativeUiUtils.clickContentNode(
                "textfield", new PointF(), 1 /* numClicks */, mVrBrowserTestFramework);
        // Wait for the click to be registered by the text box.
        mVrBrowserTestFramework.waitOnJavaScriptStep();
        String initialString = "a bbbbbbbbbbbbbbbbbbbbbbb cccccccccccccccc";
        NativeUiUtils.inputString(initialString);
        // Wait for the text input to be registered by the text box.
        mVrBrowserTestFramework.waitOnJavaScriptStep();
        mVrBrowserTestFramework.executeStepAndWait(
                "stepVerifyInitialString('" + initialString + "')");
        // This should select all the "b"s.
        NativeUiUtils.clickContentNode(
                "textfield", new PointF(-0.4f, 0.0f), 2 /* numClicks */, mVrBrowserTestFramework);
        // Wait for the double click to be registered by the text box.
        mVrBrowserTestFramework.waitOnJavaScriptStep();
        NativeUiUtils.inputBackspace();
        // Wait for the text change to be registered.
        mVrBrowserTestFramework.waitOnJavaScriptStep();
        // Verify that all the "b"s were deleted.
        mVrBrowserTestFramework.executeStepAndWait(
                "stepVerifyDeletedString('" + initialString.replace("b", "") + "')");
        // This should select all the text.
        // We need to wait a bit so that these clicks aren't interpreted as a continuation of the
        // double click.
        Thread.sleep(500);
        NativeUiUtils.clickContentNode(
                "textfield", new PointF(-0.4f, 0.0f), 3 /* numClicks */, mVrBrowserTestFramework);
        // Wait for the triple click to be registered.
        mVrBrowserTestFramework.waitOnJavaScriptStep();
        NativeUiUtils.inputBackspace();
        // Wait for the deletion to be registered by the text box.
        mVrBrowserTestFramework.waitOnJavaScriptStep();
        mVrBrowserTestFramework.executeStepAndWait("stepVerifyClearedString()");
        mVrBrowserTestFramework.endTest();
    }

    /**
     * Tests that clicking outside of the keyboard area and not in a text input field hides the
     * keyboard.
     */
    @Test
    @MediumTest
    public void testClicksHideKeyboard() throws InterruptedException, TimeoutException {
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        mVrTestRule.loadUrl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("generic_text_entry_page"),
                PAGE_LOAD_TIMEOUT_S);
        NativeUiUtils.enableMockedInput();

        VrShell vrShell = TestVrShellDelegate.getVrShellForTesting();
        // For whatever reason, clicking outside the content quad doesn't hide the keyboard unless
        // we forward the show/hide calls to the actual keyboard. So, wrap the real interface
        // instead of being a pure mock interface.
        MockBrowserKeyboardInterface keyboard =
                new MockBrowserKeyboardInterface(vrShell.getInputMethodManagerWrapperForTesting()
                                                         .getBrowserKeyboardInterfaceForTesting());
        vrShell.getInputMethodManagerWrapperForTesting().setBrowserKeyboardInterfaceForTesting(
                keyboard);

        NativeUiUtils.clickContentNode(
                "textfield", new PointF(), 1 /* numClicks */, mVrBrowserTestFramework);
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> {
                    Boolean visible = keyboard.getLastKeyboardVisibility();
                    return visible != null && visible;
                },
                "Keyboard did not show from focusing a web input box", POLL_TIMEOUT_SHORT_MS,
                POLL_CHECK_INTERVAL_SHORT_MS);
        // Clicking near the top right of the content quad (or anywhere else away from the text box)
        // should hide the keyboard.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.CONTENT_QUAD, new PointF(0.4f, 0.4f));
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> {
                    Boolean visible = keyboard.getLastKeyboardVisibility();
                    return visible != null && !visible;
                },
                "Keyboard did not hide from clicking on content quad", POLL_TIMEOUT_SHORT_MS,
                POLL_CHECK_INTERVAL_SHORT_MS);

        // Make the keyboard appear again.
        NativeUiUtils.clickContentNode(
                "textfield", new PointF(), 1 /* numClicks */, mVrBrowserTestFramework);
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> {
                    Boolean visible = keyboard.getLastKeyboardVisibility();
                    return visible != null && visible;
                },
                "Keyboard did not show from focusing a web input box", POLL_TIMEOUT_SHORT_MS,
                POLL_CHECK_INTERVAL_SHORT_MS);

        // Clicking outside the content quad should also hide the keyboard.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.CONTENT_QUAD, new PointF(0.6f, 0.4f));
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> {
                    Boolean visible = keyboard.getLastKeyboardVisibility();
                    return visible != null && !visible;
                },
                "Keyboard did not hide from clicking outside content quad", POLL_TIMEOUT_SHORT_MS,
                POLL_CHECK_INTERVAL_SHORT_MS);
    }

    /**
     * Tests that pressing the app button on the Daydream controller hides the keyboard when editing
     * text on a webpage.
     */
    @Test
    @MediumTest
    public void testAppButtonHidesKeyboard() throws InterruptedException, TimeoutException {
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        mVrTestRule.loadUrl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("generic_text_entry_page"),
                PAGE_LOAD_TIMEOUT_S);
        NativeUiUtils.enableMockedInput();

        VrShell vrShell = TestVrShellDelegate.getVrShellForTesting();
        // The app button only hides the keyboard if we forward events to the actual keyboard
        // interface.
        MockBrowserKeyboardInterface keyboard =
                new MockBrowserKeyboardInterface(vrShell.getInputMethodManagerWrapperForTesting()
                                                         .getBrowserKeyboardInterfaceForTesting());
        vrShell.getInputMethodManagerWrapperForTesting().setBrowserKeyboardInterfaceForTesting(
                keyboard);

        NativeUiUtils.clickContentNode(
                "textfield", new PointF(), 1 /* numClicks */, mVrBrowserTestFramework);
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> {
                    Boolean visible = keyboard.getLastKeyboardVisibility();
                    return visible != null && visible;
                },
                "Keyboard did not show from focusing a web input box", POLL_TIMEOUT_SHORT_MS,
                POLL_CHECK_INTERVAL_SHORT_MS);

        NativeUiUtils.clickAppButton(UserFriendlyElementName.NONE, new PointF());
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> {
                    Boolean visible = keyboard.getLastKeyboardVisibility();
                    return visible != null && !visible;
                },
                "Keyboard did not hide from app button press", POLL_TIMEOUT_SHORT_MS,
                POLL_CHECK_INTERVAL_SHORT_MS);
    }

    /**
     * Tests that video controls look correct in the VR browser and properly hide when the
     * controller is pointed outside of the content quad.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "RenderTest"})
    public void testFullscreenVideoControls()
            throws InterruptedException, TimeoutException, IOException {
        // There's occasionally slight AA differences along the play button, so tolerate a small
        // amount of differing.
        mRenderTestRule.setPixelDiffThreshold(2);
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        NativeUiUtils.enableMockedInput();
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_video_controls"),
                PAGE_LOAD_TIMEOUT_S);

        // Click the fullscreen button. We use a separate button instead of the controls' fullscreen
        // button since that's more stable.
        DOMUtils.clickNode(mVrBrowserTestFramework.getCurrentWebContents(), "fullscreen_button",
                false /* goThroughRootAndroidView */);
        mVrBrowserTestFramework.pollJavaScriptBooleanOrFail(
                "document.getElementById('video_element').readyState === 4", POLL_TIMEOUT_LONG_MS);
        // The readyState === 4 check only checks that the video can be played through without any
        // buffering. Wait a bit after that to make sure that the video is actually fully loaded,
        // and that the loading animation has stopped.
        SystemClock.sleep(1000);

        NativeUiUtils.waitForUiQuiescence();

        HashMap<String, String> suffixToIds = new HashMap<String, String>();
        suffixToIds.put(
                NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI, "fullscreen_video_paused_browser_ui");
        suffixToIds.put(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_CONTENT,
                "fullscreen_video_paused_browser_content");
        RenderTestUtils.dumpAndCompare(suffixToIds, null /* bounds */, mRenderTestRule);

        // Start the video and hover outside of the content quad to make sure that the controls
        // auto-hide.
        NativeUiUtils.clickContentNode("video_element", new PointF(), 1, mVrBrowserTestFramework);
        NativeUiUtils.hoverElement(UserFriendlyElementName.CONTENT_QUAD, new PointF(1.0f, 0.0f));
        SystemClock.sleep(2000);
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_CONTENT,
                "fullscreen_video_playing_not_hovered_browser_content", mRenderTestRule);
    }
}
