// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.content.Context;

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
 * Implementation of {@link SplitCompatApplication.Impl} that will be initialized in all processes.
 */
public class MainDexApplicationImpl extends SplitCompatApplication.Impl {
    @Override
    @CallSuper
    public void onCreate() {
        // These can't go in attachBaseContext because Context.getApplicationContext() (which
        // they use under-the-hood) does not work until after it returns.
        FontPreloadingWorkaround.maybeInstallWorkaround(getApplication());
        MemoryPressureMonitor.INSTANCE.registerComponentCallbacks();
    }

    @Override
    @CallSuper
    public void attachBaseContext(Context context) {
        super.attachBaseContext(context);
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

    public boolean isWebViewProcess() {
        return false;
    }

    private void maybeInitProcessType() {
        if (SplitCompatApplication.isBrowserProcess()) {
            LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
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
