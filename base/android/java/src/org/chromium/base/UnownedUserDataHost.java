// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Handler;
import android.os.Looper;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.lifetime.DestroyChecker;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;

/**
 * UnownedUserDataHost is a type-safe and heterogeneous container that does not own the objects that
 * are stored within. It has the ability to associate a key of type {@code UnownedUserDataKey<T>},
 * where {@code T extends UnownedUserData}, with an instance of {@code T}.
 * <p>
 * Mismatch of types between key and value type information can be checked at compile time, which
 * ensures it is not possible to insert or retrieve data where the types do not match. Neither the
 * key nor the object is allowed to be {@code null}.
 * <p>
 * Value objects are held using {@link WeakReference} in the container, which means that they can be
 * garbage collected once the last strong reference has been removed. The {@link UnownedUserDataKey}
 * is still a strong reference, so it is important that it does not have a reference to the object
 * it is used as a key for. When trying to retrieve a garbage collected item for which a key is
 * still held, the entry in the map is removed during the invocation.
 * <p>
 * Invoking {@link #destroy()} clears out the map, including both keys and the {@link
 * WeakReference}s to the {@link UnownedUserData}s, making them available for garbage collection.
 * This enables the garbage collector to not be blocked on this class for continuing the garbage
 * collection cycle. During this process, all {@link UnownedUserData} objects are informed that they
 * have been detached.
 * <p>
 * All interaction with the UnownedUserDataHost must be performed on the same thread.
 * <p>
 * {@link UnownedUserData} is somewhat similar to {@link org.chromium.base.UserData}, except that it
 * is not owned by the host. The structure is also a bit different since the instances are retrieved
 * through a {@link UnownedUserDataKey} instead of the class type itself. The reason for this is to
 * ensure that we protect against accidental incorrect usage where something has been made
 * accessible through misconfigured GN visibility rules, incorrect package visibility or
 * misconfigured DEPS rules. In addition, it enforces clients to go through the from-method to
 * retrieve the object, ensuring that control stays with the object itself.
 * <p>
 * All methods on UnownedUserDataHost except {@link #destroy()} is package protected to ensure all
 * interaction with the host goes through the {@link UnownedUserDataKey}.
 * <p>
 * Example usage:
 * <pre>{@code
 * public class HolderClass {
 *     // Defines the container.
 *     private final UnownedUserDataHost mUnownedUserDataHost = new UnownedUserDataHost();
 *
 *     public UnownedUserDataHost getUnownedUserDataHost() {
 *         return mUnownedUserDataHost;
 *     }
 * }
 *
 * public class Foo implements UnownedUserData {
 *     // Keeping KEY private enforces acquisition by calling #from(), therefore Foo is in control
 *     // of getting the instance.
 *     private static final UnownedUserDataKey<Foo> KEY = new UnownedUserDataKey<>(Foo.class);
 *
 *     // The UnownedUserData framework enables this method in particular.
 *     public static Foo from(HolderClass holder) {
 *         return KEY.retrieveDataFromHost(holderClass.getUnownedUserDataHost());
 *     }
 *
 *     public void initialize(HolderClass holderClass) {
 *         // This could also be in the constructor or somewhere else that is reasonable for a
 *         // particular object.
 *         KEY.attachToHost(holderClass.getUnownedUserDataHost(), this);
 *     }
 *
 *     public void destroy() {
 *         // This ensures that the UnownedUserData can not be resurrected through any
 *         // UnownedUserDataHost after this.
 *         // For detaching from a particular host, use KEY.detachFromHost(host) instead.
 *         KEY.detachFromAllHosts(this);
 *     }
 * }
 *
 *
 *    // After construction, `foo` needs to attach itself to the HolderClass instance of the
 *    // UnownedUserDataHost.
 *    // Depending on who owns Foo, this could be its factory, or some other ownership model. Foo
 *    // does not need to hold on to the HolderClass, as that is taken care of by the key during
 *    // attachment. It is up to the implementor to decide whether this is in the constructor, or
 *    // in a separate initialize step.
 *    Foo foo = new Foo();
 *    foo.initialize(holderClass);
 *
 *    ...
 *
 *    // Now that the instance of Foo is attached to the particular instance of Holder, it
 *    // can be retrieved using just the HolderClass instance.
 *    Foo sameFoo = Foo.from(holderClass);
 *
 *    ...
 *
 *    // During destruction of `foo`, it must remove itself from the instance of HolderClass to
 *    // ensure that it can not be retrieved using that path any longer.
 *    foo.destroy();
 * }
 * </pre>
 * <p>
 * The code snippet above uses a {@code static} key to be able to facilitate the method {@code
 * public static Foo from(HolderClass holderClass)}, since it would not be possible to retrieve the
 * private key from that method if it was an instance member.
 * <p>
 * The code snippet above also assumes that {@code Foo} has knowledge about the {@code HolderClass},
 * instead of taking in a {@link UnownedUserDataHost} in the {@code from} method, since that
 * typically provides a more pleasant experience for users.
 * <p>
 * There is also another common pattern for retrieving an attached object, and that is to do it
 * lazily:
 * <pre>{@code
 * public static Foo from(HolderClass holderClass) {
 *     Foo foo = KEY.retrieveDataFromHost(holderClass.getUnownedUserDataHost());
 *     if (foo == null) {
 *         foo = new Foo();
 *         KEY.attachToHost(holderClass.getUnownedUserDataHost(), foo);
 *     }
 *     return foo;
 * }
 * }
 * </pre>
 * <p>
 * However, it is important to note that in this scenario, as soon as the code that invokes
 * from(...) drops the reference, Foo will be eligible for garbage collection since the host only
 * holds a {@link WeakReference}. This means that Foo could end up being constructed and garbage
 * collected often, depending on whether the caller holds on to a strong reference or not.
 *
 * @see UnownedUserDataKey for information about the type of key that is required.
 * @see UnownedUserData for the marker interface used for this type of data.
 */
