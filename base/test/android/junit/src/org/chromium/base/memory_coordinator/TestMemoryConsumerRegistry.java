// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.memory_coordinator;

import org.jni_zero.JniUniquePtr;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.memory_coordinator.MemoryConsumerRegistration.NativeMemoryConsumerAndroid;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;

/**
 * A fake memory consumer registry for Robolectric tests. Java equivalent of the C++
 * base::TestMemoryConsumerRegistry.
 *
 * <p>Replaces the native side of {@link MemoryConsumerRegistration}, so that consumers registered
 * with {@link MemoryConsumerRegistration#create} can be notified directly from tests:
 *
 * <pre>{@code
 * TestMemoryConsumerRegistry registry = new TestMemoryConsumerRegistry();
 * Foo foo = new Foo(); // Registers a MemoryConsumer.
 * registry.notifyReleaseMemory();
 * }</pre>
 *
 * <p>Consumers registered before this registry is created are also added. The fake is automatically
 * uninstalled at the end of the test.
 */
@NullMarked
public final class TestMemoryConsumerRegistry implements MemoryConsumerRegistration.Natives {
    private static @Nullable TestMemoryConsumerRegistry sInstance;

    private final List<MemoryConsumer> mConsumers = new ArrayList<>();

    // The last limit passed to notifyUpdateMemoryLimit(). Applied to consumers that register
    // afterwards, like the production registry does.
    private MemoryLimit mCurrentMemoryLimit = MemoryLimit.DEFAULT;

    public TestMemoryConsumerRegistry() {
        assert sInstance == null : "Only one TestMemoryConsumerRegistry may exist at a time";
        sInstance = this;
        MemoryConsumerRegistrationJni.setInstanceForTesting(this);
        ResettersForTesting.register(
                () -> {
                    sInstance = null;
                    MemoryConsumerRegistrationJni.setInstanceForTesting(null);
                    MemoryConsumerRegistration.resetForTesting();
                });
        // Marks the registry as ready, which also registers any pending consumers.
        MemoryConsumerRegistration.flushPendingRegistrations();
    }

    // MemoryConsumerRegistration.Natives:
    @Override
    public JniUniquePtr<NativeMemoryConsumerAndroid> register(
            String consumerName, int packedTraits, MemoryConsumer consumer) {
        assert !mConsumers.contains(consumer) : "Consumer already registered: " + consumerName;
        mConsumers.add(consumer);
        if (!mCurrentMemoryLimit.equals(MemoryLimit.DEFAULT)) {
            consumer.onUpdateMemoryLimit(mCurrentMemoryLimit);
        }
        return () -> mConsumers.remove(consumer);
    }

    /**
     * Invokes onUpdateMemoryLimit(memoryLimit) on all consumers. The limit is also applied to
     * consumers registered afterwards.
     */
    public void notifyUpdateMemoryLimit(MemoryLimit memoryLimit) {
        mCurrentMemoryLimit = memoryLimit;
        for (MemoryConsumer consumer : new ArrayList<>(mConsumers)) {
            // Skip consumers unregistered by a previous notification.
            if (mConsumers.contains(consumer)) consumer.onUpdateMemoryLimit(memoryLimit);
        }
    }

    /** Invokes onReleaseMemory() on all consumers. */
    public void notifyReleaseMemory() {
        for (MemoryConsumer consumer : new ArrayList<>(mConsumers)) {
            // Skip consumers unregistered by a previous notification.
            if (mConsumers.contains(consumer)) consumer.onReleaseMemory();
        }
    }

    /** Invokes onUpdateMemoryLimit(memoryLimit) followed by onReleaseMemory() on all consumers. */
    public void notifyUpdateMemoryLimitAndRelease(MemoryLimit memoryLimit) {
        notifyUpdateMemoryLimit(memoryLimit);
        notifyReleaseMemory();
    }

    /** Returns the number of registered consumers. */
    public int size() {
        return mConsumers.size();
    }
}
