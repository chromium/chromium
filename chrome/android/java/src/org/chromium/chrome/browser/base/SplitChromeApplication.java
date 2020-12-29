// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import static org.chromium.chrome.browser.base.SplitCompatUtils.CHROME_SPLIT_NAME;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.SystemClock;

import dalvik.system.DexFile;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.JNIUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.compat.ApiHelperForO;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.version.ChromeVersionInfo;

import java.lang.reflect.Field;

/**
 * Application class to use for Chrome when //chrome code is in an isolated split. This class will
 * perform any necessary initialization for non-browser processes without loading code from the
 * chrome split. In the browser process, the necessary logic is loaded from the chrome split using
 * reflection.
 *
 * This class will be used when isolated splits are enabled.
 */
public class SplitChromeApplication extends SplitCompatApplication {
    private static final String TAG = "SplitChromeApp";
    private String mChromeApplicationClassName;
    private SplitPreloader mSplitPreloader;

    public SplitChromeApplication() {
        this(SplitCompatUtils.getIdentifierName(
                "org.chromium.chrome.browser.ChromeApplication$ChromeApplicationImpl"));
    }

    public SplitChromeApplication(String chromeApplicationClassName) {
        mChromeApplicationClassName = chromeApplicationClassName;
    }

    @Override
    public void onCreate() {
        if (mSplitPreloader != null) {
            mSplitPreloader.wait(CHROME_SPLIT_NAME);
        }
        super.onCreate();
    }

    @Override
    protected void attachBaseContext(Context context) {
        super.attachBaseContext(context);
        if (isBrowserProcess()) {
            setImplSupplier(() -> {
                Context chromeContext = SplitCompatUtils.createChromeContext(this);
                return (Impl) SplitCompatUtils.newInstance(
                        chromeContext, mChromeApplicationClassName);
            });
            applyActivityClassLoaderWorkaround();
            applyDexCompileWorkaround();
        } else {
            setImplSupplier(() -> createNonBrowserApplication());
        }
    }

    @Override
    public Context createContextForSplit(String name) throws PackageManager.NameNotFoundException {
        try (TraceEvent te = TraceEvent.scoped("SplitChromeApplication.createContextForSplit")) {
            if (mSplitPreloader != null) {
                // Wait for any splits that are preloading so we don't have a race to update the
                // class loader cache (b/172602571).
                mSplitPreloader.wait(name);
            }
            long startTime = SystemClock.uptimeMillis();
            Context context = super.createContextForSplit(name);
            RecordHistogram.recordTimesHistogram("Android.IsolatedSplits.ContextCreateTime." + name,
                    SystemClock.uptimeMillis() - startTime);
            return context;
        }
    }

    @Override
    protected void performBrowserProcessPreloading(Context context) {
        // The chrome split has a large amount of code, which can slow down startup. Loading
        // this in the background allows us to do this in parallel with startup tasks which do
        // not depend on code in the chrome split.
        mSplitPreloader = new SplitPreloader(context);
        // If the chrome module is not enabled or isolated splits are not supported (e.g. in Android
        // N), the onComplete function will run immediately so it must handle the case where the
        // base context of the application has not been set yet.
        mSplitPreloader.preload(CHROME_SPLIT_NAME, (chromeContext) -> {
            // When installed, the vr module is always loaded on startup, so preload here.
            mSplitPreloader.preload("vr", null);
            // If the chrome module is not enabled or isolated splits are not supported,
            // chromeContext will have the same ClassLoader as the base context, so no need to
            // replace the ClassLoaders here.
            if (!context.getClassLoader().equals(chromeContext.getClassLoader())) {
                replaceClassLoader(this, chromeContext.getClassLoader());
                JNIUtils.setClassLoader(chromeContext.getClassLoader());
            }
        });
    }

    protected Impl createNonBrowserApplication() {
        return new Impl();
    }

