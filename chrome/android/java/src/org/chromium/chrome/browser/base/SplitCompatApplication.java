// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;

import androidx.annotation.CallSuper;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.JNIUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.memory.MemoryPressureMonitor;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.ProductConfig;
import org.chromium.components.embedder_support.application.FontPreloadingWorkaround;
import org.chromium.components.module_installer.util.ModuleUtil;
import org.chromium.ui.base.ResourceBundle;

/**
 * Application base class which will call through to the given {@link Impl}. Application classes
 * which extend this class should also extend {@link Impl}, and call {@link #setImpl(Impl)} before
 * calling {@link attachBaseContext(Context)}.
 */
public class SplitCompatApplication extends Application {
    private Impl mImpl;

    /**
     * Holds the implementation of application logic. Will be called by {@link
     * SplitCompatApplication}.
     */
    protected static class Impl {
        private SplitCompatApplication mApplication;

        private final void setApplication(SplitCompatApplication application) {
            mApplication = application;
        }

        protected final SplitCompatApplication getApplication() {
            return mApplication;
        }

        @CallSuper
        public void onCreate() {
            // These can't go in attachBaseContext because Context.getApplicationContext() (which
            // they use under-the-hood) does not work until after it returns.
            FontPreloadingWorkaround.maybeInstallWorkaround(getApplication());
            MemoryPressureMonitor.INSTANCE.registerComponentCallbacks();
        }

        @CallSuper
        public void attachBaseContext(Context context) {
            mApplication.superAttachBaseContext(context);

            // Perform initialization of globals common to all processes.
            ContextUtils.initApplicationContext(getApplication());
            maybeInitProcessType();
            BundleUtils.setIsBundle(ProductConfig.IS_BUNDLE);

            // Write installed modules to crash keys. This needs to be done as early as possible so
            // that these values are set before any crashes are reported.
            ModuleUtil.updateCrashKeys();

            AsyncTask.takeOverAndroidThreadPool();
            JNIUtils.setClassLoader(getApplication().getClassLoader());
            ResourceBundle.setAvailablePakLocales(
                    ProductConfig.COMPRESSED_LOCALES, ProductConfig.UNCOMPRESSED_LOCALES);
            LibraryLoader.getInstance().setLinkerImplementation(
                    ProductConfig.USE_CHROMIUM_LINKER, ProductConfig.USE_MODERN_LINKER);
            LibraryLoader.getInstance().enableJniChecks();
        }

        public void onTrimMemory(int level) {}

        @CallSuper
        public void startActivity(Intent intent, Bundle options) {
            mApplication.superStartActivity(intent, options);
        }

        public void onConfigurationChanged(Configuration newConfig) {}

        public boolean isWebViewProcess() {
            return false;
        }

        private void maybeInitProcessType() {
            if (isBrowserProcess()) {
                LibraryLoader.getInstance().setLibraryProcessType(
                        LibraryProcessType.PROCESS_BROWSER);
                return;
            }
            // WebView initialization sets the correct process type.
            if (isWebViewProcess()) return;

            // Child processes set their own process type when bound.
            String processName = ContextUtils.getProcessName();
            if (processName.contains("privileged_process")
                    || processName.contains("sandboxed_process")) {
                return;
            }

            // We must be in an isolated service process.
            LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_CHILD);
        }
    }

    public final void setImpl(Impl impl) {
        assert mImpl == null;
        mImpl = impl;
        mImpl.setApplication(this);
    }

    /**
     * This exposes the super method so it can be called inside the Impl class code instead of just
     * at the start.
     */
    private void superAttachBaseContext(Context context) {
        super.attachBaseContext(context);
    }

    /**
     * This exposes the super method so it can be called inside the Impl class code instead of just
     * at the start.
     */
    private void superStartActivity(Intent intent, Bundle options) {
        super.startActivity(intent, options);
    }

    @Override
    protected void attachBaseContext(Context context) {
        mImpl.attachBaseContext(context);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        mImpl.onCreate();
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        mImpl.onTrimMemory(level);
    }

    /** Forward all startActivity() calls to the two argument version. */
    @Override
    public void startActivity(Intent intent) {
        startActivity(intent, null);
    }

    @Override
    public void startActivity(Intent intent, Bundle options) {
        mImpl.startActivity(intent, options);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        mImpl.onConfigurationChanged(newConfig);
    }

    public static boolean isBrowserProcess() {
        return !ContextUtils.getProcessName().contains(":");
    }
}
