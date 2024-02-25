// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.dev_ui;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.components.module_installer.engine.InstallListener;

/** {@link InstallListener} implementation that uses JNI to propagate install signal to native. */
@JNINamespace("dev_ui")
public class DevUiInstallListener implements InstallListener {
    private long mNativeListener;

    @Override
    public void onComplete(boolean success) {
        if (mNativeListener == 0) return;
        DevUiInstallListenerJni.get().onComplete(mNativeListener, success);
    }

    @CalledByNative
    private DevUiInstallListener(long nativeListener) {
        mNativeListener = nativeListener;
    }

    @CalledByNative
    private void onNativeDestroy() {
        mNativeListener = 0;
    }

    @NativeMethods
    interface Natives {
        void onComplete(long nativeDevUiInstallListener, boolean success);
    }
}
