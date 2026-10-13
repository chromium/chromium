// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.http_headers;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;
import java.util.function.Consumer;
import java.util.function.Function;
import java.util.function.Supplier;

/** Manages AwHeaderInterceptor registrations. */
@JNINamespace("android_webview")
@NullMarked
public class AwHeaderInterceptorStore {
    @JniType("android_webview::AwHeaderInterceptorStore")
    public interface Native extends JniTypeToken {}

    private final Supplier<@Nullable JniRawPtr<Native>> mNativeSupplier;

    public AwHeaderInterceptorStore(Supplier<@Nullable JniRawPtr<Native>> nativeSupplier) {
        mNativeSupplier = nativeSupplier;
    }

    /**
     * Adds a header interceptor for requests matching the given URL patterns.
     *
     * @param interceptor The interceptor to add.
     * @param urlPatterns The URL patterns to match against.
     * @return The ID of the registered interceptor.
     */
    public int addInterceptor(AwHeaderInterceptor interceptor, List<String> urlPatterns) {
        ThreadUtils.assertOnUiThread();
        return withNativeOrThrow(
                nativePointer ->
                        AwHeaderInterceptorStoreJni.get()
                                .addInterceptorFromJava(nativePointer, interceptor, urlPatterns));
    }

    /**
     * Removes the interceptor associated with the given ID.
     *
     * @param id The ID returned by {@link #addInterceptor}.
     */
    public void removeInterceptor(int id) {
        ThreadUtils.assertOnUiThread();
        runWithNativeOrThrow(
                nativePointer ->
                        AwHeaderInterceptorStoreJni.get().removeInterceptor(nativePointer, id));
    }

    /** Removes all registered interceptors. */
    public void clearInterceptors() {
        ThreadUtils.assertOnUiThread();
        runWithNativeOrThrow(AwHeaderInterceptorStoreJni.get()::clearInterceptors);
    }

    /** Gets the number of registered interceptors. */
    public int getInterceptorCountForTesting() {
        ThreadUtils.assertOnUiThread();
        return withNativeOrThrow(AwHeaderInterceptorStoreJni.get()::getInterceptorCountForTesting);
    }

    private <R extends @Nullable Object> R withNativeOrThrow(
            Function<JniRawPtr<Native>, R> function) {
        JniRawPtr<Native> nativePointer = mNativeSupplier.get();
        if (nativePointer == null) {
            throw new IllegalStateException(
                    "The native AwHeaderInterceptorStore is not available.");
        }
        try {
            return function.apply(nativePointer);
        } finally {
            nativePointer.release();
        }
    }

    private void runWithNativeOrThrow(Consumer<JniRawPtr<Native>> consumer) {
        this.<@Nullable Void>withNativeOrThrow(
                nativePointer -> {
                    consumer.accept(nativePointer);
                    return null;
                });
    }

    @NativeMethods
    interface Natives {
        int addInterceptorFromJava(
                JniPtr<Native> self,
                AwHeaderInterceptor interceptor,
                @JniType("std::vector<std::string>") List<String> urlPatterns);

        void removeInterceptor(JniPtr<Native> self, int id);

        void clearInterceptors(JniPtr<Native> self);

        int getInterceptorCountForTesting(JniPtr<Native> self);
    }
}
