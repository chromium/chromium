// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1;

import org.junit.Assert;

import org.chromium.base.Consumer;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/**
 * ConsumerSyncWrapper allows the user to wait for the wrapped consumer to
 * accept an input.
 */
class ConsumerSyncWrapper<T> implements Consumer<T> {
    private final Consumer<T> mWrapped;
    private final AtomicReference<T> mResult;
    private final CountDownLatch mLatch;
    private ConsumerSyncWrapper(Consumer<T> wrappedConsumer) {
        mWrapped = wrappedConsumer;
        mResult = new AtomicReference<>();
        mLatch = new CountDownLatch(1);
    }
    private void blockAndWrappedAccept(long timeoutMs) {
        try {
            mLatch.await(timeoutMs, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            Assert.fail("waitForAccept was interrupted: " + e.getMessage());
        }
        mWrapped.accept(mResult.get());
    }
    @Override
    public void accept(T input) {
        mResult.set(input);
        mLatch.countDown();
    }

    /**
     * waitForConsumer calls the consumer operation on the UI Thread then blocks until the
     * consumer has accepted the results of the operation.
     *
     * @param consumer The {@link Consumer} to block on.
     * @param operation The operation that should feed its result to the consumer.
     * @param timeoutMs The timeout in milliseconds to wait for operation to execute
     */
    public static <T> void waitForConsumer(
            Consumer<T> consumer, Consumer<Consumer<T>> operation, long timeoutMs) {
        ConsumerSyncWrapper<T> wrapper = new ConsumerSyncWrapper<>(consumer);
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> operation.accept(wrapper));
        wrapper.blockAndWrappedAccept(timeoutMs);
    }
}
