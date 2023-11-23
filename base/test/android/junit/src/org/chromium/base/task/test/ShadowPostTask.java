// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task.test;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.util.ReflectionHelpers.ClassParameter;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

/** Shadow implementation for {@link PostTask}. */
@Implements(PostTask.class)
public class ShadowPostTask {
    @FunctionalInterface
    public interface TestImpl {
        void postDelayedTask(@TaskTraits int taskTraits, Runnable task, long delay);
    }

    private static TestImpl sTestImpl;

    /** Set implementation for tests. */
    public static void setTestImpl(TestImpl testImpl) {
        sTestImpl = testImpl;
        ResettersForTesting.register(ShadowPostTask::reset);
    }

    /** Resets the {@link TestImpl} instance, undoing any shadowing. */
    @Resetter
    public static void reset() {
        sTestImpl = null;
    }

    @Implementation
    public static void postDelayedTask(@TaskTraits int taskTraits, Runnable task, long delay) {
        if (sTestImpl == null) {
            // Can use reflection to call into the real method that is being shadowed. This is the
            // same as not having a shadow.
            Shadow.directlyOn(
                    PostTask.class,
                    "postDelayedTask",
                    ClassParameter.from(int.class, taskTraits),
                    ClassParameter.from(Runnable.class, task),
                    ClassParameter.from(long.class, delay));
        } else {
            sTestImpl.postDelayedTask(taskTraits, task, delay);
        }
    }
}
