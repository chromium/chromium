// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Manages interactions with the VR Shell.
 */
@JNINamespace("vr")
public class VrShellDelegate {


    @CalledByNative
    private static VrShellDelegate getInstance() {
        return null;
    }

    @CalledByNative
    private void presentRequested() {
    }

    @CalledByNative
    /* package */ void exitWebVRPresent() {
    }

    /**
     * @return Pointer to the native VrShellDelegate object.
     */
    @CalledByNative
    private long getNativePointer() {
        return 0;
    }

    @NativeMethods
    interface Natives {
        long init(VrShellDelegate caller);
        void onLibraryAvailable();
        void setPresentResult(long nativeVrShellDelegate, VrShellDelegate caller, boolean result);
        void onPause(long nativeVrShellDelegate, VrShellDelegate caller);
        void onResume(long nativeVrShellDelegate, VrShellDelegate caller);
        void destroy(long nativeVrShellDelegate, VrShellDelegate caller);
        void registerVrAssetsComponent();
    }
}
