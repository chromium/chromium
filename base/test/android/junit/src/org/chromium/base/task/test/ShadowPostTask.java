// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task.test;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.util.ReflectionHelpers.ClassParameter;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

/**
 * Shadow implementation for {@link PostTask}.
 */
@Implements(PostTask.class)
public class ShadowPostTask {
    private static TestImpl sTestImpl = new TestImpl();

    /** Set implementation for tests. Don't forget to call {@link #reset} later. */
    public static void setTestImpl(TestImpl testImpl) {
        sTestImpl = testImpl;
    }

    @Resetter
    public static void reset() {
        sTestImpl = new TestImpl();
    }

    @Implementation
    public static void postDelayedTask(@TaskTraits int taskTraits, Runnable task, long delay) {
        sTestImpl.postDelayedTask(taskTraits, task, delay);
    }

    /** Default implementation for tests. Override methods or add new ones as necessary. */
    public static class TestImpl {
        public void postDelayedTask(@TaskTraits int taskTraits, Runnable task, long delay) {
            Shadow.directlyOn(PostTask.class, "postDelayedTask",
                    ClassParameter.from(int.class, taskTraits),
                    ClassParameter.from(Runnable.class, task),
                    ClassParameter.from(long.class, delay));
        }
    }
}
