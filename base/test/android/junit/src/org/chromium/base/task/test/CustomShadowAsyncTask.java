// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task.test;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.task.AsyncTask;

import java.util.concurrent.Executor;

/**
 * Forces async tasks to execute with the default executor. This works around Robolectric not
 * working out of the box with custom executors.
 */
@Implements(AsyncTask.class)
public class CustomShadowAsyncTask<Result> extends ShadowAsyncTask<Result> {
    @Override
    @Implementation
    public final AsyncTask<Result> executeOnExecutor(Executor executor) {
        return super.executeInRobolectric();
    }
}
