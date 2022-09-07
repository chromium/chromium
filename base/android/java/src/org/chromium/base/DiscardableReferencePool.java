// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.annotation.Nullable;

import java.util.Collections;
import java.util.Set;
import java.util.WeakHashMap;

/**
 * A DiscardableReferencePool allows handing out typed references to objects ("payloads") that can
 * be dropped in one batch ("drained"), e.g. under memory pressure. In contrast to {@link
 * java.lang.ref.WeakReference}s, which drop their referents when they get garbage collected, a
 * reference pool gives more precise control over when exactly it is drained.
 *
 * <p>Internally it uses a {@link WeakHashMap} with the reference itself as a key to allow the
 * payloads to be garbage collected regularly when the last reference goes away before the pool is
 * drained.
 *
 * <p>This class and its references are not thread-safe and should not be used simultaneously by
 * multiple threads.
 */
public class DiscardableReferencePool {
    /**
     * The underlying data storage. The wildcard type parameter allows using a single pool for
     * references of any type.
     */
    private final Set<DiscardableReference<?>> mPool;

    public DiscardableReferencePool() {
        WeakHashMap<DiscardableReference<?>, Boolean> map = new WeakHashMap<>();
        mPool = Collections.newSetFromMap(map);
    }

    /**
     * A reference to an object in the pool. Will be nulled out when the pool is drained.
     * @param <T> The type of the object.
     */
    public static class DiscardableReference<T> {
        @Nullable
        private T mPayload;

        private DiscardableReference(T payload) {
            assert payload != null;
            mPayload = payload;
        }

        /**
         * @return The referent, or null if the pool has been drained.
         */
        @Nullable
        public T get() {
            return mPayload;
        }

        /**
         * Clear the referent.
         */
        private void discard() {
            assert mPayload != null;
            mPayload = null;
        }
    }

    /**
     * @param <T> The type of the object.
     * @param payload The payload to add to the pool.
     * @return A new reference to the {@code payload}.
     */
    public <T> DiscardableReference<T> put(T payload) {
        assert payload != null;
        DiscardableReference<T> reference = new DiscardableReference<>(payload);
        mPool.add(reference);
        return reference;
    }

    /**
     * Remove this reference from the pool, allowing garbage collection to pick it up.
     *
     * @param ref The discardable reference to remove.
     */
    public void remove(DiscardableReference<?> ref) {
        assert ref != null;
        if (!mPool.contains(ref)) return;
        assert ref.get() != null;

        ref.discard();
        mPool.remove(ref);
    }

    /**
     * Drains the pool, removing all references to objects in the pool and therefore allowing them
     * to be garbage collected.
     */
    public void drain() {
        for (DiscardableReference<?> ref : mPool) {
            ref.discard();
        }
        mPool.clear();
    }
}
