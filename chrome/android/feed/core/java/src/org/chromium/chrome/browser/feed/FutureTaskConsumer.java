// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.common.concurrent.SimpleSettableFuture;
import com.google.android.libraries.feed.common.functional.Consumer;

import org.chromium.base.Callback;
import org.chromium.base.Log;

import java.util.concurrent.ExecutionException;

/**
 * Processes a {@link Callback} on a {@link SimpleSettableFuture}. This is necessary to provide a
 * synchronous interface to the Feed consumer.
 */
public class FutureTaskConsumer {
    private static final String TAG = "FutureTaskConsumer";
    /**
     * Sets the task result on a Future and returns it.
     *
     * @param location Caller location for error logging.
     * @param task Callback on which to run the results.
     * @param failure Default result in case of failure.
     * @param <T> Type of result expected by the Feed {@link Consumer}.
     * @return Result of the task.
     */
    public static <T> T consume(String location, Callback<Consumer<T>> task, T failure) {
        SimpleSettableFuture<T> sharedStatesFuture = new SimpleSettableFuture<>();
        task.onResult(sharedStatesFuture::put);
        try {
            return sharedStatesFuture.get();
        } catch (InterruptedException | ExecutionException e) {
            Log.e(TAG, "%s: %s", location, e.toString());
            return failure;
        }
    }
}
