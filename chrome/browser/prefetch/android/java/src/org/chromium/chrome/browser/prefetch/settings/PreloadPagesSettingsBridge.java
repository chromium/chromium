// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prefetch.settings;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * Reads and writes preferences related to preloading.
 */
// TODO(crbug.com/1410601): Pass in the profile and remove GetActiveUserProfile in C++.
@JNINamespace("prefetch")
public class PreloadPagesSettingsBridge {
    /**
     * @return The current Preload Pages state (off, standard or extended).
     */
    public static @PreloadPagesState int getState() {
        return PreloadPagesSettingsBridgeJni.get().getState();
    }

    /**
     * Sets the current Preload Pages state.
     *
     * @param mode The desired new Preload Pages state.
     */
    public static void setState(@PreloadPagesState int state) {
        PreloadPagesSettingsBridgeJni.get().setState(state);
    }

    /**
     * Determines whether the Preload Pages state is controlled by an enterprise
     * policy.
     *
     * @return True if the Preload Pages state is managed.
     */
    public static boolean isNetworkPredictionManaged() {
        return PreloadPagesSettingsBridgeJni.get().isNetworkPredictionManaged();
    }

    @NativeMethods
    public interface Natives {
        @PreloadPagesState
        int getState();
        void setState(@PreloadPagesState int mode);
        boolean isNetworkPredictionManaged();
    }
}
