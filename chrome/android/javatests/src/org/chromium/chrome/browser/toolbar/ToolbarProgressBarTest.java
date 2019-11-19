// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.animation.Animator;
import android.content.res.Resources;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.widget.ClipDrawableProgressBar.ProgressBarObserver;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;

/**
 * Tests related to the ToolbarProgressBar.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
public class ToolbarProgressBarTest extends DummyUiActivityTestCase {
    private final CallbackHelper mProgressUpdateHelper = new CallbackHelper();
    private final CallbackHelper mProgressVisibilityHelper = new CallbackHelper();
    private ToolbarProgressBar mProgressBar;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        TestThreadUtils.runOnUiThreadBlocking(this::setUpTestOnUi);
    }

    private void setUpTestOnUi() {
        ViewGroup view = new FrameLayout(getActivity());
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        getActivity().setContentView(view, params);

        Resources res = getActivity().getResources();
        int heightPx = res.getDimensionPixelSize(R.dimen.toolbar_progress_bar_height);

        View anchor = new View(getActivity());
        view.addView(anchor,
                new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, heightPx * 2));

        mProgressBar = new ToolbarProgressBar(getActivity(), heightPx, anchor, false);
        @ColorInt
        int toolbarColor = ApiCompatibilityUtils.getColor(res, R.color.toolbar_background_primary);
        mProgressBar.setThemeColor(toolbarColor, false);
        mProgressBar.setProgressBarObserver(new ProgressBarObserver() {
            @Override
            public void onVisibleProgressUpdated() {
                mProgressUpdateHelper.notifyCalled();
            }

            @Override
            public void onVisibilityChanged() {
                mProgressVisibilityHelper.notifyCalled();
            }
        });

        view.addView(mProgressBar,
                new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, heightPx));
    }

    /**
     * Get the current progress from the UI thread.
     * @return The current progress displayed by the progress bar.
     */
    private float getProgress() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> mProgressBar.getProgress());
    }

    /**
     * Get the current progress bar visibility from the UI thread.
     * @return The current progress displayed by the progress bar.
     */
    private boolean isProgressBarVisible() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mProgressBar.getVisibility() == View.VISIBLE);
    }

    /**
     * Test that the progress bar indeterminate animation completely traverses the screen.
     */
    @Test
    @Feature({"Android-Progress-Bar"})
    @SmallTest
    public void testProgressBarCompletion_indeterminateAnimation() throws TimeoutException {
        Animator progressAnimator = mProgressBar.getIndeterminateAnimatorForTesting();

        int currentVisibilityCallCount = mProgressVisibilityHelper.getCallCount();

        TestThreadUtils.runOnUiThreadBlocking(() -> mProgressBar.start());
        assertFalse("Indeterminate animation should not be running.", progressAnimator.isRunning());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mProgressBar.startIndeterminateAnimationForTesting();
            mProgressBar.setProgress(0.5f);
        });

        // Wait for a visibility change.
        mProgressVisibilityHelper.waitForCallback(currentVisibilityCallCount, 1);
        currentVisibilityCallCount++;

        assertTrue("Indeterminate animation should be running.", progressAnimator.isRunning());

        // Wait for progress updates to reach 50%.
        int currentProgressCallCount = mProgressUpdateHelper.getCallCount();
        while (!MathUtils.areFloatsEqual(getProgress(), 0.5f)) {
            mProgressUpdateHelper.waitForCallback(currentProgressCallCount, 1);
            currentProgressCallCount++;
        }

        TestThreadUtils.runOnUiThreadBlocking(() -> mProgressBar.finish(true));

        // Wait for progress updates to reach 100%.
        currentProgressCallCount = mProgressUpdateHelper.getCallCount();
        while (!MathUtils.areFloatsEqual(getProgress(), 1.0f)) {
            mProgressUpdateHelper.waitForCallback(currentProgressCallCount, 1);
            currentProgressCallCount++;
        }

        // Make sure the progress bar remains visible through completion.
        assertTrue("Progress bar should still be visible.", isProgressBarVisible());

        assertEquals("Progress should have reached 100%.", 1.0f, getProgress(), MathUtils.EPSILON);

        // Wait for a visibility change now that progress has completed.
        mProgressVisibilityHelper.waitForCallback(currentVisibilityCallCount, 1);

        assertFalse("Indeterminate animation should not be running.", progressAnimator.isRunning());
        assertFalse("Progress bar should not be visible.", isProgressBarVisible());
    }

    /**
     * Test that the progress bar completely traverses the screen without animation.
     */
    @Test
    @Feature({"Android-Progress-Bar"})
    @SmallTest
    public void testProgressBarCompletion_noAnimation() throws TimeoutException {
        int currentVisibilityCallCount = mProgressVisibilityHelper.getCallCount();
        int currentProgressCallCount = mProgressUpdateHelper.getCallCount();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mProgressBar.start();
            mProgressBar.setProgress(0.5f);
        });

        // Wait for a visibility change.
        mProgressVisibilityHelper.waitForCallback(currentVisibilityCallCount, 1);
        currentVisibilityCallCount++;

        // Wait for progress updates to reach 50%.
        mProgressUpdateHelper.waitForCallback(currentProgressCallCount, 1);
        currentProgressCallCount++;
        assertEquals("Progress should have reached 50%.", 0.5f, getProgress(), MathUtils.EPSILON);

        currentProgressCallCount = mProgressUpdateHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> mProgressBar.finish(true));

        // Wait for progress updates to reach 100%.
        mProgressUpdateHelper.waitForCallback(currentProgressCallCount, 1);
        currentProgressCallCount++;
        assertEquals("Progress should have reached 100%.", 1.0f, getProgress(), MathUtils.EPSILON);

        // Make sure the progress bar remains visible through completion.
        assertTrue("Progress bar should still be visible.", isProgressBarVisible());

        // Wait for a visibility change now that progress has completed.
        mProgressVisibilityHelper.waitForCallback(currentVisibilityCallCount, 1);

        assertFalse("Progress bar should not be visible.", isProgressBarVisible());
    }

    /**
     * Test that the progress bar ends immediately if #finish(...) is called with delay = false.
     */
    @Test
    @Feature({"Android-Progress-Bar"})
    @SmallTest
    public void testProgressBarCompletion_indeterminateAnimation_noDelay() throws TimeoutException {
        Animator progressAnimator = mProgressBar.getIndeterminateAnimatorForTesting();

        int currentVisibilityCallCount = mProgressVisibilityHelper.getCallCount();

        TestThreadUtils.runOnUiThreadBlocking(() -> mProgressBar.start());
        assertFalse("Indeterminate animation should not be running.", progressAnimator.isRunning());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mProgressBar.startIndeterminateAnimationForTesting();
            mProgressBar.setProgress(0.5f);
        });

        // Wait for a visibility change.
        mProgressVisibilityHelper.waitForCallback(currentVisibilityCallCount, 1);
        currentVisibilityCallCount++;

        assertTrue("Indeterminate animation should be running.", progressAnimator.isRunning());

        // Wait for progress updates to reach 50%.
        int currentProgressCallCount = mProgressUpdateHelper.getCallCount();
        while (!MathUtils.areFloatsEqual(getProgress(), 0.5f)) {
            mProgressUpdateHelper.waitForCallback(currentProgressCallCount, 1);
            currentProgressCallCount++;
        }

        // Finish progress with no delay.
        TestThreadUtils.runOnUiThreadBlocking(() -> mProgressBar.finish(false));

        // The progress bar should immediately be invisible.
        assertFalse("Progress bar should be invisible.", isProgressBarVisible());

        assertFalse("Indeterminate animation should not be running.", progressAnimator.isRunning());
    }

    /**
     * Test that the progress bar resets if a navigation occurs mid-progress while the indeterminate
     * animation is running.
     */
    @Test
    @Feature({"Android-Progress-Bar"})
    @SmallTest
    public void testProgressBarReset_indeterminateAnimation() throws TimeoutException {
        Animator progressAnimator = mProgressBar.getIndeterminateAnimatorForTesting();

        int currentVisibilityCallCount = mProgressVisibilityHelper.getCallCount();

        TestThreadUtils.runOnUiThreadBlocking(() -> mProgressBar.start());
        assertFalse("Indeterminate animation should not be running.", progressAnimator.isRunning());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mProgressBar.startIndeterminateAnimationForTesting();
            mProgressBar.setProgress(0.5f);
        });

        // Wait for a visibility change.
        mProgressVisibilityHelper.waitForCallback(currentVisibilityCallCount, 1);
        currentVisibilityCallCount++;

        assertTrue("Indeterminate animation should be running.", progressAnimator.isRunning());

        // Wait for progress updates to reach 50%.
        int currentProgressCallCount = mProgressUpdateHelper.getCallCount();
        while (!MathUtils.areFloatsEqual(getProgress(), 0.5f)) {
            mProgressUpdateHelper.waitForCallback(currentProgressCallCount, 1);
            currentProgressCallCount++;
        }

        // Restart the progress bar.
        currentProgressCallCount = mProgressUpdateHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> mProgressBar.start());

        // Wait for progress update.
        mProgressUpdateHelper.waitForCallback(currentProgressCallCount, 1);
        currentProgressCallCount++;

        // Make sure the progress bar remains visible through completion.
        assertTrue("Progress bar should still be visible.", isProgressBarVisible());

        assertEquals("Progress should be at 0%.", 0.0f, getProgress(), MathUtils.EPSILON);
    }
}
