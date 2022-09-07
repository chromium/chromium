// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.BuildConfig;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Objects;
import java.util.Set;
import java.util.WeakHashMap;

/**
 * UnownedUserDataKey is used in conjunction with a particular {@link UnownedUserData} as the key
 * for that when it is added to an {@link UnownedUserDataHost}.
 * <p>
 * This key is supposed to be private and not visible to other parts of the code base. Instead of
 * using the class as a key like in owned {@link org.chromium.base.UserData}, for {@link
 * UnownedUserData}, a particular object is used, ensuring that even if a class is visible outside
 * its own module, the instance of it as referenced from a {@link UnownedUserDataHost}, can not be
 * retrieved.
 * <p>
 * In practice, instances will typically be stored on this form:
 *
 * <pre>{@code
 * public class Foo implements UnownedUserData {
 *     private static final UnownedUserDataKey<Foo> KEY = new UnownedUserDataKey<>(Foo.class);
 *     ...
 * }
 * }
 * </pre>
 * <p>
 * This class and all its methods are final to ensure that no usage of the class leads to leaking
 * data about the object it is used as a key for.
 * <p>
 * It is OK to attach this key to as many different {@link UnownedUserDataHost} instances as
 * necessary, but doing so requires the client to invoke either {@link
 * #detachFromHost(UnownedUserDataHost)} or {@link #detachFromAllHosts(UnownedUserData)} during
 * cleanup.
 * <p>
 * Guarantees provided by this class together with {@link UnownedUserDataHost}:
 * <ul>
 * <li> One key can be used for multiple {@link UnownedUserData}s.
 * <li> One key can be attached to multiple {@link UnownedUserDataHost}s.
 * <li> One key can be attached to a particular {@link UnownedUserDataHost} only once. This ensures
 * a pair of {@link UnownedUserDataHost} and UnownedUserDataKey can only refer to a single
 * UnownedUserData.
 * <li> When a {@link UnownedUserData} is detached from a particular host, it is informed of this,
 * except if it has been garbage collected.
 * <li> When an {@link UnownedUserData} object is replaced with a different {@link UnownedUserData}
 * using the same UnownedUserDataKey, the former is detached.
 * </ul>
 *
 * @param <T> The Class this key is used for.
 * @see UnownedUserDataHost for more details on ownership and typical usage.
 * @see UnownedUserData for the marker interface used for this type of data.
 */
public final class UnownedUserDataKey<T extends UnownedUserData> {
    @NonNull
    private final Class<T> mClazz;
    // A Set that uses WeakReference<UnownedUserDataHost> internally.
    private final Set<UnownedUserDataHost> mWeakHostAttachments =
            Collections.newSetFromMap(new WeakHashMap<>());

    /**
     * Constructs a key to use for attaching to a particular {@link UnownedUserDataHost}.
     *
     * @param clazz The particular {@link UnownedUserData} class.
     */
    public UnownedUserDataKey(@NonNull Class<T> clazz) {
        mClazz = clazz;
    }

    @NonNull
    /* package */ final Class<T> getValueClass() {
        return mClazz;
    }

    /**
     * Attaches the {@link UnownedUserData} object to the given {@link UnownedUserDataHost}, and
     * stores the host as a {@link WeakReference} to be able to detach from it later.
     *
     * @param host   The host to attach the {@code object} to.
     * @param object The object to attach.
     */
    public final void attachToHost(@NonNull UnownedUserDataHost host, @NonNull T object) {
        Objects.requireNonNull(object);
        // Setting a new value might lead to detachment of previously attached data, including
        // re-entry to this key, to happen before we update the {@link #mHostAttachments}.
        host.set(this, object);

        if (!isAttachedToHost(host)) {
            mWeakHostAttachments.add(host);
        }
    }

    /**
     * Attempts to retrieve the instance of the {@link UnownedUserData} from the given {@link
     * UnownedUserDataHost}. It will return {@code null} if the object is not attached to that
     * particular {@link UnownedUserDataHost} using this key, or the {@link UnownedUserData} has
     * been garbage collected.
     *
     * @param host The host to retrieve the {@link UnownedUserData} from.
     * @return The current {@link UnownedUserData} stored in the {@code host}, or {@code null}.
     */
    @Nullable
    public final T retrieveDataFromHost(@NonNull UnownedUserDataHost host) {
        assertNoDestroyedAttachments();
        for (UnownedUserDataHost attachedHost : mWeakHostAttachments) {
            if (host.equals(attachedHost)) {
                return host.get(this);
            }
        }
        return null;
    }

    /**
     * Detaches the key and object from the given host if it is attached with this key. It is OK to
     * call this for already detached objects.
     *
     * @param host The host to detach from.
     */
    public final void detachFromHost(@NonNull UnownedUserDataHost host) {
        assertNoDestroyedAttachments();
        for (UnownedUserDataHost attachedHost : new ArrayList<>(mWeakHostAttachments)) {
            if (host.equals(attachedHost)) {
                removeHostAttachment(attachedHost);
            }
        }
    }

    /**
     * Detaches the {@link UnownedUserData} from all hosts that it is currently attached to with
     * this key. It is OK to call this for already detached objects.
     *
     * @param object The object to detach from all hosts.
     */
    public final void detachFromAllHosts(@NonNull T object) {
        assertNoDestroyedAttachments();
        for (UnownedUserDataHost attachedHost : new ArrayList<>(mWeakHostAttachments)) {
            if (object.equals(attachedHost.get(this))) {
                removeHostAttachment(attachedHost);
            }
        }
    }

    /**
     * Checks if the {@link UnownedUserData} is currently attached to the given host with this key.
     *
     * @param host The host to check if the {@link UnownedUserData} is attached to.
     * @return true if currently attached, false otherwise.
     */
    public final boolean isAttachedToHost(@NonNull UnownedUserDataHost host) {
        T t = retrieveDataFromHost(host);
        return t != null;
    }

    /**
     * @return Whether the {@link UnownedUserData} is currently attached to any hosts with this key.
     */
    public final boolean isAttachedToAnyHost(@NonNull T object) {
        return getHostAttachmentCount(object) > 0;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    /* package */ int getHostAttachmentCount(@NonNull T object) {
        assertNoDestroyedAttachments();
        int ret = 0;
        for (UnownedUserDataHost attachedHost : mWeakHostAttachments) {
            if (object.equals(attachedHost.get(this))) {
                ret++;
            }
        }
        return ret;
    }

    private void removeHostAttachment(UnownedUserDataHost host) {
        host.remove(this);
        mWeakHostAttachments.remove(host);
    }

    private void assertNoDestroyedAttachments() {
        if (BuildConfig.ENABLE_ASSERTS) {
            for (UnownedUserDataHost attachedHost : mWeakHostAttachments) {
                if (attachedHost.isDestroyed()) {
                    assert false : "Host should have been removed already.";
                    throw new IllegalStateException();
                }
            }
        }
    }
}
