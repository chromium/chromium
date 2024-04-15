// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prefetch.settings;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;

/** Reads and writes preferences related to preloading. */
@JNINamespace("prefetch")
public class PreloadPagesSettingsBridge {
    /**
     * @param profile The {@link Profile} associated with the settings.
     * @return The current Preload Pages state (off, standard or extended).
     */
    public static @PreloadPagesState int getState(Profile profile) {
        return PreloadPagesSettingsBridgeJni.get().getState(profile);
    }

    /**
     * Sets the current Preload Pages state.
     *
     * @param profile The {@link Profile} associated with the settings.
     * @param state The desired new Preload Pages state.
     */
    public static void setState(Profile profile, @PreloadPagesState int state) {
        PreloadPagesSettingsBridgeJni.get().setState(profile, state);
    }

    /**
     * Determines whether the Preload Pages state is controlled by an enterprise policy.
     *
     * @param profile The {@link Profile} associated with the settings.
     * @return True if the Preload Pages state is managed.
     */
    public static boolean isNetworkPredictionManaged(Profile profile) {
        return PreloadPagesSettingsBridgeJni.get().isNetworkPredictionManaged(profile);
    }

    @NativeMethods
    public interface Natives {
        @PreloadPagesState
        int getState(@JniType("Profile*") Profile profile);

        void setState(@JniType("Profile*") Profile profile, @PreloadPagesState int mode);

        boolean isNetworkPredictionManaged(@JniType("Profile*") Profile profile);
    }
}
