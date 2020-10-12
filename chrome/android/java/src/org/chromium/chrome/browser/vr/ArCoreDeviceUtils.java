// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Helper used to ensure that the ArCore device is appropriately "installed" in
 * the browser after any requisite dependencies (e.g. the AR DFM), have been
 * installed.
 */
@JNINamespace("vr")
public class ArCoreDeviceUtils {
    /**
     * Installs the ArCoreDeviceProvider, so that the browser can use an ArCore device.
     * Should be called once any ArCore dependencies (except ArCore itself) have been installed.
     */
    public static void installArCoreDeviceProviderFactory() {
        ArCoreDeviceUtilsJni.get().installArCoreDeviceProviderFactory();
    }

    @NativeMethods
    /* package */ interface ArCodeDeviceUtilsNative {
        void installArCoreDeviceProviderFactory();
    }
}
