// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeLibraryLoadedStatus;
import org.jni_zero.NativeLibraryLoadedStatus.NativeLibraryLoadedStatusProvider;
import org.jni_zero.NativeMethods;

import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** This class provides JNI-related methods to the native library. */
@NullMarked
@JNINamespace("base::android")
public class JNIUtils {
    private static final String TAG = "JNIUtils";
    private static @Nullable ClassLoader sJniClassLoader;
    private static boolean sBadClassLoaderUsed;
    private static boolean sHasBeenCalled;

    /**
     * Returns a ClassLoader which can load Java classes from the specified split.
     *
     * @param splitName Name of the split, or empty string for the base split.
     */
    @CalledByNative
    private static ClassLoader getSplitClassLoader(@JniType("std::string") String splitName) {
        if (!sHasBeenCalled) {
            overwriteNativeLibraryLoadedStatus();
            sHasBeenCalled = true;
        }
        if (!splitName.isEmpty()) {
            boolean isInstalled = BundleUtils.isIsolatedSplitInstalled(splitName);
            Log.i(TAG, "Init JNI Classloader for %s. isInstalled=%b", splitName, isInstalled);

            if (isInstalled) {
                return BundleUtils.getOrCreateSplitClassLoader(splitName);
            } else {
                // Split was installed by PlayCore in "compat" mode, meaning that our base module's
                // ClassLoader was patched to add the splits' dex file to it.
                // This should never happen on Android T+, where PlayCore is configured to fully
                // install splits from the get-go, but can still sometimes happen if play store
                // is very out of date.
            }
        }
        if (sJniClassLoader == null) {
            sBadClassLoaderUsed = true;
            // This will be replaced by the Chrome split's ClassLoader as soon as we call
            // setClassLoader.
            return JNIUtils.class.getClassLoader();
        }
        return sJniClassLoader;
    }

    private static void overwriteNativeLibraryLoadedStatus() {
        // There exist some race conditions where native can call into Java before LibraryLoader is
        // aware that it's allowed. This is a convenient location, as the first "real" native to
        // Java call, where we can know for sure that the native library is operating and that, even
        // though LibraryLoader doesn't know it yet, it's safe to call into the native library.
        // crbug.com/398057788 is the reason for this.
        if (BuildConfig.ENABLE_ASSERTS) {
            NativeLibraryLoadedStatus.setProvider(
                    new NativeLibraryLoadedStatusProvider() {
                        @Override
                        public boolean areNativeMethodsReady() {
                            return true;
                        }
                    });
        }
    }

    /**
     * Sets the ClassLoader to be used for loading Java classes from native.
     *
     * @param classLoader the ClassLoader to use.
     */
    public static void setClassLoader(ClassLoader classLoader) {
        assert sJniClassLoader == null : "setClassLoader should be called only once.";
        sJniClassLoader = classLoader;
        if (sBadClassLoaderUsed) {
            // In the case that we attempt a JNI call before the Chrome split is loaded, we want to
            // make sure that we invalidate the cached ClassLoader, since the cached ClassLoader
            // only includes the base module. We cannot do this unconditionally, however, since
            // sBadClassLoaderUsed also indicates that native is loaded, and this function will
            // often execute before native is loaded.
            JNIUtilsJni.get().overwriteMainClassLoader(classLoader);
        }
    }

    @NativeMethods
    interface Natives {
        void overwriteMainClassLoader(ClassLoader classLoader);
    }
}
