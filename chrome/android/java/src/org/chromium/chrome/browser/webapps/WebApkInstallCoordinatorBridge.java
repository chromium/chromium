// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.graphics.Bitmap;
import android.os.RemoteException;

import androidx.annotation.BinderThread;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.webapk_install.IOnFinishInstallCallback;

/**
 * This class owns the native WebApkInstallCoordinatorBridge, so {@link destroy} must be
 * called from Java side to avoid memory leaks.
 * The C++ counterpart of this class handles the WebAPK installation and calls
 * {@link onFinishedInstall} which is invoking the {@code callback} with the outcome
 * of the installation.
 */
@JNINamespace("webapps")
public class WebApkInstallCoordinatorBridge {
    private long mNativeWebApkInstallCoordinatorBridge;
    private IOnFinishInstallCallback mCallback;

    public WebApkInstallCoordinatorBridge() {
        mNativeWebApkInstallCoordinatorBridge =
                WebApkInstallCoordinatorBridgeJni.get().init(WebApkInstallCoordinatorBridge.this);
    }

    @BinderThread
    void install(byte[] apkProto, Bitmap primaryIcon, boolean isPrimaryIconMaskable,
            IOnFinishInstallCallback callback) {
        mCallback = callback;

        WebApkInstallCoordinatorBridgeJni.get().install(mNativeWebApkInstallCoordinatorBridge,
                WebApkInstallCoordinatorBridge.this, apkProto, primaryIcon, isPrimaryIconMaskable);
    }

    @CalledByNative
    @BinderThread
    void onFinishedInstall(int result) {
        try {
            mCallback.handleOnFinishInstall(result);
        } catch (RemoteException e) {
            // Client shut down already, nothing to do.
        }
        destroy();
    }

    // Must be called to destroy this and the native counterpart. Called by the
    // {@link onFinishedInstall} callback that is invoked by the {@link WebApkInstaller}
    // when the installation finished or failed.
    @BinderThread
    void destroy() {
        if (mNativeWebApkInstallCoordinatorBridge != 0) {
            WebApkInstallCoordinatorBridgeJni.get().destroy(mNativeWebApkInstallCoordinatorBridge);
            mNativeWebApkInstallCoordinatorBridge = 0;
        }
    }

    @NativeMethods
    interface Natives {
        long init(WebApkInstallCoordinatorBridge caller);
        void install(long nativeWebApkInstallCoordinatorBridge,
                WebApkInstallCoordinatorBridge caller, byte[] apkProto, Bitmap primaryIcon,
                boolean isPrimaryIconMaskable);
        void destroy(long nativeWebApkInstallCoordinatorBridge);
    }
}
