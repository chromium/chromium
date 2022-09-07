// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

/**
 * An AsyncTask which does not require post-execution.
 *
 * The addition of this class is only temporary with the eventual goal of
 * transitioning all such tasks to FutureTasks / Runnables.
 *
 * @param <Result> Return type of the background task.
 */
public abstract class BackgroundOnlyAsyncTask<Result> extends AsyncTask<Result> {
    @Override
    protected final void onPostExecute(Result result) {
        // This method should never be executed for background-only tasks.
        assert false;
    }
}
