// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.modules.ModuleInstallUi;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Instantiates the VR delegates. If the VR module is not available this provider will
 * instantiate a fallback implementation.
 */
@JNINamespace("vr")
public class VrModuleProvider implements ModuleInstallUi.FailureUiListener {

    private long mNativeVrModuleProvider;

    @CalledByNative
    private static VrModuleProvider create(long nativeVrModuleProvider) {
        return new VrModuleProvider(nativeVrModuleProvider);
    }

    @CalledByNative
    private static boolean isModuleInstalled() {
        return false;
    }

    @Override
    public void onFailureUiResponse(boolean retry) {
    }

    private VrModuleProvider(long nativeVrModuleProvider) {
        mNativeVrModuleProvider = nativeVrModuleProvider;
    }

    @CalledByNative
    private void onNativeDestroy() {
        mNativeVrModuleProvider = 0;
    }

    @CalledByNative
    private void installModule(Tab tab) {
    }

    @NativeMethods
    interface Natives {
        void registerJni();
        void onInstalledModule(
                long nativeVrModuleProvider, VrModuleProvider caller, boolean success);
    }
}
