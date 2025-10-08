// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.compositor.layouts.phone.AnimationFreezeChecker.AnimationState;

/** Unit tests for {@link AnimationFreezeChecker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AnimationFreezeCheckerUnitTest {
    private static final String HISTOGRAM_NAME = "Tab.TestTag.NewTabAnimationProgress";

    private AnimationFreezeChecker mAnimationFreezeChecker;

    @Before
    public void setUp() {
        mAnimationFreezeChecker = new AnimationFreezeChecker("TestTag");
    }

    @Test
    public void testAnimationCompletes_End() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(HISTOGRAM_NAME, AnimationState.STARTED)
                        .expectIntRecord(HISTOGRAM_NAME, AnimationState.ENDED)
                        .build();

        mAnimationFreezeChecker.onAnimationStart();
        mAnimationFreezeChecker.onAnimationEnd();

        // Timeout should not do anything.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        watcher.assertExpected();
    }

    @Test
    public void testAnimationCompletes_Cancel() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(HISTOGRAM_NAME, AnimationState.STARTED)
                        .expectIntRecord(HISTOGRAM_NAME, AnimationState.CANCELLED)
                        .build();

        mAnimationFreezeChecker.onAnimationStart();
        mAnimationFreezeChecker.onAnimationCancel();

        // Timeout should not do anything.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        watcher.assertExpected();
    }

    @Test
    public void testAnimationTimeout_ThenEnd() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(HISTOGRAM_NAME, AnimationState.STARTED)
                        .expectIntRecord(HISTOGRAM_NAME, AnimationState.TIMED_OUT)
                        .expectIntRecord(
                                HISTOGRAM_NAME, AnimationState.CANCELLED_OR_ENDED_AFTER_TIMEOUT)
                        .build();

        mAnimationFreezeChecker.onAnimationStart();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        mAnimationFreezeChecker.onAnimationEnd();

        watcher.assertExpected();
    }

    @Test
    public void testAnimationTimeout_ThenCancel() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(HISTOGRAM_NAME, AnimationState.STARTED)
                        .expectIntRecord(HISTOGRAM_NAME, AnimationState.TIMED_OUT)
                        .expectIntRecord(
                                HISTOGRAM_NAME, AnimationState.CANCELLED_OR_ENDED_AFTER_TIMEOUT)
                        .build();

        mAnimationFreezeChecker.onAnimationStart();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        mAnimationFreezeChecker.onAnimationCancel();

        watcher.assertExpected();
    }

    @Test
    public void testMultipleEndCalls() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(HISTOGRAM_NAME, AnimationState.STARTED)
                        .expectIntRecord(HISTOGRAM_NAME, AnimationState.ENDED)
                        .expectIntRecord(HISTOGRAM_NAME, AnimationState.ENDED_LOOPED)
                        .build();

        mAnimationFreezeChecker.onAnimationStart();
        mAnimationFreezeChecker.onAnimationEnd();

        // Second call should record a loop.
        mAnimationFreezeChecker.onAnimationEnd();
        // This call should be ignored.
        mAnimationFreezeChecker.onAnimationCancel();
        // Third call should be ignored.
        mAnimationFreezeChecker.onAnimationEnd();

        watcher.assertExpected();
    }

    @Test
    public void testMultipleCancelCalls() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(HISTOGRAM_NAME, AnimationState.STARTED)
                        .expectIntRecord(HISTOGRAM_NAME, AnimationState.CANCELLED)
                        .expectIntRecord(HISTOGRAM_NAME, AnimationState.CANCELLED_LOOPED)
                        .build();

        mAnimationFreezeChecker.onAnimationStart();
        mAnimationFreezeChecker.onAnimationCancel();

        // Second call should record a loop.
        mAnimationFreezeChecker.onAnimationCancel();
        // This call should be ignored.
        mAnimationFreezeChecker.onAnimationEnd();
        // Third call should be ignored.
        mAnimationFreezeChecker.onAnimationCancel();

        watcher.assertExpected();
    }
}
