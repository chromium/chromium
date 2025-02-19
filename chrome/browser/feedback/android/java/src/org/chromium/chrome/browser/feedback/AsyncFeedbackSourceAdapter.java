// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.AsyncTask.Status;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.concurrent.ExecutionException;

/**
 * A helper class to make implementing an AsyncFeedbackSource easier for the common case.  The bulk
 * of the background work is meant to be done in {@link #doInBackground(Context)} and the result can
 * be queried from {@link #getResult()}.  Subclasses are meant to override {@link #getFeedback()} or
 * {@link #getLogs()} as necessary and use {@link #getResult()} if they need the result from the
 * asynchronous work.
 * @param <Result> The {@link Object} type that represents the result of doing the background work.
 */
@NullMarked
public abstract class AsyncFeedbackSourceAdapter<Result> implements AsyncFeedbackSource {
    private @Nullable Worker mWorker;

    private class Worker extends AsyncTask<@Nullable Result> {
        private final Runnable mCallback;

        public Worker(Runnable callback) {
            mCallback = callback;
        }

        // AsyncTask implementation.
        @Override
        protected @Nullable Result doInBackground() {
            return AsyncFeedbackSourceAdapter.this.doInBackground(
                    ContextUtils.getApplicationContext());
        }

        @Override
        protected void onPostExecute(@Nullable Result result) {
            mCallback.run();
        }
    }

    /**
     * Meant to do the actual work in the background.  This method will be called from a thread in
     * the {@link AsyncTask} thread pool.
     * @param context The application {@link Context}.
     * @return        The result of doing the work in the background or {@code null}.
     */
    protected abstract @Nullable Result doInBackground(Context context);

    /**
     * @return The result of the background work if it has been started and finished.  This will be
     *         null if the underlying background task has not finished yet (see {@link #isReady()})
     *         or if {@link #doInBackground(Context)} returned {@code null}.
     */
    protected final @Nullable Result getResult() {
        try {
            return mWorker != null && mWorker.getStatus() == Status.FINISHED ? mWorker.get() : null;
        } catch (ExecutionException | InterruptedException e) {
            return null;
        }
    }

    // AsyncFeedbackSource implementation.
    @Override
    public final boolean isReady() {
        return mWorker != null && mWorker.getStatus() == Status.FINISHED;
    }

    @Override
    public final void start(Runnable callback) {
        if (mWorker != null) return;
        mWorker = new Worker(callback);
        // USER_BLOCKING since we eventually .get() this.
        mWorker.executeWithTaskTraits(TaskTraits.USER_BLOCKING_MAY_BLOCK);
    }
}
