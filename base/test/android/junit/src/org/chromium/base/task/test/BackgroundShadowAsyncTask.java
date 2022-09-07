// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task.test;

import static org.junit.Assert.fail;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.base.task.AsyncTask;

import java.util.concurrent.Callable;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Executes async tasks on a background thread and waits on the result on the main thread.
 * This is useful for users of AsyncTask on Roboelectric who check if the code is actually being
 * run on a background thread (i.e. through the use of {@link ThreadUtils#runningOnUiThread()}).
 * @param <Result>     type for reporting result
 */
@Implements(AsyncTask.class)
public class BackgroundShadowAsyncTask<Result> extends ShadowAsyncTask<Result> {
    private static final ExecutorService sExecutorService = Executors.newSingleThreadExecutor();

    @Override
    @Implementation
    public final AsyncTask<Result> executeOnExecutor(Executor e) {
        try {
            return sExecutorService
                    .submit(new Callable<AsyncTask<Result>>() {
                        @Override
                        public AsyncTask<Result> call() {
                            return BackgroundShadowAsyncTask.super.executeInRobolectric();
                        }
                    })
                    .get();
        } catch (Exception ex) {
            fail(ex.getMessage());
            return null;
        }
    }

    @Override
    @Implementation
    public final Result get() {
        try {
            runBackgroundTasks();
            return BackgroundShadowAsyncTask.super.get();
        } catch (Exception e) {
            return null;
        }
    }

    public static void runBackgroundTasks() throws Exception {
        sExecutorService
                .submit(new Runnable() {
                    @Override
                    public void run() {
                        ShadowApplication.runBackgroundTasks();
                    }
                })
                .get();
    }
}
