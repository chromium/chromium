// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import org.chromium.base.ThreadUtils;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;

/** Helper methods to deal with threading related tasks in tests. */
public class TestThreadUtils {
    /**
     * Since PostTask goes through c++, and order is not guaranteed between c++ and Java Handler
     * tasks, in many cases it's not sufficient to PostTask to the UI thread to check if UI
     * properties like visibility are updated in response to a click, as UI updates go through a
     * Java Handler and may run after the posted PostTask task.
     *
     * You should strongly prefer to wait on the signal you care about (eg. use Espresso to wait for
     * a view to be displayed) instead of flushing tasks.
     */
    public static void flushNonDelayedLooperTasks() {
        if (ThreadUtils.runningOnUiThread()) return;
        var task = new FutureTask<Void>(() -> {}, null);
        ThreadUtils.getUiThreadHandler().post(task);
        try {
            task.get();
        } catch (InterruptedException | ExecutionException e) {
        }
    }

    /** Wraps Thread.sleep(). */
    public static void sleep(int millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException e) {
        }
    }
}
