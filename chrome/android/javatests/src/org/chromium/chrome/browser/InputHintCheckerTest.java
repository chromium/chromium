// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.widget.TextView;

import androidx.annotation.Keep;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.InputHintChecker;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** Integration tests for InputHintChecker. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Tests once-per-process initialization")
public final class InputHintCheckerTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @ClassRule
    public static final ChromeBrowserTestRule sBrowserTestRule = new ChromeBrowserTestRule();

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InputHintChecker.setView(null);
                    InputHintChecker.setAllowSetViewForTesting(false);
                });
    }

    public static class TestTextView extends TextView {
        public TestTextView(Context context) {
            super(context);
        }

        @Keep // Keep for reflection calls in this test.
        public boolean probablyHasInput() {
            mCallCount++;
            return mReturnValue;
        }

        private boolean mReturnValue;
        private int mCallCount;
    }

    private static void checkHasInputOnUi(TestTextView view, boolean withThrottling) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view.mCallCount = 0;
                    // Do not check the return value because the view can be changed concurrently.
                    // Verify that there is no crash.
                    if (withThrottling) {
                        InputHintChecker.hasInputWithThrottlingForTesting();
                    } else {
                        InputHintChecker.hasInputForTesting();
                    }
                    Assert.assertEquals(1, view.mCallCount);
                });
    }

    @Test
    @MediumTest
    public void testReadHintDoesNotCrash() {
        // Start InputHintChecker asynchronous initialization by calling setView().
        TestTextView view =
                new TestTextView(InstrumentationRegistry.getInstrumentation().getTargetContext());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InputHintChecker.setAllowSetViewForTesting(true);
                    InputHintChecker.setView(view);
                });
        CriteriaHelper.pollUiThread(InputHintChecker::isInitializedForTesting);

        checkHasInputOnUi(view, /* withThrottling= */ false);
    }

    @Test
    @MediumTest
    public void testInitFailureReturnsFalse() {
        // Start InputHintChecker asynchronous initialization in a way to fail on a helper thread.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InputHintChecker.setAllowSetViewForTesting(true);
                    InputHintChecker.setWrongViewForTesting();
                });

        // Wait for initialization to fail.
        CriteriaHelper.pollUiThread(InputHintChecker::failedToInitializeForTesting);

        // Verify.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(InputHintChecker.hasInputForTesting());
                });
    }

    @Test
    @MediumTest
    public void testInitFailureRecordsHistogram() {
        try (var ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.InputHintChecker.InitializationResult", 1)) {
            // Start InputHintChecker asynchronous initialization in a way to fail on a helper
            // thread.
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        InputHintChecker.setAllowSetViewForTesting(true);
                        InputHintChecker.setWrongViewForTesting();
                    });
            // Wait for initialization to fail.
            CriteriaHelper.pollUiThread(InputHintChecker::failedToInitializeForTesting);
        }
    }

    @Test
    @MediumTest
    public void testReadHintDoesNotCrashWithThrottling() {
        // Start InputHintChecker asynchronous initialization by calling setView().
        TestTextView view =
                new TestTextView(InstrumentationRegistry.getInstrumentation().getTargetContext());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InputHintChecker.setAllowSetViewForTesting(true);
                    InputHintChecker.setView(view);
                });
        CriteriaHelper.pollUiThread(InputHintChecker::isInitializedForTesting);

        checkHasInputOnUi(view, /* withThrottling= */ true);
    }

    @Test
    @MediumTest
    public void testReadHintBeforeFinishInitialization() {
        // Start InputHintChecker asynchronous initialization by calling setView().
        TestTextView view =
                new TestTextView(InstrumentationRegistry.getInstrumentation().getTargetContext());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InputHintChecker.setAllowSetViewForTesting(true);
                    InputHintChecker.setView(view);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // The initialization is likely not finished, hence do not verify that the call
                    // to probablyHasInput() happened. Verify that there is no crash.
                    InputHintChecker.hasInputForTesting();
                });
    }

    @Test
    @MediumTest
    public void testInitAndCheckHint() {
        // Start InputHintChecker asynchronous initialization by calling setView().
        TestTextView view =
                new TestTextView(InstrumentationRegistry.getInstrumentation().getTargetContext());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InputHintChecker.setAllowSetViewForTesting(true);
                    InputHintChecker.setView(view);
                });

        // Wait for initialization to finish.
        CriteriaHelper.pollUiThread(InputHintChecker::isInitializedForTesting);

        // Verify that InputHintChecker.probablyHasInput() can return |true|.
        view.mReturnValue = true;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InputHintChecker.setView(view);
                    view.mCallCount = 0;
                    Assert.assertTrue(InputHintChecker.hasInputForTesting());
                    Assert.assertEquals(1, view.mCallCount);
                });

        // Verify that InputHintChecker.probablyHasInput() can return |false|.
        view.mReturnValue = false;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InputHintChecker.setView(view);
                    view.mCallCount = 0;
                    Assert.assertFalse(InputHintChecker.hasInputForTesting());
                    Assert.assertEquals(1, view.mCallCount);
                });

        // Verify that the input hint can go back to |true|.
        view.mReturnValue = true;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InputHintChecker.setView(view);
                    Assert.assertTrue(InputHintChecker.hasInputForTesting());
                });
    }
}
