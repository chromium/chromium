// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.ArrayMap;
import android.view.LayoutInflater;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import dalvik.system.BaseDexClassLoader;
import dalvik.system.PathClassLoader;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.compat.ApiHelperForO;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.BuildConfig;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Map;

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
public class BundleUtils {
    private static final String TAG = "BundleUtils";
    private static final String LOADED_SPLITS_KEY = "split_compat_loaded_splits";
    private static Boolean sIsBundle;
    private static final Object sSplitLock = new Object();

    // This cache is needed to support the workaround for b/172602571, see
    // createIsolatedSplitContext() for more info.
    private static final ArrayMap<String, ClassLoader> sCachedClassLoaders = new ArrayMap<>();

    private static final Map<String, ClassLoader> sInflationClassLoaders =
            Collections.synchronizedMap(new ArrayMap<>());
    private static SplitCompatClassLoader sSplitCompatClassLoaderInstance;

    // List of splits that were loaded during the last run of chrome when
    // restoring from recents.
    private static ArrayList<String> sSplitsToRestore;

    public static void resetForTesting() {
        sIsBundle = null;
        sCachedClassLoaders.clear();
        sInflationClassLoaders.clear();
        sSplitCompatClassLoaderInstance = null;
        sSplitsToRestore = null;
    }

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

    public static boolean isolatedSplitsEnabled() {
        return BuildConfig.ISOLATED_SPLITS_ENABLED;
    }

    @RequiresApi(api = Build.VERSION_CODES.O)
    private static String getSplitApkPath(String splitName) {
        ApplicationInfo appInfo = ContextUtils.getApplicationContext().getApplicationInfo();
        String[] splitNames = appInfo.splitNames;
        if (splitNames == null) {
            return null;
        }
        int idx = Arrays.binarySearch(splitNames, splitName);
        return idx < 0 ? null : appInfo.splitSourceDirs[idx];
    }

