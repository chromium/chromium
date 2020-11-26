// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.MainDex;

/**
 * This class provides JNI-related methods to the native library.
 */
@MainDex
public class JNIUtils {
    private static Boolean sSelectiveJniRegistrationEnabled;
    private static ClassLoader sJniClassLoader;

    /**
     * This returns a ClassLoader that is capable of loading Chromium Java code. Such a ClassLoader
     * is needed for the few cases where the JNI mechanism is unable to automatically determine the
     * appropriate ClassLoader instance.
     */
    private static ClassLoader getClassLoader() {
        if (sJniClassLoader == null) {
            return JNIUtils.class.getClassLoader();
        }
        return sJniClassLoader;
    }

    /** Returns a ClassLoader which can load Java classes from the specified split. */
    @CalledByNative
    public static ClassLoader getSplitClassLoader(String splitName) {
        Context context = ContextUtils.getApplicationContext();
        if (!TextUtils.isEmpty(splitName)
                && BundleUtils.isIsolatedSplitInstalled(context, splitName)) {
            return BundleUtils.createIsolatedSplitContext(context, splitName).getClassLoader();
        }
        return getClassLoader();
    }

    /**
     * Sets the ClassLoader to be used for loading Java classes from native.
     * @param classLoader the ClassLoader to use.
     */
    public static void setClassLoader(ClassLoader classLoader) {
        sJniClassLoader = classLoader;
    }

    /**
     * @return whether or not the current process supports selective JNI registration.
     */
    @CalledByNative
    public static boolean isSelectiveJniRegistrationEnabled() {
        if (sSelectiveJniRegistrationEnabled == null) {
            sSelectiveJniRegistrationEnabled = false;
        }
        return sSelectiveJniRegistrationEnabled;
    }

    /**
     * Allow this process to selectively perform JNI registration. This must be called before
     * loading native libraries or it will have no effect.
     */
    public static void enableSelectiveJniRegistration() {
        assert sSelectiveJniRegistrationEnabled == null;
        sSelectiveJniRegistrationEnabled = true;
    }
}
