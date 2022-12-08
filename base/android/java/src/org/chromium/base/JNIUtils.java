// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.pm.ApplicationInfo;
import android.os.Build;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.build.annotations.MainDex;

import java.util.Map;

/**
 * This class provides JNI-related methods to the native library.
 */
@MainDex
public class JNIUtils {
    private static final String TAG = "JNIUtils";
    private static Boolean sSelectiveJniRegistrationEnabled;
    private static ClassLoader sJniClassLoader;

    /**
     * Returns a ClassLoader which can load Java classes from the specified split.
     *
     * @param splitName Name of the split, or empty string for the base split.
     */
    @CalledByNative
    private static ClassLoader getSplitClassLoader(String splitName) {
        if (!splitName.isEmpty()) {
            boolean isInstalled = BundleUtils.isIsolatedSplitInstalled(splitName);
            Log.i(TAG, "Init JNI Classloader for %s. isInstalled=%b", splitName, isInstalled);

            if (!isInstalled && BundleUtils.isBundle()
                    && Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                // Address race condition on global ApplicationInfo being updated.
                // https://crbug.com/1395191
                ApplicationInfo globalAppInfo =
                        ContextUtils.getApplicationContext().getApplicationInfo();
                ApplicationInfo freshAppInfo =
                        PackageUtils.getApplicationPackageInfo(0).applicationInfo;
                globalAppInfo.splitNames = freshAppInfo.splitNames;
                globalAppInfo.splitSourceDirs = freshAppInfo.splitSourceDirs;
                globalAppInfo.splitPublicSourceDirs = freshAppInfo.splitPublicSourceDirs;

                isInstalled = BundleUtils.isIsolatedSplitInstalled(splitName);
                Log.i(TAG, "Init JNI Classloader for %s. isInstalled=%b", splitName, isInstalled);
                assert isInstalled
                    : "Should not hit splitcompat mode for Android T+. You might have a "
                      + "generate_jni() target that declares split_name, but includes "
                      + "a .java file from the base split (https://crbug.com/1394148).";
            }

            if (isInstalled) {
                return BundleUtils.getOrCreateSplitClassLoader(splitName);
            } else {
                // Split was installed by PlayCore in "compat" mode, meaning that our base module's
                // ClassLoader was patched to add the splits' dex file to it.
            }
        }
        return sJniClassLoader != null ? sJniClassLoader : JNIUtils.class.getClassLoader();
    }

    /**
     * Sets the ClassLoader to be used for loading Java classes from native.
     *
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

    /**
     * Helper to convert from java maps to two arrays for JNI.
     */
    public static <K, V> void splitMap(Map<K, V> map, K[] outKeys, V[] outValues) {
        assert map.size() == outKeys.length;
        assert outValues.length == outKeys.length;

        int i = 0;
        for (Map.Entry<K, V> entry : map.entrySet()) {
            outKeys[i] = entry.getKey();
            outValues[i] = entry.getValue();
            i++;
        }
    }
}
