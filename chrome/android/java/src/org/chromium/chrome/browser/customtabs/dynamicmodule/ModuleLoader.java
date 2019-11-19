// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import android.content.ComponentCallbacks2;
import android.content.ComponentName;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.IBinder;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import dalvik.system.DexClassLoader;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.customtabs.dynamicmodule.ModuleMetrics.DestructionReason;
import org.chromium.components.crash.CrashKeyIndex;
import org.chromium.components.crash.CrashKeys;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/**
 * Dynamically loads a module from another apk.
 */
public class ModuleLoader {
    private static final String TAG = "ModuleLoader";

    private static final String DEX_FILE_PREFIX = "custom_tabs_module_dex_";
    private static final String DEX_LAST_UPDATE_TIME_PREF_PREFIX =
            "pref_local_custom_tabs_module_dex_last_update_time_";

    /** Specifies the module package name and entry point class name. */
    private final ComponentName mComponentName;
    @Nullable
    private final String mDexAssetName;
    private final DexInputStreamProvider mDexInputStreamProvider;
    private final DexClassLoaderProvider mDexClassLoaderProvider;
    private final ModuleApkVersion mModuleApkVersion;
    private final String mModuleId;

    /** @param moduleContext The context for the package to load the class from. */
    private Context mModuleContext;

    @Nullable
    private ClassLoader mClassLoader;
    private boolean mIsClassLoaderCreating;
    private boolean mNeedsToLoadModule;

    /**
     * Tracks the number of usages of the module. If it is no longer used, it may be destroyed, but
     * the time of destruction depends on the caching policy.
     */
    private int mModuleUseCount;

    private boolean mIsModuleLoading;

    private final ObserverList<Callback<ModuleEntryPoint>> mCallbacks = new ObserverList<>();
    private final List<Bundle> mPendingBundles = new ArrayList<>();

    /**
     * The timestamp of the moment the module became unused. This is used to determine whether or
     * not to continue caching it. A value of -1 indicates there is no usable value.
     */
    private long mModuleUnusedTimeMs = -1;

    /**
     * The name of the experiment parameter for setting the caching time limit.
     */
    private static final String MODULE_CACHE_TIME_LIMIT_MS_NAME = "cct_module_cache_time_limit_ms";

    /**
     * The default time limit for caching an unused module under mild memory pressure, in
     * milliseconds.
     */
    private static final int MODULE_CACHE_TIME_LIMIT_MS_DEFAULT = 300000; // 5 minutes

    @Nullable
    private ModuleEntryPoint mModuleEntryPoint;

    /**
     * Helper class to store info about module version.
     */
    public static class ModuleApkVersion {
        final int mApkVersionCode;
        final String mApkVersionName;
        final long mLastUpdateTime;

        ModuleApkVersion(int apkVersionCode, String apkVersionName, long lastUpdateTime) {
            mApkVersionCode = apkVersionCode;
            mApkVersionName = apkVersionName;
            mLastUpdateTime = lastUpdateTime;
        }

        public static ModuleApkVersion getModuleVersion(String packageName) {
            int apkVersionCode = 0;
            String apkVersionName = "";
            long lastUpdateTime = 0;

            try {
                PackageInfo info = ContextUtils.getApplicationContext()
                        .getPackageManager()
                        .getPackageInfo(packageName, 0);
                apkVersionCode = info.versionCode;
                apkVersionName = info.versionName;
                lastUpdateTime = info.lastUpdateTime;
            } catch (PackageManager.NameNotFoundException ignored) {
                // Ignore the exception.
                // Failure to find the package name will be handled createModuleContext().
            }

            return new ModuleApkVersion(apkVersionCode, apkVersionName, lastUpdateTime);
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (!(o instanceof ModuleApkVersion)) return false;

            ModuleApkVersion that = (ModuleApkVersion) o;

            if (mApkVersionCode != that.mApkVersionCode) return false;
            if (mLastUpdateTime != that.mLastUpdateTime) return false;
            return TextUtils.equals(mApkVersionName, that.mApkVersionName);
        }

        @Override
        public int hashCode() {
            return Objects.hash(mApkVersionCode, mApkVersionName, mLastUpdateTime);
        }
    }

