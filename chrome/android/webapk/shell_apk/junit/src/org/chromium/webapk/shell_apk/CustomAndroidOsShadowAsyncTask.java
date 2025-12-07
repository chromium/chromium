// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowLegacyAsyncTask;

import java.util.concurrent.Executor;

/**
 * Forces async tasks to execute with the default executor. This works around the problem of
 * Robolectric not working out of the box with custom executors.
 */
@SuppressWarnings("NoAndroidAsyncTaskCheck")
@Implements(android.os.AsyncTask.class)
public class CustomAndroidOsShadowAsyncTask<Params, Progress, Result>
        extends ShadowLegacyAsyncTask<Params, Progress, Result> {
    @Override
    @Implementation
    public final android.os.AsyncTask<Params, Progress, Result> executeOnExecutor(
            Executor executor, Params... params) {
        return super.execute(params);
    }
}
