// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.browser_context.PartitionResolver;
import org.chromium.components.embedder_support.simple_factory_key.SimpleFactoryKeyHandle;
import org.chromium.content_public.browser.BrowserContextHandle;

/**
 * Profile specific implementation for resolving various partition objects, such as {@link
 * BrowserContextHandle}. This class requires native to be initialized in order to work properly.
 */
@JNINamespace("profile_resolver")
@NullMarked
public class ProfileResolver implements PartitionResolver {
    @Override
    public String tokenize(@Nullable BrowserContextHandle handle) {
        return ProfileResolverJni.get().tokenizeProfile((Profile) handle);
    }

    @Override
    public String tokenize(@Nullable SimpleFactoryKeyHandle handle) {
        return ProfileResolverJni.get().tokenizeProfileKey((ProfileKey) handle);
    }

    @Override
    public void resolveBrowserContext(
            @Nullable String token, Callback<@Nullable BrowserContextHandle> callback) {
        ProfileResolverJni.get()
                .resolveProfile(token, (@Nullable Profile profile) -> callback.onResult(profile));
    }

    @Override
    public void resolveSimpleFactoryKey(
            @Nullable String token, Callback<@Nullable SimpleFactoryKeyHandle> callback) {
        ProfileResolverJni.get()
                .resolveProfileKey(token, (@Nullable ProfileKey key) -> callback.onResult(key));
    }

    /**
     * Profile specific resolve method. Often more convenient to call from //chrome/ code. Callers
     * should be careful to be able to handle both inline/renterant and asynchronous invocations of
     * the passed callback.
     *
     * @param token A value previously provided by a tokenize call.
     * @param callback A callback to pass the resolved profile on success, otherwise null.
     */
    public void resolveProfile(@Nullable String token, Callback<@Nullable Profile> callback) {
        ProfileResolverJni.get().resolveProfile(token, callback);
    }

    /**
     * Profile specific resolve method. Often more convenient to call from //chrome/ code. Callers
     * should be careful to be able to handle both inline/renterant and asynchronous invocations of
     * the passed callback.
     *
     * @param token A value previously provided by a tokenize call.
     * @param callback A callback to pass the resolved profile key on success, otherwise null.
     */
    public void resolveProfileKey(@Nullable String token, Callback<@Nullable ProfileKey> callback) {
        ProfileResolverJni.get().resolveProfileKey(token, callback);
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        String tokenizeProfile(@Nullable @JniType("Profile*") Profile profile);

        String tokenizeProfileKey(@Nullable ProfileKey profileKey);

        void resolveProfile(
                @Nullable String token, Callback<@JniType("Profile*") @Nullable Profile> callback);

        void resolveProfileKey(@Nullable String token, Callback<@Nullable ProfileKey> callback);
    }
}
