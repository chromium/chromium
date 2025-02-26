// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** This class provides JNI-related methods to the native library. */
@NullMarked
@JNINamespace("base::android")
public class JNIUtils {
    private static final String TAG = "JNIUtils";
    private static final JniClassLoader sJniClassLoader = new JniClassLoader();

    /**
     * Returns a ClassLoader which can load Java classes from the specified split.
     *
     * @param splitName Name of the split, or empty string for the base split.
     */
    @CalledByNative
    private static ClassLoader getSplitClassLoader(@JniType("std::string") String splitName) {
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
        return sJniClassLoader;
    }

    /**
     * Sets the ClassLoader to be used for loading Java classes from native.
     *
     * @param classLoader the ClassLoader to use.
     */
    public static void setDefaultClassLoader(ClassLoader classLoader) {
        sJniClassLoader.mDelegate = classLoader;
    }

    /**
     * Allows swapping out the underlying class loader to a an apk split's class loader without
     * having to invalidate the native code's caching of the class loader (which may or may not
     * have happened yet).
     */
    private static class JniClassLoader extends ClassLoader {
        @Nullable ClassLoader mDelegate;

        JniClassLoader() {
            super(JNIUtils.class.getClassLoader());
        }

        // ClassLoader.loadClass() delegates to this method.
        @Override
        public Class<?> findClass(String cn) throws ClassNotFoundException {
            ClassLoader delegate = mDelegate;
            if (delegate != null) {
                return delegate.loadClass(cn);
            }
            return super.findClass(cn);
        }
    }
}
