// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.hamcrest.core.StringStartsWith.startsWith;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Assume;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ThreadUtils.ThreadChecker;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.BuildConfig;

/** Unit tests for ThreadUtils. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ThreadUtilsTest {
    @Test
    @SmallTest
    public void testThreadChecker_uiThread() {
        Assume.assumeTrue(BuildConfig.ENABLE_ASSERTS);
        ThreadChecker checker = new ThreadChecker();
        checker.assertOnValidThread();

        RuntimeException e =
                Assert.assertThrows(
                        RuntimeException.class,
                        () ->
                                PostTask.runSynchronously(
                                        TaskTraits.USER_BLOCKING, checker::assertOnValidThread));
        Assert.assertThat(
                e.getCause().getMessage(),
                startsWith("UI-only class called from background thread"));
    }

    @Test
    @SmallTest
    public void testThreadChecker_backgroundThread() {
        Assume.assumeTrue(BuildConfig.ENABLE_ASSERTS);
        ThreadChecker[] checkerHolder = new ThreadChecker[1];
        PostTask.runSynchronously(
                TaskTraits.USER_BLOCKING,
                () -> {
                    checkerHolder[0] = new ThreadChecker();
                });

        AssertionError e =
                Assert.assertThrows(AssertionError.class, checkerHolder[0]::assertOnValidThread);
        Assert.assertThat(
                e.getMessage(), startsWith("Background-only class called from UI thread"));
    }
}