public final class UnownedUserDataHost {
    private static Looper retrieveNonNullLooperOrThrow() {
        Looper looper = Looper.myLooper();
        if (looper == null) throw new IllegalStateException();
        return looper;
    }

    private final ThreadUtils.ThreadChecker mThreadChecker = new ThreadUtils.ThreadChecker();
    private final DestroyChecker mDestroyChecker = new DestroyChecker();

    /**
     * Handler to use to post {@link UnownedUserData#onDetachedFromHost(UnownedUserDataHost)}
     * invocations to.
     */
    private Handler mHandler;

    /**
     * The core data structure within this host.
     */
    private HashMap<UnownedUserDataKey<?>, WeakReference<? extends UnownedUserData>>
            mUnownedUserDataMap = new HashMap<>();

    public UnownedUserDataHost() {
        this(new Handler(retrieveNonNullLooperOrThrow()));
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    /* package */ UnownedUserDataHost(Handler handler) {
        mHandler = handler;
    }

    /**
     * Stores a {@link WeakReference} to {@code object} using the given {@code key}.
     * <p>
     * If the key is already attached to a different host, it is detached from that host.
     *
     * @param key    the key to use for the object.
     * @param newValue the object to store.
     * @param <T>    the type of {@link UnownedUserData}.
     */
    /* package */<T extends UnownedUserData> void set(
            @NonNull UnownedUserDataKey<T> key, @NonNull T newValue) {
        checkState();

        // If we already have data, we might want to detach that first.
        if (mUnownedUserDataMap.containsKey(key)) {
            T currentValue = get(key);
            // If we are swapping objects, inform the previous object of detachment.
            if (!newValue.equals(currentValue)) key.detachFromHost(this);
        }

        mUnownedUserDataMap.put(key, new WeakReference<>(newValue));
    }

    /**
     * Retrieves the {@link UnownedUserData} object stored under the given key.
     *
     * @param key the key to use for the object.
     * @param <T> the type of {@link UnownedUserData}.
     * @return the stored version or {@code null} if it is not stored or has been garbage collected.
     */
    @Nullable
    /* package */<T extends UnownedUserData> T get(@NonNull UnownedUserDataKey<T> key) {
        checkState();

        WeakReference<? extends UnownedUserData> valueWeakRef = mUnownedUserDataMap.get(key);
        if (valueWeakRef == null) return null;
        UnownedUserData value = valueWeakRef.get();
        if (value == null) {
            // The object the entry referenced has now been GCed, so remove the entry.
            key.detachFromHost(this);
            return null;
        }
        return key.getValueClass().cast(value);
    }

    /**
     * Removes the {@link UnownedUserData} object stored under the given key, if any.
     *
     * @param key the key to use for the object.
     * @param <T> the type of {@link UnownedUserData}.
     */
    /* package */<T extends UnownedUserData> void remove(@NonNull UnownedUserDataKey<T> key) {
        checkState();

        WeakReference<? extends UnownedUserData> valueWeakRef = mUnownedUserDataMap.remove(key);
        if (valueWeakRef == null) return;

        UnownedUserData value = valueWeakRef.get();
        // Invoking anything on `value` might be re-entrant for the caller so responses should be
        // posted. However, the informOnDetachmentFromHost() method contains a documented warning
        // that it might be re-entrant, so it is OK to use that to guard the call to
        // `onDetachedFromHost(...)`.
        if (value != null && value.informOnDetachmentFromHost()) {
            mHandler.post(() -> value.onDetachedFromHost(this));
        }
    }

    /**
     * Destroys the UnownedUserDataHost by clearing out the map, making the objects stored within
     * available for garbage collection as early as possible, in case the object owning the
     * UnownedUserDataHost stays alive for a while after destroy() has been invoked.
     * <p>
     * Objects stored within the UnownedUserDataHost are informed of this destroy() call through
     * {@link UnownedUserData#onDetachedFromHost(UnownedUserDataHost)}, and the {@link
     * UnownedUserDataKey} instances are updated to not refer to this host anymore.
     */
    public void destroy() {
        mThreadChecker.assertOnValidThread();

        // Protect against potential races.
        if (mDestroyChecker.isDestroyed()) return;

        // Create a shallow copy of all keys to ensure each held object can safely remove itself
        // from the map while iterating over their keys.
        Set<UnownedUserDataKey<?>> keys = new HashSet<>(mUnownedUserDataMap.keySet());
        for (UnownedUserDataKey<?> key : keys) key.detachFromHost(this);

        mUnownedUserDataMap = null;
        mHandler = null;

        // Need to wait until the end to destroy the ThreadChecker to ensure that the
        // detachFromHost(...) invocations above are allowed to invoke remove(...).
        mDestroyChecker.destroy();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    /* package */ int getMapSize() {
        checkState();

        return mUnownedUserDataMap.size();
    }

    /* package */ boolean isDestroyed() {
        return mDestroyChecker.isDestroyed();
    }

    private void checkState() {
        mThreadChecker.assertOnValidThread();
        mDestroyChecker.checkNotDestroyed();
    }
}
