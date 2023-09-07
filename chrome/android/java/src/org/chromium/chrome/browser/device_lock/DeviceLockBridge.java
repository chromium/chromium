// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_lock;

import android.app.Activity;
import android.app.KeyguardManager;
import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.base.WindowAndroid;

/**
 * This bridge allows native web C++ code to launch DeviceLockActivity.
 */
public class DeviceLockBridge {
    private long mNativeDeviceLockBridge;

    private DeviceLockBridge(long nativeDeviceLockBridge) {
        mNativeDeviceLockBridge = nativeDeviceLockBridge;
    }

    @CalledByNative
    static DeviceLockBridge create(long nativeDeviceLockBridge) {
        return new DeviceLockBridge(nativeDeviceLockBridge);
    }

    /**
     * Launches {@link DeviceLockActivity} (explainer dialog and PIN/password setup flow) before
     * allowing users to continue with the saving passwords flow if the user's device is not secure
     * (ex: no PIN or password set). Currently, these additional steps are only added for Android
     * automotive devices. Note that the explainer dialog will not be shown if the user has already
     * seen it.
     *
     * TODO(crbug/1474036): Handle edge case where Chrome is killed when switching to OS PIN flow.
     */
    @CalledByNative
    private void launchDeviceLockUiBeforeSavingPassword(WindowAndroid windowAndroid) {
        if (mNativeDeviceLockBridge == 0) {
            return;
        }
        final Context context = windowAndroid.getContext().get();
        if (context != null) {
            DeviceLockActivityLauncherImpl.get().launchDeviceLockActivity(context, null,
                    windowAndroid,
                    (resultCode, unused)
                            -> DeviceLockBridgeJni.get().onDeviceLockUiFinished(
                                    mNativeDeviceLockBridge, resultCode == Activity.RESULT_OK));
        } else {
            DeviceLockBridgeJni.get().onDeviceLockUiFinished(mNativeDeviceLockBridge, false);
        }
    }

    @CalledByNative
    private void clearNativePointer() {
        mNativeDeviceLockBridge = 0;
    }

    @CalledByNative
    private boolean isDeviceSecure() {
        return ((KeyguardManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.KEYGUARD_SERVICE))
                .isDeviceSecure();
    }

    /**
     * C++ method signatures.
     */
    @NativeMethods
    interface Natives {
        void onDeviceLockUiFinished(long nativeDeviceLockBridge, boolean isDeviceLockSet);
    }
}
