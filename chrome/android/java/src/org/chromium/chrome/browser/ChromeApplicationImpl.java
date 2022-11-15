// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Application;
import android.content.Intent;
import android.os.Bundle;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.browser.app.notifications.ContextualNotificationPermissionRequesterImpl;
import org.chromium.chrome.browser.background_task_scheduler.ChromeBackgroundTaskFactory;
import org.chromium.chrome.browser.base.SplitCompatApplication;
import org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.embedder_support.browser_context.PartitionResolverSupplier;
import org.chromium.components.module_installer.util.ModuleUtil;
import org.chromium.components.version_info.Channel;
import org.chromium.components.version_info.VersionConstants;
import org.chromium.url.GURL;

/**
 * Basic application functionality that should be shared among all browser applications that use
 * chrome layer.
 *
 * Note: All application logic should be added to {@link ChromeApplicationImpl}, which will be
 * called from the superclass. See {@link SplitCompatApplication} for more info.
 */
public class ChromeApplicationImpl extends SplitCompatApplication.Impl {

    public ChromeApplicationImpl() {}

    @Override
    public void onCreate() {
        super.onCreate();

        if (SplitCompatApplication.isBrowserProcess()) {

            // Only load the native library early for bundle builds since some tests use the
            // "--disable-native-initialization" switch, and the CommandLine is not initialized at
            // this point to check.
            if (CachedFeatureFlags.isEnabled(ChromeFeatureList.EARLY_LIBRARY_LOAD)
                    && ProductConfig.IS_BUNDLE) {
                // Kick off library loading in a separate thread so it's ready when we need it.
                new Thread(() -> LibraryLoader.getInstance().ensureMainDexInitialized()).start();
            }

            ApplicationStatus.registerStateListenerForAllActivities(
                    ChromePowerModeVoter.getInstance());

            // Initializes the support for dynamic feature modules (browser only).
            ModuleUtil.initApplication();

            if (VersionConstants.CHANNEL == Channel.CANARY) {
                GURL.setReportDebugThrowableCallback(
                        ChromePureJavaExceptionReporter::reportJavaException);
            }

            // Set Chrome factory for mapping BackgroundTask classes to TaskIds.
            ChromeBackgroundTaskFactory.setAsDefault();
            ContextualNotificationPermissionRequesterImpl.initialize();
            PartitionResolverSupplier.setInstance(new ProfileResolver());

            AppHooks.get().getChimeDelegate().initialize();



        }
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        if (isSevereMemorySignal(level)
                && GlobalDiscardableReferencePool.getReferencePool() != null) {
            GlobalDiscardableReferencePool.getReferencePool().drain();
        }
    }

    @Override
    public void startActivity(Intent intent, Bundle options) {
        super.startActivity(intent, options);
    }

    /**
     * Determines whether the given memory signal is considered severe.
     * @param level The type of signal as defined in {@link android.content.ComponentCallbacks2}.
     */
    public static boolean isSevereMemorySignal(int level) {
        // The conditions are expressed using ranges to capture intermediate levels possibly added
        // to the API in the future.
        return (level >= Application.TRIM_MEMORY_RUNNING_LOW
                       && level < Application.TRIM_MEMORY_UI_HIDDEN)
                || level >= Application.TRIM_MEMORY_MODERATE;
    }

}
