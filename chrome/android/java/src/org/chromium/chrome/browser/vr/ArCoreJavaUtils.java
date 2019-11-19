// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.content.Context;
import android.view.Surface;

import dalvik.system.BaseDexClassLoader;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Provides ARCore classes access to java-related app functionality.
 *
 * <p>This class provides static methods called by ArDelegateImpl via ArDelegateProvider,
 * and provides JNI interfaces to/from the C++ AR code.</p>
 */
@JNINamespace("vr")
public class ArCoreJavaUtils {
    private static final String TAG = "ArCoreJavaUtils";
    private static final boolean DEBUG_LOGS = false;

    private long mNativeArCoreJavaUtils;

    // The native ArCoreDevice runtime creates a ArCoreJavaUtils instance in its constructor,
    // and keeps a strong reference to it for the lifetime of the device. It creates and
    // owns an ArImmersiveOverlay for the duration of an immersive-ar session, which in
    // turn contains a reference to ArCoreJavaUtils for making JNI calls back to the device.
    private ArImmersiveOverlay mArImmersiveOverlay;

    // ArDelegateImpl needs to know if there's an active immersive session so that it can handle
    // back button presses from ChromeActivity's onBackPressed(). It's only set while a session is
    // in progress, and reset to null on session end. The ArImmersiveOverlay member has a strong
    // reference to the ChromeActivity, and that shouldn't be retained beyond the duration of a
    // session.
    private static ArCoreJavaUtils sActiveSessionInstance;

    @CalledByNative
    private static ArCoreJavaUtils create(long nativeArCoreJavaUtils) {
        ThreadUtils.assertOnUiThread();
        return new ArCoreJavaUtils(nativeArCoreJavaUtils);
    }

    @CalledByNative
    private static String getArCoreShimLibraryPath() {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return ((BaseDexClassLoader) ContextUtils.getApplicationContext().getClassLoader())
                    .findLibrary("arcore_sdk_c");
        }
    }

    /**
     * Gets the current application context.
     *
     * @return Context The application context.
     */
    @CalledByNative
    private static Context getApplicationContext() {
        return ContextUtils.getApplicationContext();
    }

    private ArCoreJavaUtils(long nativeArCoreJavaUtils) {
        if (DEBUG_LOGS) Log.i(TAG, "constructor, nativeArCoreJavaUtils=" + nativeArCoreJavaUtils);
        mNativeArCoreJavaUtils = nativeArCoreJavaUtils;
    }

    @CalledByNative
    private void startSession(final Tab tab, boolean useOverlay) {
        if (DEBUG_LOGS) Log.i(TAG, "startSession");
        mArImmersiveOverlay = new ArImmersiveOverlay();
        sActiveSessionInstance = this;
        mArImmersiveOverlay.show(tab.getActivity(), this, useOverlay);
    }

    @CalledByNative
    private void endSession() {
        if (DEBUG_LOGS) Log.i(TAG, "endSession");
        if (mArImmersiveOverlay == null) return;

        mArImmersiveOverlay.cleanupAndExit();
        mArImmersiveOverlay = null;
        sActiveSessionInstance = null;
    }

    // Called from ArDelegateImpl
    public static boolean onBackPressed() {
        if (DEBUG_LOGS) Log.i(TAG, "onBackPressed");
        // If there's an active immersive session, consume the "back" press and shut down the
        // session.
        if (sActiveSessionInstance != null) {
            sActiveSessionInstance.endSession();
            return true;
        }
        return false;
    }

    public void onDrawingSurfaceReady(Surface surface, int rotation, int width, int height) {
        if (DEBUG_LOGS) Log.i(TAG, "onDrawingSurfaceReady");
        if (mNativeArCoreJavaUtils == 0) return;
        ArCoreJavaUtilsJni.get().onDrawingSurfaceReady(
                mNativeArCoreJavaUtils, ArCoreJavaUtils.this, surface, rotation, width, height);
    }

    public void onDrawingSurfaceTouch(boolean isTouching, float x, float y) {
        if (DEBUG_LOGS) Log.i(TAG, "onDrawingSurfaceTouch");
        if (mNativeArCoreJavaUtils == 0) return;
        ArCoreJavaUtilsJni.get().onDrawingSurfaceTouch(
                mNativeArCoreJavaUtils, ArCoreJavaUtils.this, isTouching, x, y);
    }

    public void onDrawingSurfaceDestroyed() {
        if (DEBUG_LOGS) Log.i(TAG, "onDrawingSurfaceDestroyed");
        if (mNativeArCoreJavaUtils == 0) return;
        ArCoreJavaUtilsJni.get().onDrawingSurfaceDestroyed(
                mNativeArCoreJavaUtils, ArCoreJavaUtils.this);
    }

    @CalledByNative
    private void onNativeDestroy() {
        // ArCoreDevice's destructor ends sessions before destroying its native ArCoreSessionUtils
        // object.
        assert sActiveSessionInstance == null : "unexpected active session in onNativeDestroy";

        mNativeArCoreJavaUtils = 0;
    }

    @NativeMethods
    interface Natives {
        void onDrawingSurfaceReady(long nativeArCoreJavaUtils, ArCoreJavaUtils caller,
                Surface surface, int rotation, int width, int height);
        void onDrawingSurfaceTouch(long nativeArCoreJavaUtils, ArCoreJavaUtils caller,
                boolean touching, float x, float y);
        void onDrawingSurfaceDestroyed(long nativeArCoreJavaUtils, ArCoreJavaUtils caller);
    }
}