    /**
     * Returns whether splitName is installed. Note, this will return false on Android versions
     * below O, where isolated splits are not supported.
     */
    public static boolean isIsolatedSplitInstalled(String splitName) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return false;
        }
        return getSplitApkPath(splitName) != null;
    }

    /**
     * The lock to hold when calling {@link Context#createContextForSplit(String)}.
     */
    public static Object getSplitContextLock() {
        return sSplitLock;
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

        try {
            Context context;
            // The Application class handles locking itself using the split context lock. This is
            // necessary to prevent a possible deadlock, since the application waits for splits
            // preloading on a background thread.
            // TODO(crbug.com/1172950): Consider moving preloading logic into //base so we can lock
            // here.
            if (isApplicationContext(base)) {
                context = ApiHelperForO.createContextForSplit(base, splitName);
            } else {
                synchronized (getSplitContextLock()) {
                    context = ApiHelperForO.createContextForSplit(base, splitName);
                }
            }
            ClassLoader parent = context.getClassLoader().getParent();
            Context appContext = ContextUtils.getApplicationContext();
            // If the ClassLoader from the newly created context does not equal either the
            // BundleUtils ClassLoader (the base module ClassLoader) or the app context ClassLoader
            // (the chrome module ClassLoader) there must be something messed up in the ClassLoader
            // cache, see b/172602571. This should be solved for the chrome ClassLoader by
            // SplitCompatAppComponentFactory, but modules which depend on the chrome module need
            // special handling here to make sure they have the correct parent.
            boolean shouldReplaceClassLoader = isolatedSplitsEnabled()
                    && !parent.equals(BundleUtils.class.getClassLoader()) && appContext != null
                    && !parent.equals(appContext.getClassLoader());
            synchronized (sCachedClassLoaders) {
                if (shouldReplaceClassLoader && !sCachedClassLoaders.containsKey(splitName)) {
                    String apkPath = getSplitApkPath(splitName);
                    // The librarySearchPath argument to PathClassLoader is not needed here
                    // because the framework doesn't pass it either, see b/171269960.
                    sCachedClassLoaders.put(
                            splitName, new PathClassLoader(apkPath, appContext.getClassLoader()));
                }
                // Always replace the ClassLoader if we have a cached version to make sure all
                // ClassLoaders are consistent.
                ClassLoader cachedClassLoader = sCachedClassLoaders.get(splitName);
                if (cachedClassLoader != null) {
                    if (!cachedClassLoader.equals(context.getClassLoader())) {
                        // Set this for recording the histogram below.
                        shouldReplaceClassLoader = true;
                        replaceClassLoader(context, cachedClassLoader);
                    }
                } else {
                    sCachedClassLoaders.put(splitName, context.getClassLoader());
                }
            }
            RecordHistogram.recordBooleanHistogram(
                    "Android.IsolatedSplits.ClassLoaderReplaced." + splitName,
                    shouldReplaceClassLoader);
            return context;
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException(e);
        }
    }

    /** Replaces the ClassLoader of the passed in Context. */
    public static void replaceClassLoader(Context baseContext, ClassLoader classLoader) {
        while (baseContext instanceof ContextWrapper) {
            baseContext = ((ContextWrapper) baseContext).getBaseContext();
        }

        try {
            // baseContext should now be an instance of ContextImpl.
            Field classLoaderField = baseContext.getClass().getDeclaredField("mClassLoader");
            classLoaderField.setAccessible(true);
            classLoaderField.set(baseContext, classLoader);
        } catch (ReflectiveOperationException e) {
            throw new RuntimeException("Error setting ClassLoader.", e);
        }
    }

    /* Returns absolute path to a native library in a feature module. */
    @CalledByNative
    @Nullable
    public static String getNativeLibraryPath(String libraryName, String splitName) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            // Due to b/171269960 isolated split class loaders have an empty library path, so check
            // the base module class loader first which loaded BundleUtils. If the library is not
            // found there, attempt to construct the correct library path from the split.
            String path = ((BaseDexClassLoader) BundleUtils.class.getClassLoader())
                                  .findLibrary(libraryName);
            if (path != null) {
                return path;
            }

            // SplitCompat is installed on the application context, so check there for library paths
            // which were added to that ClassLoader.
            ClassLoader classLoader = ContextUtils.getApplicationContext().getClassLoader();
            // In WebLayer, the class loader will be a WrappedClassLoader.
            if (classLoader instanceof BaseDexClassLoader) {
                path = ((BaseDexClassLoader) classLoader).findLibrary(libraryName);
            } else if (classLoader instanceof WrappedClassLoader) {
                path = ((WrappedClassLoader) classLoader).findLibrary(libraryName);
            }
            if (path != null) {
                return path;
            }

            return getSplitApkLibraryPath(libraryName, splitName);
        }
    }

    public static void checkContextClassLoader(Context baseContext, Activity activity) {
        ClassLoader activityClassLoader = activity.getClass().getClassLoader();
        ClassLoader contextClassLoader = baseContext.getClassLoader();
        if (activityClassLoader != contextClassLoader) {
            Log.w(TAG, "Mismatched ClassLoaders between Activity and context (fixing): %s",
                    activity.getClass());
            replaceClassLoader(baseContext, activityClassLoader);
        }
    }

    /**
     * Constructs a new instance of the given class name. If the application context class loader
     * can load the class, that class loader will be used, otherwise the class loader from the
     * passed in context will be used.
     */
    public static Object newInstance(Context context, String className) {
        Context appContext = ContextUtils.getApplicationContext();
        if (appContext != null && canLoadClass(appContext.getClassLoader(), className)) {
            context = appContext;
        }
        try {
            return context.getClassLoader().loadClass(className).newInstance();
        } catch (ReflectiveOperationException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Creates a context which can access classes from the specified split, but inherits theme
     * resources from the passed in context. This is useful if a context is needed to inflate
     * layouts which reference classes from a split.
     */
    public static Context createContextForInflation(Context context, String splitName) {
        if (!isIsolatedSplitInstalled(splitName)) {
            return context;
        }
        ClassLoader splitClassLoader = registerSplitClassLoaderForInflation(splitName);
        return new ContextWrapper(context) {
            @Override
            public ClassLoader getClassLoader() {
                return splitClassLoader;
            }

            @Override
            public Object getSystemService(String name) {
                Object ret = super.getSystemService(name);
                if (Context.LAYOUT_INFLATER_SERVICE.equals(name)) {
                    ret = ((LayoutInflater) ret).cloneInContext(this);
                }
                return ret;
            }
        };
    }

    /**
     * Returns the ClassLoader for the given split, loading the split if it has not yet been
     * loaded.
     */
    public static ClassLoader getOrCreateSplitClassLoader(String splitName) {
        ClassLoader ret;
        synchronized (sCachedClassLoaders) {
            ret = sCachedClassLoaders.get(splitName);
        }

        if (ret == null) {
            // Do not hold lock since split loading can be slow.
            createIsolatedSplitContext(ContextUtils.getApplicationContext(), splitName);
            synchronized (sCachedClassLoaders) {
                ret = sCachedClassLoaders.get(splitName);
                assert ret != null;
            }
        }
        return ret;
    }

    public static ClassLoader registerSplitClassLoaderForInflation(String splitName) {
        ClassLoader splitClassLoader = getOrCreateSplitClassLoader(splitName);
        sInflationClassLoaders.put(splitName, splitClassLoader);
        return splitClassLoader;
    }

    public static boolean canLoadClass(ClassLoader classLoader, String className) {
        try {
            Class.forName(className, false, classLoader);
            return true;
        } catch (ClassNotFoundException e) {
            return false;
        }
    }

    public static ClassLoader getSplitCompatClassLoader() {
        // SplitCompatClassLoader needs to be lazy loaded to ensure the Chrome
        // context is loaded and its class loader is set as the parent
        // classloader for the SplitCompatClassLoader. This happens in
        // Application#attachBaseContext.
        if (sSplitCompatClassLoaderInstance == null) {
            sSplitCompatClassLoaderInstance = new SplitCompatClassLoader();
        }
        return sSplitCompatClassLoaderInstance;
    }

    public static void saveLoadedSplits(Bundle outState) {
        outState.putStringArrayList(
                LOADED_SPLITS_KEY, new ArrayList(sInflationClassLoaders.keySet()));
    }

    public static void restoreLoadedSplits(Bundle savedInstanceState) {
        if (savedInstanceState == null) {
            return;
        }
        sSplitsToRestore = savedInstanceState.getStringArrayList(LOADED_SPLITS_KEY);
    }

    private static class SplitCompatClassLoader extends ClassLoader {
        private static final String TAG = "SplitCompatClassLoader";

        public SplitCompatClassLoader() {
            // The chrome split classloader if the chrome split exists, otherwise
            // the base module class loader.
            super(ContextUtils.getApplicationContext().getClassLoader());
            Log.i(TAG, "Splits: %s", sSplitsToRestore);
        }

        private Class<?> checkSplitsClassLoaders(String className) throws ClassNotFoundException {
            for (ClassLoader cl : sInflationClassLoaders.values()) {
                try {
                    return cl.loadClass(className);
                } catch (ClassNotFoundException ignore) {
                }
            }
            return null;
        }

        /**
         * Loads the class with the specified binary name.
         */
        @Override
        public Class<?> findClass(String cn) throws ClassNotFoundException {
            Class<?> foundClass = checkSplitsClassLoaders(cn);
            if (foundClass != null) {
                return foundClass;
            }
            // We will never have android.* classes in isolated split class loaders,
            // but android framework inflater does sometimes try loading classes
            // that do not exist when inflating xml files on startup.
            if (!cn.startsWith("android.")) {
                // If we fail from all the currently loaded classLoaders, lets
                // try loading some splits that were loaded when chrome was last
                // run and check again.
                if (sSplitsToRestore != null) {
                    restoreSplitsClassLoaders();
                    foundClass = checkSplitsClassLoaders(cn);
                    if (foundClass != null) {
                        return foundClass;
                    }
                }
                Log.w(TAG, "No class %s amongst %s", cn,
                        TextUtils.join("\n", sInflationClassLoaders.keySet()));
            }
            throw new ClassNotFoundException(cn);
        }

        private void restoreSplitsClassLoaders() {
            // Load splits that were stored in the SavedInstanceState Bundle.
            for (String splitName : sSplitsToRestore) {
                if (!sInflationClassLoaders.containsKey(splitName)) {
                    registerSplitClassLoaderForInflation(splitName);
                }
            }
            sSplitsToRestore = null;
        }
    }

    @Nullable
    private static String getSplitApkLibraryPath(String libraryName, String splitName) {
        // If isolated splits aren't supported, the library should have already been found.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return null;
        }

        String apkPath = getSplitApkPath(splitName);
        if (apkPath == null) {
            return null;
        }

        try {
            ApplicationInfo info = ContextUtils.getApplicationContext().getApplicationInfo();
            String primaryCpuAbi = (String) info.getClass().getField("primaryCpuAbi").get(info);
            // This matches the logic LoadedApk.java uses to construct library paths.
            return apkPath + "!/lib/" + primaryCpuAbi + "/" + System.mapLibraryName(libraryName);
        } catch (ReflectiveOperationException e) {
            throw new RuntimeException(e);
        }
    }

    private static boolean isApplicationContext(Context context) {
        while (context instanceof ContextWrapper) {
            if (context instanceof Application) return true;
            context = ((ContextWrapper) context).getBaseContext();
        }
        return false;
    }
}
