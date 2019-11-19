// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;

import android.graphics.PointF;
import android.graphics.Rect;
import android.view.Choreographer;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;

import org.junit.Assert;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.vr.KeyboardTestAction;
import org.chromium.chrome.browser.vr.TestVrShellDelegate;
import org.chromium.chrome.browser.vr.UiTestOperationResult;
import org.chromium.chrome.browser.vr.UiTestOperationType;
import org.chromium.chrome.browser.vr.UserFriendlyElementName;
import org.chromium.chrome.browser.vr.VrBrowserTestFramework;
import org.chromium.chrome.browser.vr.VrControllerTestAction;
import org.chromium.chrome.browser.vr.VrDialog;
import org.chromium.chrome.browser.vr.VrShell;
import org.chromium.chrome.browser.vr.VrViewContainer;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeoutException;

/**
 * Class containing utility functions for interacting with the VR browser native UI, e.g. the
 * omnibox or back button.
 */
public class NativeUiUtils {
    @IntDef({ScrollDirection.UP, ScrollDirection.DOWN, ScrollDirection.LEFT, ScrollDirection.RIGHT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ScrollDirection {
        int UP = 0;
        int DOWN = 1;
        int LEFT = 2;
        int RIGHT = 3;
    }

    // How many frames to wait after entering text in the omnibox before we can assume that
    // suggestions are updated. This should only be used if the workaround of inputting text and
    // waiting for the suggestion box to appear doesn't work, e.g. if you need to input text, wait
    // for autocomplete, then input more text before committing. 20 is arbitrary, but stable.
    public static final int NUM_FRAMES_FOR_SUGGESTION_UPDATE = 20;
    // Arbitrary number of interpolated steps to perform within a scroll to consistently trigger
    // either fling or non-fling scrolling.
    public static final int NUM_STEPS_NON_FLING_SCROLL = 60;
    public static final int NUM_STEPS_FLING_SCROLL = 6;
    // Number of frames to wait after queueing a non-fling scroll before we can be sure that all the
    // scroll actions have been processed. The +2 comes from scrolls always having a touch down and
    // up action with NUM_STEPS_*_SCROLL additional actions in between.
    public static final int NUM_FRAMES_NON_FLING_SCROLL = NUM_STEPS_NON_FLING_SCROLL + 2;
    // Arbitrary number of frames to wait before sending a touch up event in order to ensure that a
    // fast scroll does not become a fling scroll.
    public static final int NUM_FRAMES_DELAY_TO_PREVENT_FLING = 30;
    public static final String FRAME_BUFFER_SUFFIX_WEB_XR_OVERLAY = "_WebXrOverlay";
    public static final String FRAME_BUFFER_SUFFIX_WEB_XR_CONTENT = "_WebXrContent";
    public static final String FRAME_BUFFER_SUFFIX_BROWSER_UI = "_BrowserUi";
    public static final String FRAME_BUFFER_SUFFIX_BROWSER_CONTENT = "_BrowserContent";
    // Valid position to click on the content quad in order to select the reposition bar.
    public static final PointF REPOSITION_BAR_COORDINATES = new PointF(0.0f, 0.55f);

    // Arbitrary but reasonable amount of time to expect the UI to stop updating after interacting
    // with an element.
    private static final int DEFAULT_UI_QUIESCENCE_TIMEOUT_MS = 2000;

    /**
     * Enables the use of both the mock head pose (locked forward) and Chrome-side mocked controller
     * without performing any specific actions.
     */
    public static void enableMockedInput() {
        TestVrShellDelegate.getInstance().performControllerActionForTesting(
                0 /* elementName, unused */, VrControllerTestAction.ENABLE_MOCKED_INPUT,
                new PointF() /* position, unused */);
    }

    /**
     * Enables the mock keyboard without sending any input. Should be called before performing an
     * action that would trigger the keyboard for the first time to ensure that the mock keyboard
     * is used instead of the real one.
     */
    public static void enableMockedKeyboard() {
        TestVrShellDelegate.getInstance().performKeyboardInputForTesting(
                KeyboardTestAction.ENABLE_MOCKED_KEYBOARD, "" /* unused */);
    }

    /**
     * Clicks on a UI element as if done via a controller.
     *
     * @param elementName The UserFriendlyElementName that will be clicked on.
     * @param position A PointF specifying where on the element to send the click relative to a
     *        unit square centered at (0, 0).
     */
    public static void clickElement(int elementName, PointF position) {
        clickDown(elementName, position);
        clickUp(elementName, position);
    }

    /**
     * Moves to the given position in the given element and presses the touchpad down.
     *
     * @param elementName The UserFriendlyElementName that will be clicked on.
     * @param position A PointF specifying where on the element to send the click relative to a
     *        unit square centered at (0, 0).
     */
    public static void clickDown(int elementName, PointF position) {
        TestVrShellDelegate.getInstance().performControllerActionForTesting(
                elementName, VrControllerTestAction.CLICK_DOWN, position);
    }

    /**
     * Moves to the given position in the given element and unpresses the touchpad.
     *
     * @param elementName The UserFriendlyElementName that will be unclicked on.
     * @param position A PointF specifying where on the element to send the click relative to a
     *        unit square centered at (0, 0).
     */
    public static void clickUp(int elementName, PointF position) {
        TestVrShellDelegate.getInstance().performControllerActionForTesting(
                elementName, VrControllerTestAction.CLICK_UP, position);
    }

    /**
     * Hovers over a UI element with the controller.
     *
     * @param elementName The UserFriendlyElementName that will be hovered over.
     * @param position A PointF specifying where on the element to hover relative to a unit square
     *        centered at (0, 0).
     */
    public static void hoverElement(int elementName, PointF position) {
        TestVrShellDelegate.getInstance().performControllerActionForTesting(
                elementName, VrControllerTestAction.HOVER, position);
    }

    /**
     * Clicks the app button while pointed at a UI element.
     * @param elementName The UserFriendlyElementName that will be pointed at.
     * @param position A PointF specifying where on the element to point at relative to a unit
     *        square centered on (0, 0).
     */
    public static void clickAppButton(int elementName, PointF position) {
        TestVrShellDelegate.getInstance().performControllerActionForTesting(
                elementName, VrControllerTestAction.APP_DOWN, position);
        TestVrShellDelegate.getInstance().performControllerActionForTesting(
                elementName, VrControllerTestAction.APP_UP, position);
    }

    /**
     * Presses the app button down while pointed at a UI element.
     * @param elementName The UserFriendlyElementName that will be pointed at.
     * @param position A PointF specifying where on the element to point at relative to a unit
     *        square centered on (0, 0).
     */
    public static void pressAppButton(int elementName, PointF position) {
        TestVrShellDelegate.getInstance().performControllerActionForTesting(
                elementName, VrControllerTestAction.APP_DOWN, position);
    }

    /**
     * Releases the app button while pointed at a UI element.
     * @param elementName The UserFriendlyElementName that will be pointed at.
     * @param position A PointF specifying where on the element to point at relative to a unit
     *        square centered on (0, 0).
     */
    public static void releaseAppButton(int elementName, PointF position) {
        TestVrShellDelegate.getInstance().performControllerActionForTesting(
                elementName, VrControllerTestAction.APP_UP, position);
    }

    /**
     * Clicks on a DOM element/node as if done via a controller.
     *
     * @param nodeId The ID of the node to click on.
     * @param position A PointF specifying where on the node to send the click relative to a unit
     *        square centered at (0, 0).
     * @param numClicks The number of times to click the element.
     * @param testFramework The VrBrowserTestFramework to use to interact with the page.
     */
    public static void clickContentNode(String nodeId, PointF position, final int numClicks,
            VrBrowserTestFramework testFramework) throws InterruptedException, TimeoutException {
        Rect nodeBounds = DOMUtils.getNodeBounds(testFramework.getCurrentWebContents(), nodeId);
        int contentWidth = Integer.valueOf(
                testFramework.runJavaScriptOrFail("window.innerWidth", POLL_TIMEOUT_SHORT_MS));
        int contentHeight = Integer.valueOf(
                testFramework.runJavaScriptOrFail("window.innerHeight", POLL_TIMEOUT_SHORT_MS));
        // Convert the given PointF (native UI-style coordinates) into absolute content coordinates
        // for the node.
        // The coordinate systems are different (content has the origin in the top left, click
        // coordinates have the "origin" in the bottom left), so be sure to account for that.
        float nodeCoordX = (nodeBounds.right - nodeBounds.left) * (0.5f + position.x);
        float nodeCoordY = (nodeBounds.bottom - nodeBounds.top) * (0.5f - position.y);
        // Offset the click position within the node by the node's location to get the click
        // position within the content.
        PointF absClickCoord =
                new PointF(nodeCoordX + nodeBounds.left, nodeCoordY + nodeBounds.top);

        // Scale the coordinates between 0 and 1.
        float contentCoordX = absClickCoord.x / contentWidth;
        float contentCoordY = absClickCoord.y / contentHeight;
        // Now convert back to the native UI-style coordinates.
        final PointF clickCoordinates = new PointF(contentCoordX - 0.5f, 0.5f - contentCoordY);
        performActionAndWaitForUiQuiescence(() -> {
            for (int i = 0; i < numClicks; ++i) {
                clickElement(UserFriendlyElementName.CONTENT_QUAD, clickCoordinates);
                // Rarely, sending clicks back to back can cause the web contents to miss a click.
                // So, if we're going to be sending more, introduce a few input-less frames to avoid
                // this issue. 5 appears to currently be the magic number that lets the web contents
                // reliably pick up all clicks.
                if (i < numClicks - 1) {
                    for (int j = 0; j < 5; ++j) {
                        hoverElement(UserFriendlyElementName.CONTENT_QUAD, clickCoordinates);
                    }
                }
            }
        });
    }

    /**
     * Helper function to click the reposition bar to select it.
     */
    public static void selectRepositionBar() {
        // We need to ensure that the reposition bar is at least partially visible before trying
        // to click it, so hover it for a frame.
        hoverElement(UserFriendlyElementName.CONTENT_QUAD, REPOSITION_BAR_COORDINATES);
        clickElement(UserFriendlyElementName.CONTENT_QUAD, REPOSITION_BAR_COORDINATES);
    }

    /**
     * An alias to click in place in order to deslect the reposition bar.
     */
    public static void deselectRepositionBar() {
        clickElement(UserFriendlyElementName.CURRENT_POSITION, new PointF());
    }

    /**
     * Touches the touchpad at the given coordinates, keeping whatever button states and direction
     * are already present.
     *
     * @param position A PointF specifying where on the touchpad to touch, each axis in the range
     *        [-1, 1].
     */
    public static void touchDown(PointF position) {
        TestVrShellDelegate.getInstance().performControllerActionForTesting(
                UserFriendlyElementName.NONE /* unused */, VrControllerTestAction.TOUCH_DOWN,
                position);
    }

    /**
     * Helper function for performing a non-fling scroll.
     *
     * @param direction the ScrollDirection to scroll in.
     */
    public static void scrollNonFling(@ScrollDirection int direction) throws InterruptedException {
        scroll(directionToStartPoint(direction), directionToEndPoint(direction),
                NUM_STEPS_NON_FLING_SCROLL, false /* delayTouchUp */);
    }

    /**
     * Helper function for performing a fling scroll.
     *
     * @param direction the ScrollDirection to scroll in.
     */
    public static void scrollFling(@ScrollDirection int direction) throws InterruptedException {
        scroll(directionToStartPoint(direction), directionToEndPoint(direction),
                NUM_STEPS_FLING_SCROLL, false /* delayTouchUp */);
    }

    /**
     * Helper function to perform the same action as a fling scroll, but delay the touch up event.
     * This results in a fast, non-fling scroll that's useful for ensuring that an actual fling
     * scroll works by asserting the actual fling scroll goes further.
     *
     * @param direction the ScrollDirection to scroll in.
     */
    public static void scrollNonFlingFast(@ScrollDirection int direction)
            throws InterruptedException {
        scroll(directionToStartPoint(direction), directionToEndPoint(direction),
                NUM_STEPS_FLING_SCROLL, true /* delayTouchUp */);
    }

    /**
     * Perform a touchpad drag to scroll.
     *
     * @param start the position on the touchpad to start the drag.
     * @param end the position on the touchpad to end the drag.
     * @param numSteps the number of steps to interpolate between the two points, one step per
     *        frame.
     * @param delayTouchUp whether to significantly delay the final touch up event, which should
     *        prevent fling scrolls regardless of scroll speed.
     */
    public static void scroll(PointF start, PointF end, int numSteps, boolean delayTouchUp)
            throws InterruptedException {
        PointF stepIncrement =
                new PointF((end.x - start.x) / numSteps, (end.y - start.y) / numSteps);
        PointF currentPosition = new PointF(start.x, start.y);
        touchDown(currentPosition);
        for (int i = 0; i < numSteps; ++i) {
            currentPosition.offset(stepIncrement.x, stepIncrement.y);
            touchDown(currentPosition);
        }
        if (delayTouchUp) {
            waitNumFrames(NUM_FRAMES_DELAY_TO_PREVENT_FLING);
        }
        TestVrShellDelegate.getInstance().performControllerActionForTesting(
                UserFriendlyElementName.NONE /* unused */, VrControllerTestAction.TOUCH_UP, end);
    }

    /**
     * Inputs the given text as if done via the VR keyboard.
     *
     * @param inputString The String to input via the keyboard.
     */
    public static void inputString(String inputString) {
        TestVrShellDelegate.getInstance().performKeyboardInputForTesting(
                KeyboardTestAction.INPUT_TEXT, inputString);
    }

    /**
     * Presses backspace as if done via the VR keyboard.
     */
    public static void inputBackspace() {
        TestVrShellDelegate.getInstance().performKeyboardInputForTesting(
                KeyboardTestAction.BACKSPACE, "" /* unused */);
    }

    /**
     * Presses enter as if done via the VR keyboard.
     */
    public static void inputEnter() throws InterruptedException {
        TestVrShellDelegate.getInstance().performKeyboardInputForTesting(
                KeyboardTestAction.ENTER, "" /* unused */);
    }

    /**
     * Clicks on a UI element as if done via a controller and waits until all resulting
     * animations have finished and propogated to the point of being visible in screenshots.
     *
     * @param elementName The UserFriendlyElementName that will be clicked on.
     * @param position A PointF specifying where on the element to send the click relative to a
     *        unit square centered at (0, 0).
     */
    public static void clickElementAndWaitForUiQuiescence(
            final int elementName, final PointF position) throws InterruptedException {
        performActionAndWaitForUiQuiescence(() -> { clickElement(elementName, position); });
    }

    /**
     * Clicks on a fallback UI element's positive button, e.g. "Allow" or "Confirm".
     */
    public static void clickFallbackUiPositiveButton() throws InterruptedException {
        clickFallbackUiButton(R.id.positive_button);
    }

    /**
     * Clicks on a fallback UI element's negative button, e.g. "Deny" or "Cancel".
     */
    public static void clickFallbackUiNegativeButton() throws InterruptedException {
        clickFallbackUiButton(R.id.negative_button);
    }

    /**
     * Clicks and drags within a single UI element.
     *
     * @param elementName The UserFriendlyElementName that will be clicked and dragged in.
     * @param positionStart The PointF specifying where on the element to start the click/drag
     *        relative to a unit square centered at (0, 0).
     * @param positionEnd The PointF specifying where on the element to end the click/drag relative
     *        to a unit square centered at (0, 0);
     * @param numInterpolatedSteps How many steps to interpolate the drag between the provided
     *        start and end positions.
     */
    public static void clickAndDragElement(
            int elementName, PointF positionStart, PointF positionEnd, int numInterpolatedSteps) {
        Assert.assertTrue(
                "Given a negative number of steps to interpolate", numInterpolatedSteps >= 0);
        TestVrShellDelegate.getInstance().performControllerActionForTesting(
                elementName, VrControllerTestAction.CLICK_DOWN, positionStart);
        PointF stepOffset =
                new PointF((positionEnd.x - positionStart.x) / (numInterpolatedSteps + 1),
                        (positionEnd.y - positionStart.y) / (numInterpolatedSteps + 1));
        PointF currentPosition = positionStart;
        for (int i = 0; i < numInterpolatedSteps; i++) {
            currentPosition.offset(stepOffset.x, stepOffset.y);
            TestVrShellDelegate.getInstance().performControllerActionForTesting(
                    elementName, VrControllerTestAction.MOVE, currentPosition);
        }
        TestVrShellDelegate.getInstance().performControllerActionForTesting(
                elementName, VrControllerTestAction.MOVE, positionEnd);
        TestVrShellDelegate.getInstance().performControllerActionForTesting(
                elementName, VrControllerTestAction.CLICK_UP, positionEnd);
    }

    /**
     * Sets the native code to start using the real controller and head pose data again instead of
     * fake testing data.
     */
    public static void revertToRealInput() {
        TestVrShellDelegate.getInstance().performControllerActionForTesting(
                0 /* elementName, unused */, VrControllerTestAction.REVERT_TO_REAL_INPUT,
                new PointF() /* position, unused */);
    }

    /**
     * Runs the given Runnable and waits until the native UI reports that it is quiescent. The
     * provided Runnable is expected to cause a UI change of some sort, so the quiescence wait will
     * fail if no change is detected within the allotted time.
     *
     * @param action A Runnable containing the action to perform.
     */
    public static void performActionAndWaitForUiQuiescence(Runnable action) {
        final TestVrShellDelegate instance = TestVrShellDelegate.getInstance();
        final CountDownLatch resultLatch = new CountDownLatch(1);
        final VrShell.UiOperationData operationData = new VrShell.UiOperationData();
        operationData.actionType = UiTestOperationType.UI_ACTIVITY_RESULT;
        operationData.resultCallback = () -> {
            resultLatch.countDown();
        };
        operationData.timeoutMs = DEFAULT_UI_QUIESCENCE_TIMEOUT_MS;
        // Run on the UI thread to prevent issues with registering a new callback before
        // ReportUiOperationResultForTesting has finished.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { instance.registerUiOperationCallbackForTesting(operationData); });
        action.run();

        // Wait for any outstanding animations to finish. Catch the interrupted exception so we
        // don't have to try/catch anytime we chain multiple actions.
        try {
            resultLatch.await();
        } catch (InterruptedException e) {
            Assert.fail("Interrupted while waiting for UI quiescence: " + e.toString());
        }
        int uiResult =
                instance.getLastUiOperationResultForTesting(UiTestOperationType.UI_ACTIVITY_RESULT);
        Assert.assertEquals("UI reported non-quiescent result '"
                        + uiTestOperationResultToString(uiResult) + "'",
                UiTestOperationResult.QUIESCENT, uiResult);
    }

    /**
     * Waits until either the UI reports quiescence or a timeout is reached. Unlike
     * performActionAndWaitForUiQuiescence, this does not fail if no UI change is detected within
     * the allotted time, so it can be used when it is unsure whether the UI is already quiescent
     * or not, e.g. when initally entering the VR browser.
     */
    public static void waitForUiQuiescence() {
        final TestVrShellDelegate instance = TestVrShellDelegate.getInstance();
        final CountDownLatch resultLatch = new CountDownLatch(1);
        final VrShell.UiOperationData operationData = new VrShell.UiOperationData();
        operationData.actionType = UiTestOperationType.UI_ACTIVITY_RESULT;
        operationData.resultCallback = () -> {
            resultLatch.countDown();
        };
        operationData.timeoutMs = DEFAULT_UI_QUIESCENCE_TIMEOUT_MS;
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { instance.registerUiOperationCallbackForTesting(operationData); });
        // Catch the interrupted exception so we don't have to try/catch anytime we chain multiple
        // actions.
        try {
            resultLatch.await();
        } catch (InterruptedException e) {
            Assert.fail("Interrupted while waiting for UI quiescence: " + e.toString());
        }

        int uiResult =
                instance.getLastUiOperationResultForTesting(UiTestOperationType.UI_ACTIVITY_RESULT);
        Assert.assertTrue("UI reported non-quiescent result '"
                        + uiTestOperationResultToString(uiResult) + "'",
                uiResult == UiTestOperationResult.QUIESCENT
                        || uiResult == UiTestOperationResult.TIMEOUT_NO_START);
    }

    /**
     * Runs the given Runnable and waits until the specified element matches the requested
     * visibility.
     *
     * @param elementName The UserFriendlyElementName to wait on to change visibility.
     * @param status The visibility status to wait for.
     * @param action A Runnable containing the action to perform.
     */
    public static void performActionAndWaitForVisibilityStatus(
            final int elementName, final boolean visible, Runnable action) {
        final TestVrShellDelegate instance = TestVrShellDelegate.getInstance();
        final CountDownLatch resultLatch = new CountDownLatch(1);
        final VrShell.UiOperationData operationData = new VrShell.UiOperationData();
        operationData.actionType = UiTestOperationType.ELEMENT_VISIBILITY_STATUS;
        operationData.resultCallback = () -> {
            resultLatch.countDown();
        };
        operationData.timeoutMs = DEFAULT_UI_QUIESCENCE_TIMEOUT_MS;
        operationData.elementName = elementName;
        operationData.visibility = visible;
        // Run on the UI thread to prevent issues with registering a new callback before
        // ReportUiOperationResultForTesting has finished.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { instance.registerUiOperationCallbackForTesting(operationData); });
        action.run();

        // Wait for the result to be reported. Catch the interrupted exception so we don't have to
        // try/catch anytime we chain multiple actions.
        try {
            resultLatch.await();
        } catch (InterruptedException e) {
            Assert.fail("Interrupted while waiting for visibility status: " + e.toString());
        }

        int result = instance.getLastUiOperationResultForTesting(
                UiTestOperationType.ELEMENT_VISIBILITY_STATUS);
        Assert.assertEquals("UI reported non-visibility-changed result '"
                        + uiTestOperationResultToString(result) + "'",
                UiTestOperationResult.VISIBILITY_MATCH, result);
    }

    /**
     * Blocks until the specified number of frames have been triggered by the Choreographer.
     *
     * @param numFrames The number of frames to wait for.
     */
    public static void waitNumFrames(int numFrames) {
        final CountDownLatch frameLatch = new CountDownLatch(numFrames);
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            final Choreographer.FrameCallback callback = new Choreographer.FrameCallback() {
                @Override
                public void doFrame(long frameTimeNanos) {
                    if (frameLatch.getCount() == 0) return;
                    Choreographer.getInstance().postFrameCallback(this);
                    frameLatch.countDown();
                }
            };
            Choreographer.getInstance().postFrameCallback(callback);
        });
        try {
            frameLatch.await();
        } catch (InterruptedException e) {
            Assert.fail("Interrupted while waiting for frames: " + e.toString());
        }
    }

    /**
     * Tells the native UI to dump the next frame's frame buffers to disk and waits for it to
     * signal that the dump is complete.
     *
     * @param filepathBase The filepath to use as a base for image dumps. Will have a suffix and
     *        file extension automatically appended.
     */
    public static void dumpNextFramesFrameBuffers(String filepathBase) throws InterruptedException {
        // Clear out any existing images with the names of the files that may be created.
        for (String suffix : new String[] {FRAME_BUFFER_SUFFIX_WEB_XR_OVERLAY,
                     FRAME_BUFFER_SUFFIX_WEB_XR_CONTENT, FRAME_BUFFER_SUFFIX_BROWSER_UI,
                     FRAME_BUFFER_SUFFIX_BROWSER_CONTENT}) {
            File dumpFile = new File(filepathBase, suffix + ".png");
            Assert.assertFalse("Failed to delete existing screenshot",
                    dumpFile.exists() && !dumpFile.delete());
        }

        final TestVrShellDelegate instance = TestVrShellDelegate.getInstance();
        final CountDownLatch resultLatch = new CountDownLatch(1);
        final VrShell.UiOperationData operationData = new VrShell.UiOperationData();
        operationData.actionType = UiTestOperationType.FRAME_BUFFER_DUMPED;
        operationData.resultCallback = () -> {
            resultLatch.countDown();
        };

        // Run on the UI thread to prevent issues with registering a new callback before
        // ReportUiOperationResultForTesting has finished.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { instance.registerUiOperationCallbackForTesting(operationData); });
        instance.saveNextFrameBufferToDiskForTesting(filepathBase);
        resultLatch.await();
    }

    /**
     * Returns the Container of 2D UI that is shown in VR.
     */
    public static ViewGroup getVrViewContainer() {
        VrShell vrShell = TestVrShellDelegate.getVrShellForTesting();
        return vrShell.getVrViewContainerForTesting();
    }

    /**
     * Waits until a modal dialog is or is not shown.
     */
    public static void waitForModalDialogStatus(
            final boolean shouldBeShown, final ChromeActivity activity) {
        CriteriaHelper.pollUiThread(
                ()
                        -> {
                    return shouldBeShown == activity.getModalDialogManager().isShowing();
                },
                "Timed out waiting for modal dialog to "
                        + (shouldBeShown ? "be shown" : "not be shown"));
    }

    private static void clickFallbackUiButton(int buttonId) throws InterruptedException {
        VrShell vrShell = TestVrShellDelegate.getVrShellForTesting();
        VrViewContainer viewContainer = vrShell.getVrViewContainerForTesting();
        Assert.assertTrue(
                "VrViewContainer does not have children", viewContainer.getChildCount() > 0);
        // Click on whatever dialog was most recently added
        VrDialog vrDialog = (VrDialog) viewContainer.getChildAt(viewContainer.getChildCount() - 1);
        View button = vrDialog.findViewById(buttonId);
        Assert.assertNotNull("Did not find view with specified ID", button);
        // Calculate the center of the button we want to click on and scale it to fit a unit square
        // centered on (0,0).
        float x = ((button.getX() + button.getWidth() / 2) - vrDialog.getWidth() / 2)
                / vrDialog.getWidth();
        float y = ((button.getY() + button.getHeight() / 2) - vrDialog.getHeight() / 2)
                / vrDialog.getHeight();
        PointF buttonCenter = new PointF(x, y);
        clickElementAndWaitForUiQuiescence(UserFriendlyElementName.BROWSING_DIALOG, buttonCenter);
    }

    private static String uiTestOperationResultToString(int result) {
        switch (result) {
            case UiTestOperationResult.UNREPORTED:
                return "Unreported";
            case UiTestOperationResult.QUIESCENT:
                return "Quiescent";
            case UiTestOperationResult.TIMEOUT_NO_START:
                return "Timeout (UI activity not started)";
            case UiTestOperationResult.TIMEOUT_NO_END:
                return "Timeout (UI activity not stopped)";
            case UiTestOperationResult.VISIBILITY_MATCH:
                return "Visibility match";
            case UiTestOperationResult.TIMEOUT_NO_VISIBILITY_MATCH:
                return "Timeout (Element visibility did not match)";
            default:
                return "Unknown result";
        }
    }

    private static PointF directionToStartPoint(@ScrollDirection int direction) {
        switch (direction) {
            case ScrollDirection.UP:
                return new PointF(0.5f, 0.05f);
            case ScrollDirection.DOWN:
                return new PointF(0.5f, 0.95f);
            case ScrollDirection.LEFT:
                return new PointF(0.05f, 0.5f);
            default:
                return new PointF(0.95f, 0.5f);
        }
    }

    private static PointF directionToEndPoint(@ScrollDirection int direction) {
        switch (direction) {
            case ScrollDirection.UP:
                return new PointF(0.5f, 0.95f);
            case ScrollDirection.DOWN:
                return new PointF(0.5f, 0.05f);
            case ScrollDirection.LEFT:
                return new PointF(0.95f, 0.5f);
            default:
                return new PointF(0.05f, 0.5f);
        }
    }
}
