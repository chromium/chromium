// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.components.embedder_support.browser_context.PartitionResolver;
import org.chromium.components.embedder_support.simple_factory_key.SimpleFactoryKeyHandle;
import org.chromium.content_public.browser.BrowserContextHandle;

/**
 * Profile specific implementation for resolving various partition objects, such as {@link
 * BrowserContextHandle}. This class requires native to be initialized in order to work properly.
 */
@JNINamespace("profile_resolver")
public class ProfileResolver implements PartitionResolver {
    @Override
    public String tokenize(BrowserContextHandle handle) {
        return ProfileResolverJni.get().tokenizeProfile((Profile) handle);
    }

    @Override
    public String tokenize(SimpleFactoryKeyHandle handle) {
        return ProfileResolverJni.get().tokenizeProfileKey((ProfileKey) handle);
    }

    @Override
    public void resolveBrowserContext(String token, Callback<BrowserContextHandle> callback) {
        ProfileResolverJni.get()
                .resolveProfile(token, (Profile profile) -> callback.onResult(profile));
    }

    @Override
    public void resolveSimpleFactoryKey(String token, Callback<SimpleFactoryKeyHandle> callback) {
        ProfileResolverJni.get()
                .resolveProfileKey(token, (ProfileKey key) -> callback.onResult(key));
    }

    /**
     * Profile specific resolve method. Often more convenient to call from //chrome/ code. Callers
     * should be careful to be able to handle both inline/renterant and asynchronous invocations of
     * the passed callback.
     * @param token A value previously provided by a tokenize call.
     * @param callback A callback to pass the resolved profile on success, otherwise null.
     */
    public void resolveProfile(String token, Callback<Profile> callback) {
        ProfileResolverJni.get().resolveProfile(token, callback);
    }

    /**
     * Profile specific resolve method. Often more convenient to call from //chrome/ code. Callers
     * should be careful to be able to handle both inline/renterant and asynchronous invocations of
     * the passed callback.
     * @param token A value previously provided by a tokenize call.
     * @param callback A callback to pass the resolved profile key on success, otherwise null.
     */
    public void resolveProfileKey(String token, Callback<ProfileKey> callback) {
        ProfileResolverJni.get().resolveProfileKey(token, callback);
    }

    @NativeMethods
    interface Natives {
        String tokenizeProfile(@JniType("Profile*") Profile profile);

        String tokenizeProfileKey(ProfileKey profileKey);

        void resolveProfile(String token, Callback<@JniType("Profile*") Profile> callback);

        void resolveProfileKey(String token, Callback<ProfileKey> callback);
    }
}