    /**
     * Instantiates a new {@link ModuleLoader}.
     * @param componentName Specifies the module package name and entry point class name.
     * @param dexAssetName Identifier for the asset that contains the dex file to load the
     *         module from. {@code null} if the module should not be loaded from a dex file.
     */
    public ModuleLoader(ComponentName componentName, @Nullable String dexAssetName) {
        this(componentName, dexAssetName, new DexInputStreamProviderImpl(),
                new DexClassLoaderProviderImpl());
    }

    @VisibleForTesting
    /* package */ ModuleLoader(ComponentName componentName, @Nullable String dexAssetName,
            DexInputStreamProvider dexInputStreamProvider,
            DexClassLoaderProvider dexClassLoaderProvider) {
        mComponentName = componentName;
        mDexAssetName = dexAssetName;
        mDexInputStreamProvider = dexInputStreamProvider;
        mDexClassLoaderProvider = dexClassLoaderProvider;
        String packageName = componentName.getPackageName();
        mModuleApkVersion = ModuleApkVersion.getModuleVersion(packageName);
        mModuleId = String.format("%s v%s (%s)",
                packageName, mModuleApkVersion.mApkVersionCode, mModuleApkVersion.mApkVersionName);

        mModuleContext = createModuleContext(
                mComponentName.getPackageName(), /* resourcesOnly = */ mDexAssetName != null);
    }

    public ModuleApkVersion getModuleApkVersion() {
        return mModuleApkVersion;
    }

    public ComponentName getComponentName() {
        return mComponentName;
    }

    @Nullable
    public String getDexAssetName() {
        return mDexAssetName;
    }

    /**
     * If the module is not loaded yet, dynamically loads the module entry point class.
     */
    public void loadModule() {
        if (mClassLoader == null) {
            mNeedsToLoadModule = true;
            if (!mIsClassLoaderCreating) createClassLoader();
            return;
        }

        if (mIsModuleLoading) return;

        // If module has been already loaded all callbacks must be notified synchronously.
        // {@see #addCallbackAndIncrementUseCount}
        if (mModuleEntryPoint != null) {
            assert mCallbacks.isEmpty();
            return;
        }

        if (mModuleContext == null) {
            runAndClearCallbacks();
            return;
        }

        ModuleMetrics.registerLifecycleState(ModuleMetrics.LifecycleState.NOT_LOADED);

        mIsModuleLoading = true;
        new LoadClassTask().executeWithTaskTraits(TaskTraits.USER_VISIBLE_MAY_BLOCK);
    }

    public void createClassLoader() {
        if (mClassLoader != null) return;

        mIsClassLoaderCreating = true;
        new ClassLoaderTask().executeWithTaskTraits(TaskTraits.USER_VISIBLE_MAY_BLOCK);
    }

    /**
     * Loads the dynamic module if it is not loaded yet,
     * and transfers the bundle to it regardless of the previous loaded state.
     */
    public void sendBundleToModule(Bundle bundle) {
        if (mModuleEntryPoint != null) {
            mModuleEntryPoint.onBundleReceived(bundle);
            return;
        }
        mPendingBundles.add(bundle);
        loadModule();
    }

    /**
     * Register a callback to receive a {@link ModuleEntryPoint} asynchronously.
     * If the module fails to load, the callback will receive null.
     * If the module was already loaded and a reference to it is still held,
     * the callback will synchronously receive a {@link ModuleEntryPoint}.
     *
     * Module use count is incremented when a callback notified.
     *
     * @param callback The callback to receive the result.
     */
    public void addCallbackAndIncrementUseCount(Callback<ModuleEntryPoint> callback) {
        if (mModuleEntryPoint != null) {
            mModuleUseCount++;
            mModuleUnusedTimeMs = -1;
            ModuleMetrics.recordLoadResult(ModuleMetrics.LoadResult.SUCCESS_CACHED);
            callback.onResult(mModuleEntryPoint);
            return;
        }
        mCallbacks.addObserver(callback);
    }

