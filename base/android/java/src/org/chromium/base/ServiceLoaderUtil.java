// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.util.ArrayMap;

import androidx.annotation.Nullable;

import org.chromium.build.annotations.AlwaysInline;

import java.util.Map;
import java.util.ServiceLoader;

/**
 * A wrapper around java.util.ServiceLoader, which ensures (with the help of R8) that ServiceLoader
 * calls are optimized away in release builds.
 *
 * <pre>
 * Example usage:
 *   // In release builds, this will be replace by either: "null" or "new MyImpl()".
 *   MyInterface impl = ServiceLoaderUtil.maybeCreate(MyInterface.class);
 *   if (impl != null) {
 *     impl.doThing();
 *   }
 *
 * To inject a singleton:
 *   public static @Nullable MyInterface getInstance() {
 *       MyProvider provider = ServiceLoaderUtil.maybeCreate(MyProvider.class);
 *       // So long as MyProviderImpl is stateless, provider.getInstance() will be inlined.
 *       return provider != null ? provider.getInstance() : null;
 *   }
 *
 * To configure MyInterfaceImpl:
 *     import org.chromium.build.annotations.ServiceImpl;
 *
 *     @ServiceImpl(MyInterface.class)
 *     public class MyInterfaceImpl implements MyInterface {
 *         // Must have a public no-arg constructor.
 *         ...
 *     }
 *
 * </pre>
 */
public final class ServiceLoaderUtil {
    private static Map<Class<?>, Object> sOverridesForTesting;

    private ServiceLoaderUtil() {}

    @AlwaysInline
    public static <T> @Nullable T maybeCreate(Class<T> clazz) {
        if (sOverridesForTesting != null) {
            Object ret = sOverridesForTesting.get(clazz);
            if (ret != null) {
                return clazz.cast(ret);
            }
        }

        var it = ServiceLoader.load(clazz, clazz.getClassLoader()).iterator();
        if (it.hasNext()) {
            T ret = it.next();
            assert !it.hasNext()
                    : String.format(
                            "Multiple implementations found for %s: %s and %s",
                            clazz, ret.getClass(), it.next().getClass());
            return ret;
        }
        return null;
    }

    @SuppressWarnings("NoSynchronizedMethodCheck")
    public static synchronized <T> void setInstanceForTesting(Class<T> clazz, T instance) {
        if (sOverridesForTesting == null) {
            sOverridesForTesting = new ArrayMap<>();
        }
        sOverridesForTesting.put(clazz, instance);
        ResettersForTesting.register(() -> sOverridesForTesting.remove(clazz));
    }
}
