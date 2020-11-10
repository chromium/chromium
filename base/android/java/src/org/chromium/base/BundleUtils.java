// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;

import androidx.collection.SimpleArrayMap;

import dalvik.system.BaseDexClassLoader;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.compat.ApiHelperForO;

import java.util.Arrays;

/**
 * Utils for working with android app bundles.
 *
 * Important notes about bundle status as interpreted by this class:
 *
 * <ul>
 *   <li>If {@link BuildConfig#BUNDLES_SUPPORTED} is false, then we are definitely not in a bundle,
 *   and ProGuard is able to strip out the bundle support library.</li>
 *   <li>If {@link BuildConfig#BUNDLES_SUPPORTED} is true, then we MIGHT be in a bundle.
 *   {@link BundleUtils#sIsBundle} is the source of truth.</li>
 * </ul>
 *
 * We need two fields to store one bit of information here to ensure that ProGuard can optimize out
 * the bundle support library (since {@link BuildConfig#BUNDLES_SUPPORTED} is final) and so that
 * we can dynamically set whether or not we're in a bundle for targets that use static shared
 * library APKs.
 */
public final class BundleUtils {
    // These Contexts will be needed throughout the lifetime of the application, so it is fine to
    // have strong references in the map.
    private static final SimpleArrayMap<String, Context> sIsolatedSplitContextCache =
            new SimpleArrayMap<>();
    private static Boolean sIsBundle;

    /**
     * {@link BundleUtils#isBundle()}  is not called directly by native because
     * {@link CalledByNative} prevents inlining, causing the bundle support lib to not be
     * removed non-bundle builds.
     *
     * @return true if the current build is a bundle.
     */
    @CalledByNative
    public static boolean isBundleForNative() {
        return isBundle();
    }

    /**
     * @return true if the current build is a bundle.
     */
    public static boolean isBundle() {
        if (!BuildConfig.BUNDLES_SUPPORTED) {
            return false;
        }
        assert sIsBundle != null;
        return sIsBundle;
    }

    public static void setIsBundle(boolean isBundle) {
        sIsBundle = isBundle;
    }

    /**
     * Returns whether splitName is installed. Note, this will return false on Android versions
     * below O, where isolated splits are not supported.
     */
    public static boolean isIsolatedSplitInstalled(Context context, String splitName) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return false;
        }

        String[] splitNames = ApiHelperForO.getSplitNames(context.getApplicationInfo());
        return splitNames != null && Arrays.asList(splitNames).contains(splitName);
    }

    /**
     * Returns a context for the isolated split with the name splitName. This will throw a
     * RuntimeException if isolated splits are enabled and the split is not installed. If the
     * current Android version does not support isolated splits, the original context will be
     * returned. If isolated splits are not enabled for this APK/bundle, the underlying ContextImpl
     * from the base context will be returned.
     */
    public static Context createIsolatedSplitContext(Context base, String splitName) {
        // Isolated splits are only supported in O+, so just return the base context on other
        // versions, since this will have access to all splits.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return base;
        }

        // Context.createContextForSplit() creates a new ContextImpl for each call, so we cache the
        // returned context for future calls.
        Context splitContext = sIsolatedSplitContextCache.get(splitName);
        if (splitContext == null) {
            try {
                splitContext = ApiHelperForO.createContextForSplit(base, splitName);
            } catch (PackageManager.NameNotFoundException e) {
                throw new RuntimeException(e);
            }
            sIsolatedSplitContextCache.put(splitName, splitContext);
        }
        return splitContext;
    }

    /* Returns absolute path to a native library in a feature module. */
    @CalledByNative
    public static String getNativeLibraryPath(String libraryName) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            // Due to b/171269960 isolated split class loaders have an empty library path, so check
            // the base module class loader which loaded BundleUtils.
            return ((BaseDexClassLoader) BundleUtils.class.getClassLoader())
                    .findLibrary(libraryName);
        }
    }
}
