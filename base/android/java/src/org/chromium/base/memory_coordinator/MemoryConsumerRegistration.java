// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.memory_coordinator;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;

/**
 * Registers a MemoryConsumer with the global MemoryConsumerRegistry on the UI thread.
 *
 * <p>This class serves two roles: 1. An AutoCloseable handle representing an active consumer
 * registration. 2. An early-registration queue that holds consumer registrations created during
 * Java startup before the native C++ coordinator is initialized.
 *
 * <p>Callers must call close() when the consumer is destroyed to unregister and free native
 * resources.
 */
@JNINamespace("base::android")
@NullMarked
public final class MemoryConsumerRegistration implements AutoCloseable {

    // -------------------------------------------------------------------------
    // Core Registration Handle
    // -------------------------------------------------------------------------

    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    /** Pointer to the native MemoryConsumerAndroid peer instance (0 if pending or closed). */
    private long mNativeMemoryConsumerAndroid;

    /** Tracks whether this registration has been closed to ensure idempotent teardown. */
    private boolean mClosed;

    /**
     * Creates and registers a MemoryConsumer on the UI thread.
     *
     * <p>If the native MemoryConsumerRegistry is not yet initialized, registration is deferred and
     * completed lazily once native becomes available.
     *
     * @param consumerName Unique name identifying the consumer.
     * @param traits Traits describing how this consumer behaves.
     * @param consumer The MemoryConsumer instance receiving notifications.
     * @return A registration handle that unregisters the consumer when closed.
     */
    public static MemoryConsumerRegistration create(
            String consumerName, MemoryConsumerTraits traits, MemoryConsumer consumer) {
        ThreadUtils.assertOnUiThread();
        assert traits.getConsumerType() != ConsumerType.PASSIVE
                        || consumer instanceof PassiveMemoryConsumer
                : "Active MemoryConsumer registered with Passive traits: " + consumerName;

        MemoryConsumerRegistration registration = new MemoryConsumerRegistration();

        if (sNativeRegistryReady) {
            registration.registerInternal(consumerName, traits, consumer);
        } else {
            sPendingRegistrations.add(
                    new PendingRegistration(registration, consumerName, traits, consumer));
        }
        return registration;
    }

    private MemoryConsumerRegistration() {}

    /** Performs registration with the native C++ registry if not already registered. */
    private void registerInternal(
            String consumerName, MemoryConsumerTraits traits, MemoryConsumer consumer) {
        if (mNativeMemoryConsumerAndroid == 0 && !mClosed) {
            mNativeMemoryConsumerAndroid =
                    MemoryConsumerRegistrationJni.get().register(consumerName, traits, consumer);
            MemoryConsumerRegistrationJni.get()
                    .notifyInitialLimitIfNonDefault(mNativeMemoryConsumerAndroid);
        }
    }

    @Override
    public void close() {
        ThreadUtils.assertOnUiThread();
        if (mClosed) return;
        mClosed = true;
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
        sPendingRegistrations.removeIf(pending -> pending.mRegistration == this);
        if (mNativeMemoryConsumerAndroid != 0) {
            MemoryConsumerRegistrationJni.get().destroy(mNativeMemoryConsumerAndroid);
            mNativeMemoryConsumerAndroid = 0;
        }
    }

    // -------------------------------------------------------------------------
    // Early (Pre-Native) Registration Support
    //
    // Components in Java may initialize before native C++ libraries or the
    // MemoryConsumerRegistry are ready. Registrations created during this phase
    // are queued in sPendingRegistrations and automatically flushed to native
    // when flushPendingRegistrations() is called.
    // -------------------------------------------------------------------------

    private static final class PendingRegistration {
        final MemoryConsumerRegistration mRegistration;
        final String mConsumerName;
        final MemoryConsumerTraits mTraits;
        final MemoryConsumer mConsumer;

        PendingRegistration(
                MemoryConsumerRegistration registration,
                String consumerName,
                MemoryConsumerTraits traits,
                MemoryConsumer consumer) {
            mRegistration = registration;
            mConsumerName = consumerName;
            mTraits = traits;
            mConsumer = consumer;
        }
    }

    /** Queue of registrations created before native is ready. */
    private static final List<PendingRegistration> sPendingRegistrations = new ArrayList<>();

    /** Whether the native MemoryConsumerRegistry is currently initialized and ready. */
    private static boolean sNativeRegistryReady;

    /** Flushes any pending pre-native registrations once native is initialized. */
    @CalledByNative
    public static void flushPendingRegistrations() {
        // Native MemoryConsumerRegistry may be initialized on a background thread in tests
        // (e.g. NativePostTaskTest). Java MemoryConsumers are UI-thread only, so ignore
        // non-UI thread native registries.
        if (!ThreadUtils.runningOnUiThread()) {
            return;
        }
        sNativeRegistryReady = true;
        if (sPendingRegistrations.isEmpty()) return;
        List<PendingRegistration> pending = new ArrayList<>(sPendingRegistrations);
        sPendingRegistrations.clear();
        for (PendingRegistration item : pending) {
            if (!item.mRegistration.mClosed) {
                item.mRegistration.registerInternal(
                        item.mConsumerName, item.mTraits, item.mConsumer);
            }
        }
    }

    /** Called when the native MemoryConsumerRegistry is destroyed. */
    @CalledByNative
    public static void onNativeRegistryDestroyed() {
        if (!ThreadUtils.runningOnUiThread()) {
            return;
        }
        sNativeRegistryReady = false;
    }

    static void resetForTesting() {
        sNativeRegistryReady = false;
        sPendingRegistrations.clear();
    }

    // -------------------------------------------------------------------------
    // JNI Callback Dispatchers (Native -> Java)
    // -------------------------------------------------------------------------

    @CalledByNative
    static void onUpdateMemoryLimit(MemoryConsumer consumer, int memoryLimitPercent) {
        ThreadUtils.assertOnUiThread();
        consumer.onUpdateMemoryLimit(new MemoryLimit(memoryLimitPercent));
    }

    @CalledByNative
    static void onReleaseMemory(MemoryConsumer consumer) {
        ThreadUtils.assertOnUiThread();
        consumer.onReleaseMemory();
    }

    // -------------------------------------------------------------------------
    // Native Interface
    // -------------------------------------------------------------------------

    @NativeMethods
    public interface Natives {
        long register(String consumerName, MemoryConsumerTraits traits, MemoryConsumer consumer);

        void destroy(long nativeMemoryConsumerAndroid);

        void notifyInitialLimitIfNonDefault(long nativeMemoryConsumerAndroid);
    }
}
