// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import org.jni_zero.CalledByNative;

import org.chromium.base.UnguessableToken;
import org.chromium.build.annotations.NullMarked;

/** Bridges native queries into the dynamic XR module installer state. */
@NullMarked
public class XrModuleBridge {
    /** Returns true if the XR module is installed. */
    @CalledByNative
    public static boolean isModuleInstalled() {
        return XrModule.isInstalled();
    }

    @CalledByNative
    public static void createImmersiveVideoPlaybackActivity(
            UnguessableToken nativeToken, Object initiatorTab) {
        XrModule.getImpl().createImmersiveVideoPlaybackActivity(nativeToken, initiatorTab);
    }
}