    /**
     * Fixes Activity ClassLoader if necessary. Isolated splits can cause a ClassLoader mismatch
     * between the Application and Activity ClassLoaders. We have a workaround in
     * SplitCompatAppComponentFactory which overrides the Activity ClassLoader, but this does not
     * change the ClassLoader for the Activity's base context. We override that ClassLoader here, so
     * it matches the ClassLoader that was used to load the Activity class. Note that
     * ContextUtils.getApplicationContext().getClassLoader() may not give the right ClassLoader here
     * because the Activity may be in a DFM which is a child of the chrome DFM. See
     * crbug.com/1146745 for more info.
     */
    private static void applyActivityClassLoaderWorkaround() {
        ApplicationStatus.registerStateListenerForAllActivities(
                new ApplicationStatus.ActivityStateListener() {
                    @Override
                    public void onActivityStateChange(
                            Activity activity, @ActivityState int newState) {
                        if (newState != ActivityState.CREATED) {
                            return;
                        }

                        // ClassLoaders are already the same, no workaround needed.
                        if (activity.getClassLoader().equals(
                                    activity.getClass().getClassLoader())) {
                            return;
                        }

                        replaceClassLoader(
                                activity.getBaseContext(), activity.getClass().getClassLoader());
                    }
                });
    }

    /**
     * Android OMR1 has a bug where bg-dexopt-job will break optimized dex files for splits. This
     * leads to *very* slow startup on those devices. To mitigate this, we attempt to force a dex
     * compile if necessary.
     */
    private void applyDexCompileWorkaround() {
        // This bug only happens in OMR1. Skip the workaround on local builds to avoid affecting
        // perf bots.
        if (Build.VERSION.SDK_INT != Build.VERSION_CODES.O_MR1
                || ChromeVersionInfo.isLocalBuild()) {
            return;
        }
        // Wait until startup completes so this doesn't slow down early startup or mess with
        // compiled dex files before they get loaded initially.
        DeferredStartupHandler.getInstance().addDeferredTask(() -> {
            // BEST_EFFORT will only affect when the task runs, the dexopt will run with
            // normal priority (but in a separate process, due to using Runtime.exec()).
            PostTask.postTask(TaskTraits.BEST_EFFORT_MAY_BLOCK, () -> {
                try {
                    // If the app has just been updated, it will be compiled with
                    // quicken. The next time bg-dexopt-job runs it will break the
                    // optimized dex for splits. If we force compile now, then
                    // bg-dexopt-job won't mess up the splits, and we save the user a
                    // slow startup.
                    if (needsDexCompileAfterUpdate()) {
                        performDexCompile();
                        return;
                    }

                    // Make sure all splits are compiled correclty, and if not force a
                    // compile.
                    String[] splitNames = ApiHelperForO.getSplitNames(getApplicationInfo());
                    for (int i = 0; i < splitNames.length; i++) {
                        // Ignore config splits like "config.en".
                        if (splitNames[i].contains(".")) {
                            continue;
                        }
                        if (DexFile.isDexOptNeeded(getApplicationInfo().splitSourceDirs[i])) {
                            performDexCompile();
                            return;
                        }
                    }
                } catch (Exception e) {
                    Log.e(TAG, "Error compiling dex.", e);
                }
            });
        });
    }

    /** Returns whether the dex has been compiled since the last app update. */
    private boolean needsDexCompileAfterUpdate() {
        return SharedPreferencesManager.getInstance().readInt(
                       ChromePreferenceKeys.ISOLATED_SPLITS_DEX_COMPILE_VERSION)
                != PackageUtils.getPackageVersion(this, getPackageName());
    }

    /** Compiles dex for the app, and sets the pref key tracking the latest compiled version. */
    private void performDexCompile() throws Exception {
        Runtime.getRuntime().exec(
                new String[] {"cmd", "package", "compile", "-r", "shared", getPackageName()});
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.ISOLATED_SPLITS_DEX_COMPILE_VERSION,
                PackageUtils.getPackageVersion(this, getPackageName()));
    }

    private static void replaceClassLoader(Context baseContext, ClassLoader classLoader) {
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
}
