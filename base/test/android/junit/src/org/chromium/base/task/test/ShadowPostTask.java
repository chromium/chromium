// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task.test;

import org.robolectric.Robolectric;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

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
    public static void postDelayedTask(TaskTraits taskTraits, Runnable task, long delay) {
        sTestImpl.postDelayedTask(taskTraits, task, delay);
    }

    /** Default implementation for tests. Override methods or add new ones as necessary. */
    public static class TestImpl {
        public void postDelayedTask(TaskTraits taskTraits, Runnable task, long delay) {
            Robolectric.getForegroundThreadScheduler().postDelayed(task, delay);
        }
    }
}
