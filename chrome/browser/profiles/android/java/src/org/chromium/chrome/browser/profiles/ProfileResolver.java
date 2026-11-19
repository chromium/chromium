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

/**
 * Serializes and resolves {@link Profile} references across native and Java. This class requires
 * native to be initialized in order to work properly.
 */
@JNINamespace("profile_resolver")
@NullMarked
public class ProfileResolver {
    // TODO(crbug.com/40254448): Remove once BackgroundTabPoolManager uses the encoded token from
    // ProfileTokenUtils.
    public String tokenize(@Nullable Profile profile) {
        return ProfileResolverJni.get().tokenizeProfile(profile);
    }

    /**
     * Resolves a {@link Profile} from a token previously provided by {@link #tokenize(Profile)}.
     * Callers should be careful to be able to handle both inline/reentrant and asynchronous
     * invocations of the passed callback.
     *
     * @param token A value previously provided by a tokenize call.
     * @param callback A callback to pass the resolved profile on success, otherwise null.
     */
    // TODO(crbug.com/40254448): Remove once BackgroundTabPoolManager uses the encoded token from
    // ProfileTokenUtils and legacy intents have expired (crbug.com/585922618).
    public void resolveProfile(@Nullable String token, Callback<@Nullable Profile> callback) {
        ProfileResolverJni.get().resolveProfile(token, callback);
    }

    /**
     * Like {@link #tokenize(Profile)}, but returns the token's bytes unchanged rather than
     * converting them to a String.
     */
    /* package */ byte[] tokenizeToBytes(Profile profile) {
        return ProfileResolverJni.get().tokenizeProfileToBytes(profile);
    }

    /** Like {@link #resolveProfile}, for a token returned by {@link #tokenizeToBytes}. */
    /* package */ void resolveProfileFromBytes(byte[] token, Callback<@Nullable Profile> callback) {
        ProfileResolverJni.get().resolveProfileFromBytes(token, callback);
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        // TODO(crbug.com/40254448): Remove once BackgroundTabPoolManager uses the encoded token
        // from ProfileTokenUtils and legacy intents have expired (crbug.com/585922618).
        @JniType("std::string")
        String tokenizeProfile(@JniType("Profile*") @Nullable Profile profile);

        void resolveProfile(
                @JniType("std::optional<std::string>") @Nullable String token,
                @JniType("base::OnceCallback<void(Profile*)>")
                        Callback<@Nullable Profile> callback);

        @JniType("std::vector<uint8_t>")
        byte[] tokenizeProfileToBytes(@JniType("Profile*") Profile profile);

        void resolveProfileFromBytes(
                @JniType("std::vector<uint8_t>") byte[] token,
                @JniType("base::OnceCallback<void(Profile*)>")
                        Callback<@Nullable Profile> callback);
    }
}
