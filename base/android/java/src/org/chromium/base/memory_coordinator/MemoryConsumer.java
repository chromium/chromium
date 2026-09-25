// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.memory_coordinator;

import org.chromium.build.annotations.NullMarked;

/**
 * Interface implemented by components to dynamically manage and adjust their memory usage.
 *
 * <p>By registering via MemoryConsumerRegistration, implementations are notified when they should
 * adjust their memory capacity or release memory: - onUpdateMemoryLimit(): Notifies the consumer of
 * an updated capacity limit (as a percentage relative to baseline). Implementations should update
 * their internal limit, but must not eagerly release memory here. - onReleaseMemory(): Invoked when
 * memory should be reclaimed. Implementations must free memory that exceeds their current memory
 * limit.
 *
 * <p>For a detailed description of the memory coordinator model, concepts, and policy, see
 * base/memory_coordinator/memory_consumer.h.
 */
@NullMarked
public interface MemoryConsumer {
    /**
     * Invoked when the memory limit assigned to this consumer is updated.
     *
     * <p>Implementations should update their internal capacity target (e.g. by scaling their
     * baseline limit using MemoryLimit.scale()), but must not eagerly free memory in this callback.
     *
     * @param memoryLimit The updated memory limit representing allowed capacity as a percentage.
     */
    void onUpdateMemoryLimit(MemoryLimit memoryLimit);

    /**
     * Invoked when memory above the current limit should be freed.
     *
     * <p>Implementations should discard cached resources or evict entries that exceed the limit
     * assigned in the most recent onUpdateMemoryLimit() call.
     */
    void onReleaseMemory();
}
