// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens;

import org.jni_zero.NativeMethods;

import org.chromium.components.policy.PolicyCache;

/** Provides Lens policy utility functions. */
public class LensPolicyUtils {
    private static final String LENS_CAMERA_ASSISTED_SEARCH_ENABLED_POLICY_NAME =
            "policy.lens_camera_assisted_search_enabled";

    /**
     * @return Whether the Lens camera assisted search should be enabled for the enterprise user.
     */
    public static boolean getLensCameraAssistedSearchEnabledForEnterprise() {
        // Read from policy cache before the native library is ready.
        if (PolicyCache.get().isReadable()) {
            return Boolean.TRUE.equals(
                    PolicyCache.get()
                            .getBooleanValue(LENS_CAMERA_ASSISTED_SEARCH_ENABLED_POLICY_NAME));
        }

        return LensPolicyUtilsJni.get().getLensCameraAssistedSearchEnabled();
    }

    @NativeMethods
    public interface Natives {
        boolean getLensCameraAssistedSearchEnabled();
    }
}
