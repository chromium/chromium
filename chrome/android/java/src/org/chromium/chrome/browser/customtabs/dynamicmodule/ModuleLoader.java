// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import android.content.ComponentCallbacks2;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.os.Process;
import android.support.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.crash.CrashKeyIndex;
import org.chromium.chrome.browser.crash.CrashKeys;
import org.chromium.chrome.browser.customtabs.dynamicmodule.ModuleMetrics.DestructionReason;

/**
 * Dynamically loads a module from another apk.
 */
public class ModuleLoader {
    private static final String TAG = "ModuleLoader";

    /** Specifies the module package name and entry point class name. */
    private final ComponentName mComponentName;
    private final String mModuleId;

    /**
     * Tracks the number of usages of the module. If it is no longer used, it may be destroyed, but
     * the time of destruction depends on the caching policy.
     */
    private int mModuleUseCount;

    private boolean mIsModuleLoading;

    private final ObserverList<Callback<ModuleEntryPoint>> mCallbacks = new ObserverList<>();

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
     * Instantiates a new {@link ModuleLoader}.
     * @param componentName Specifies the module package name and entry point class name.
     */
    public ModuleLoader(ComponentName componentName) {
        mComponentName = componentName;
        String packageName = componentName.getPackageName();
        int versionCode = 0;
        String versionName = "";
        try {
            PackageInfo info = ContextUtils.getApplicationContext()
                                     .getPackageManager()
                                     .getPackageInfo(packageName, 0);
            versionCode = info.versionCode;
            versionName = info.versionName;
        } catch (PackageManager.NameNotFoundException ignored) {
            // Ignore the exception. Failure to find the package name will be handled in
            // getModuleContext() below.
        }
        mModuleId = String.format("%s v%s (%s)", packageName, versionCode, versionName);
    }

    public ComponentName getComponentName() {
        return mComponentName;
    }

    /**
     * If the module is not loaded yet, dynamically loads the module entry point class.
     */
    public void loadModule() {
        if (mIsModuleLoading) return;

        // If module has been already loaded all callbacks must be notified synchronously.
        // {@see #addCallbackAndIncrementUseCount}
        if (mModuleEntryPoint != null) {
            assert mCallbacks.isEmpty();
            return;
        }

        Context moduleContext = getModuleContext(mComponentName.getPackageName());
        if (moduleContext == null) {
            runAndClearCallbacks();
            return;
        }

        mIsModuleLoading = true;
        new LoadClassTask(moduleContext).executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
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

    private void destroyModule(@DestructionReason int reason) {
        assert mModuleEntryPoint != null;
        ModuleMetrics.recordDestruction(reason);
        mModuleEntryPoint.onDestroy();
        CrashKeys.getInstance().set(CrashKeyIndex.ACTIVE_DYNAMIC_MODULE, null);
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

    /**
     * A task for loading the module entry point class on a background thread.
     */
    private class LoadClassTask extends AsyncTask<Class<?>> {
        private final Context mModuleContext;

        /**
         * Constructs the task.
         * @param moduleContext The context for the package to load the class from.
         */
        LoadClassTask(Context moduleContext) {
            mModuleContext = moduleContext;
        }

        @Override
        @Nullable
        protected Class<?> doInBackground() {
            int oldPriority = Process.getThreadPriority(0);
            try {
                // We don't want to block the UI thread, but we don't want to be really slow either.
                // The AsyncTask class sets the thread priority quite low
                // (THREAD_PRIORITY_BACKGROUND) and does not distinguish between user-visible
                // user-invisible tasks.
                // TODO(crbug.com/863457): Replace this with something like a task trait that
                // influences priority once we have a task scheduler in Java.
                Process.setThreadPriority(Process.THREAD_PRIORITY_DEFAULT);

                long entryPointLoadClassStartTime = ModuleMetrics.now();
                Class<?> clazz =
                        mModuleContext.getClassLoader().loadClass(mComponentName.getClassName());
                ModuleMetrics.recordLoadClassTime(entryPointLoadClassStartTime);
                return clazz;
            } catch (ClassNotFoundException e) {
                Log.e(TAG, "Could not find class %s", mComponentName.getClassName(), e);
                ModuleMetrics.recordLoadResult(ModuleMetrics.LoadResult.CLASS_NOT_FOUND_EXCEPTION);
            } finally {
                Process.setThreadPriority(oldPriority);
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

                long entryPointInitStartTime = ModuleMetrics.now();
                entryPoint.init(moduleHost);
                ModuleMetrics.recordEntryPointInitTime(entryPointInitStartTime);
                ModuleMetrics.recordLoadResult(ModuleMetrics.LoadResult.SUCCESS_NEW);
                mModuleEntryPoint = entryPoint;
                mModuleUnusedTimeMs = ModuleMetrics.now();
                runAndClearCallbacks();
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
    private static Context getModuleContext(String packageName) {
        try {
            // The flags Context.CONTEXT_INCLUDE_CODE and Context.CONTEXT_IGNORE_SECURITY are
            // needed to be able to load classes via the classloader of the returned context.
            long createPackageContextStartTime = ModuleMetrics.now();
            Context moduleContext = ContextUtils.getApplicationContext().createPackageContext(
                    packageName, Context.CONTEXT_INCLUDE_CODE | Context.CONTEXT_IGNORE_SECURITY);
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
}