    public void removeCallbackAndDecrementUseCount(Callback<ModuleEntryPoint> callback) {
        boolean isPendingCallback = mCallbacks.removeObserver(callback);
        if (mModuleEntryPoint == null || isPendingCallback) return;

        mModuleUseCount--;
        if (mModuleUseCount == 0) {
            mModuleUnusedTimeMs = ModuleMetrics.now();
            if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_MODULE_CACHE)) {
                destroyModule(DestructionReason.NO_CACHING_UNUSED);
            }
        }
    }

    /**
     * Destroys the unused cached module (if present) under certain circumstances. If the memory
     * signal is considered severe, the module will always be destroyed. If the memory signal is
     * considered mild, the module will only be destroyed if the time limit has passed.
     * @param level The type of signal as defined in {@link ComponentCallbacks2}.
     */
    public void onTrimMemory(int level) {
        if (mModuleEntryPoint == null || mModuleUseCount > 0) return;

        if (ChromeApplication.isSevereMemorySignal(level)) {
            destroyModule(DestructionReason.CACHED_SEVERE_MEMORY_PRESSURE);
        } else if (cacheExceededTimeLimit()) {
            if (level == ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN) {
                destroyModule(DestructionReason.CACHED_UI_HIDDEN_TIME_EXCEEDED);
            } else {
                destroyModule(DestructionReason.CACHED_MILD_MEMORY_PRESSURE_TIME_EXCEEDED);
            }
        }
    }

    private boolean cacheExceededTimeLimit() {
        if (mModuleUnusedTimeMs == -1) return false;
        long limit = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.CCT_MODULE_CACHE, MODULE_CACHE_TIME_LIMIT_MS_NAME,
                MODULE_CACHE_TIME_LIMIT_MS_DEFAULT);
        return ModuleMetrics.now() - mModuleUnusedTimeMs > limit;
    }

    public void destroyModule(@DestructionReason int reason) {
        if (mModuleEntryPoint == null) return;

        ModuleMetrics.recordDestruction(reason);
        mModuleEntryPoint.onDestroy();
        CrashKeys.getInstance().set(CrashKeyIndex.ACTIVE_DYNAMIC_MODULE, null);
        ModuleMetrics.registerLifecycleState(ModuleMetrics.LifecycleState.DESTROYED);
        mModuleEntryPoint = null;
        mModuleUnusedTimeMs = -1;
    }

    /**
     * Notify all callbacks which are waiting for module loading. Each callback is needed to notify
     * only once therefore all callbacks are cleared after call.
     */
    private void runAndClearCallbacks() {
        assert !mIsModuleLoading;
        if (mModuleEntryPoint != null && mCallbacks.size() > 0) {
            mModuleUseCount += mCallbacks.size();
            mModuleUnusedTimeMs = -1;
        }

        for (Callback<ModuleEntryPoint> callback: mCallbacks) {
            callback.onResult(mModuleEntryPoint);
        }
        mCallbacks.clear();
    }

    private void sendAllBundles() {
        assert !mIsModuleLoading;
        for (Bundle bundle: mPendingBundles) {
            mModuleEntryPoint.onBundleReceived(bundle);
        }
        mPendingBundles.clear();
    }

    @VisibleForTesting
    /* package */ void cleanUpLocalDex() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .remove(getDexLastUpdateTimePrefName())
                .apply();
        FileUtils.recursivelyDeleteFile(getDexDirectory());
    }

    @VisibleForTesting
    /* package */ File getDexDirectory() {
        return ContextUtils.getApplicationContext().getDir(
                getDexDirectoryName(mComponentName.getPackageName()), Context.MODE_PRIVATE);
    }

    @VisibleForTesting
    /* package */ File getDexFile() {
        return new File(getDexDirectory(), getDexFileName(mComponentName.getPackageName()));
    }

    @VisibleForTesting
    /* package */ long getModuleLastUpdateTime() {
        return mModuleApkVersion.mLastUpdateTime;
    }

    @VisibleForTesting
    /* package */ String getDexLastUpdateTimePrefName() {
        return DEX_LAST_UPDATE_TIME_PREF_PREFIX + mComponentName.getPackageName();
    }

    private static String getDexDirectoryName(String packageName) {
        return DEX_FILE_PREFIX + packageName;
    }

    private static String getDexFileName(String packageName) {
        return DEX_FILE_PREFIX + packageName;
    }

    /**
     * A task for creating module {@link ClassLoader}.
     */
    private class ClassLoaderTask extends AsyncTask<ClassLoader> {
        @Override
        @Nullable
        protected ClassLoader doInBackground() {
            try {
                boolean loadFromDex = updateModuleDexInDiskIfNeeded();
                mClassLoader = getModuleClassLoader(loadFromDex);
                return mClassLoader;
            } catch (IOException e) {
                Log.e(TAG, "Could not copy dex to local storage", e);
                ModuleMetrics.recordLoadResult(
                        ModuleMetrics.LoadResult.FAILED_TO_COPY_DEX_EXCEPTION);
            }
            return null;
        }

        @Override
        protected void onPostExecute(ClassLoader classLoader) {
            mIsClassLoaderCreating = false;
            if (mNeedsToLoadModule) {
                mNeedsToLoadModule = false;
                loadModule();
            }
        }

        /**
         * Updates the local copy of the module dex file in disk if necessary.
         *
         * <p>This method first checks the existing module lastUpdateTime in shared preferences and
         * compares it to the new lastUpdateTime of the module.
         *
         * @return Whether the module code should be loaded from the dex file.
         */
        private boolean updateModuleDexInDiskIfNeeded() throws IOException {
            SharedPreferences preferences = ContextUtils.getAppSharedPreferences();
            String dexLastUpdateTimePref = getDexLastUpdateTimePrefName();
            long localDexLastUpdateTime = preferences.getLong(dexLastUpdateTimePref, -1);

            if (mDexAssetName == null) {
                if (localDexLastUpdateTime != -1) {
                    // The module had a dex before but now it doesn't. Clean up previous local dex.
                    cleanUpLocalDex();
                }
                return false;
            } else if (localDexLastUpdateTime != mModuleApkVersion.mLastUpdateTime) {
                try {
                    copyDexToDisk(mDexAssetName);
                    preferences.edit()
                            .putLong(dexLastUpdateTimePref, mModuleApkVersion.mLastUpdateTime)
                            .apply();
                } catch (IOException e) {
                    if (localDexLastUpdateTime != -1) {
                        cleanUpLocalDex();
                    }
                    throw e;
                }
            }
            return true;
        }

        /**
         * Copies the dex file with the given {@code dexAssetName} from the module's context
         * into the local storage.
         */
        private void copyDexToDisk(String dexAssetName) throws IOException {
            InputStream in =
                    mDexInputStreamProvider.createInputStream(dexAssetName, mModuleContext);
            FileUtils.copyStreamToFile(in, getDexFile());
        }

        private ClassLoader getModuleClassLoader(boolean loadFromDex) {
            if (mDexAssetName == null || !loadFromDex) {
                // Load directly from the APK if an extra dex file is not provided.
                return mModuleContext.getClassLoader();
            }
            return mDexClassLoaderProvider.createClassLoader(getDexFile());
        }
    }

    /**
     * A task for loading the module entry point class on a background thread.
     */
    private class LoadClassTask extends AsyncTask<Class<?>> {
        @Override
        @Nullable
        protected Class<?> doInBackground() {
            if (mClassLoader == null) return null;
            try {
                long entryPointLoadClassStartTime = ModuleMetrics.now();
                Class<?> clazz = mClassLoader.loadClass(mComponentName.getClassName());
                ModuleMetrics.recordLoadClassTime(entryPointLoadClassStartTime);
                return clazz;
            } catch (ClassNotFoundException e) {
                Log.e(TAG, "Could not find class %s", mComponentName.getClassName(), e);
                ModuleMetrics.recordLoadResult(ModuleMetrics.LoadResult.CLASS_NOT_FOUND_EXCEPTION);
            }
            return null;
        }

        @Override
        protected void onPostExecute(@Nullable Class<?> clazz) {
            mIsModuleLoading = false;
            if (clazz == null) {
                runAndClearCallbacks();
                return;
            }

            try {
                long entryPointNewInstanceStartTime = ModuleMetrics.now();
                IBinder binder = (IBinder) clazz.newInstance();
                ModuleMetrics.recordEntryPointNewInstanceTime(entryPointNewInstanceStartTime);

                ModuleHostImpl moduleHost =
                        new ModuleHostImpl(ContextUtils.getApplicationContext(), mModuleContext);
                ModuleEntryPoint entryPoint =
                        new ModuleEntryPoint(IModuleEntryPoint.Stub.asInterface(binder));

                if (!isCompatible(moduleHost, entryPoint)) {
                    Log.w(TAG,
                            "Incompatible module due to version mismatch: host version %s, "
                                    + "minimum required host version %s, entry point version %s, "
                                    + "minimum required entry point version %s.",
                            moduleHost.getHostVersion(), entryPoint.getMinimumHostVersion(),
                            entryPoint.getModuleVersion(), moduleHost.getMinimumModuleVersion());
                    ModuleMetrics.recordLoadResult(ModuleMetrics.LoadResult.INCOMPATIBLE_VERSION);
                    runAndClearCallbacks();
                    return;
                }

                CrashKeys crashKeys = CrashKeys.getInstance();
                crashKeys.set(CrashKeyIndex.LOADED_DYNAMIC_MODULE, mModuleId);
                crashKeys.set(CrashKeyIndex.ACTIVE_DYNAMIC_MODULE, mModuleId);
                crashKeys.set(CrashKeyIndex.DYNAMIC_MODULE_DEX_NAME, mDexAssetName);

                ModuleMetrics.registerLifecycleState(ModuleMetrics.LifecycleState.INSTANTIATED);

                long entryPointInitStartTime = ModuleMetrics.now();
                entryPoint.init(moduleHost);
                ModuleMetrics.recordEntryPointInitTime(entryPointInitStartTime);

                ModuleMetrics.recordLoadResult(ModuleMetrics.LoadResult.SUCCESS_NEW);

                mModuleEntryPoint = entryPoint;
                mModuleUnusedTimeMs = ModuleMetrics.now();
                runAndClearCallbacks();
                sendAllBundles();

                return;
            } catch (Exception e) {
                // No multi-catch below API level 19 for reflection exceptions.
                // This catches InstantiationException and IllegalAccessException.
                Log.e(TAG, "Could not instantiate class %s", mComponentName.getClassName(), e);
                ModuleMetrics.recordLoadResult(ModuleMetrics.LoadResult.INSTANTIATION_EXCEPTION);
            }
            runAndClearCallbacks();
        }
    }

    @Nullable
    private static Context createModuleContext(String packageName, boolean resourcesOnly) {
        try {
            // The flags Context.CONTEXT_INCLUDE_CODE and Context.CONTEXT_IGNORE_SECURITY are
            // needed to be able to load classes via the classloader of the returned context.
            long createPackageContextStartTime = ModuleMetrics.now();
            int flags = resourcesOnly
                    ? 0
                    : Context.CONTEXT_INCLUDE_CODE | Context.CONTEXT_IGNORE_SECURITY;
            Context moduleContext =
                    ContextUtils.getApplicationContext().createPackageContext(packageName, flags);
            ModuleMetrics.recordCreatePackageContextTime(createPackageContextStartTime);
            return moduleContext;
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Could not create package context for %s", packageName, e);
            ModuleMetrics.recordLoadResult(
                    ModuleMetrics.LoadResult.PACKAGE_NAME_NOT_FOUND_EXCEPTION);
        }
        return null;
    }

    private static boolean isCompatible(ModuleHostImpl moduleHost, ModuleEntryPoint entryPoint) {
        return entryPoint.getModuleVersion() >= moduleHost.getMinimumModuleVersion()
                && moduleHost.getHostVersion() >= entryPoint.getMinimumHostVersion();
    }

    @VisibleForTesting
    public int getModuleUseCount() {
        return mModuleUseCount;
    }

    /**
     * Provides an {@link InputStream} for the dex file content. This is abstracted to use a fake
     * for testing.
     */
    @VisibleForTesting
    public interface DexInputStreamProvider {
        InputStream createInputStream(@Nullable String dexAssetName, Context moduleContext)
                throws IOException;
    }

    private static class DexInputStreamProviderImpl implements DexInputStreamProvider {
        @Override
        public InputStream createInputStream(@Nullable String dexAssetName, Context moduleContext)
                throws IOException {
            return moduleContext.getResources().getAssets().open(dexAssetName);
        }
    }

    /** Creates a ClassLoader from a dex file. This is abstracted to use a fake for testing. */
    @VisibleForTesting
    public interface DexClassLoaderProvider {
        ClassLoader createClassLoader(File dexFile);
    }

    private static class DexClassLoaderProviderImpl implements DexClassLoaderProvider {
        @Override
        public ClassLoader createClassLoader(File dexFile) {
            File optimizedDexDirectory = dexFile.getParentFile();

            // It is important to use the system ClassLoader as the base ClassLoader. Class
            // loading is performed top-down, which means if we use Chrome ClassLoader as the
            // base it will be used first to find class references. This is a problem as the
            // the loaded module and Chrome could possibly use same classes but different
            // versions of it. Such a case would cause RuntimeExceptions in the module.
            return new DexClassLoader(dexFile.getAbsolutePath(),
                    optimizedDexDirectory.getAbsolutePath(), null,
                    ClassLoader.getSystemClassLoader());
        }
    }
}
