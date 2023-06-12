// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import android.graphics.PointF;
import android.view.Choreographer;

import androidx.annotation.IntDef;

import org.junit.Assert;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.vr.UiTestOperationResult;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.CountDownLatch;

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

    /**
     * Blocks until the specified number of frames have been triggered by the Choreographer.
     *
     * @param numFrames The number of frames to wait for.
     */
    public static void waitNumFrames(int numFrames) {
        final CountDownLatch frameLatch = new CountDownLatch(numFrames);
        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> {
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
