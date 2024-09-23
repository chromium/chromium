// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.chrome.browser.browserservices.ui.SharedActivityCoordinator;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.webapps.WebappSplashController;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.metrics.LaunchMetrics;
import org.chromium.chrome.browser.util.AndroidTaskUtils;

import javax.inject.Inject;

/**
 * Coordinator shared between webapp activity and WebAPK activity components.
 * Add methods here if other components need to communicate with either of these components.
 */
@ActivityScope
public class WebappActivityCoordinator
        implements InflationObserver, PauseResumeWithNativeObserver, StartStopWithNativeObserver {
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final WebappInfo mWebappInfo;
    private final Activity mActivity;
    private final WebappDeferredStartupWithStorageHandler mDeferredStartupWithStorageHandler;

    // Whether the current page is within the webapp's scope.

    @Inject
    public WebappActivityCoordinator(
            SharedActivityCoordinator sharedActivityCoordinator,
            Activity activity,
            BrowserServicesIntentDataProvider intentDataProvider,
            ActivityTabProvider activityTabProvider,
            CurrentPageVerifier currentPageVerifier,
            WebappSplashController splashController,
            WebappDeferredStartupWithStorageHandler deferredStartupWithStorageHandler,
            WebappActionsNotificationManager actionsNotificationManager,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        // We don't need to do anything with |sharedActivityCoordinator|, |splashController| or
        // |actionsNotificationManager|. We just need to resolve it so that it starts working.

        mIntentDataProvider = intentDataProvider;
        mWebappInfo = WebappInfo.create(mIntentDataProvider);
        mActivity = activity;
        mDeferredStartupWithStorageHandler = deferredStartupWithStorageHandler;

        mDeferredStartupWithStorageHandler.addTask(
                (storage, didCreateStorage) -> {
                    if (lifecycleDispatcher.isActivityFinishingOrDestroyed()) return;

                    if (storage != null) {
                        updateStorage(storage);
                    }
                });

        lifecycleDispatcher.register(this);

        // Initialize the WebappRegistry and warm up the shared preferences for this web app. No-ops
        // if the registry and this web app are already initialized. Must override Strict Mode to
        // avoid a violation.
        WebappRegistry.getInstance();
        WebappRegistry.warmUpSharedPrefsForId(mWebappInfo.id());
    }

    /** Invoked to add deferred startup tasks to queue. */
    public void initDeferredStartupForActivity() {
        mDeferredStartupWithStorageHandler.initDeferredStartupForActivity();
    }

    @Override
    public void onPreInflationStartup() {
        LaunchMetrics.recordHomeScreenLaunchIntoStandaloneActivity(mWebappInfo);
    }

    @Override
    public void onPostInflationStartup() {}

    @Override
    public void onStartWithNative() {
        WebappDirectoryManager.cleanUpDirectories();
    }

    @Override
    public void onStopWithNative() {}

    @Override
    public void onPauseWithNative() {}

    @Override
    public void onResumeWithNative() {
        if (!mActivity.isFinishing() && mActivity.getIntent() != null) {
            // Avoid situations where Android starts two Activities with the same data.
            // TODO: Determine whether this is still needed now that we use
            // Activity#finishAndRemoveTask() when handling 'back'.
            AndroidTaskUtils.finishOtherTasksWithData(
                    mActivity.getIntent().getData(), mActivity.getTaskId());
        }
    }

    private void updateStorage(@NonNull WebappDataStorage storage) {
        // The information in the WebappDataStorage may have been purged by the
        // user clearing their history or not launching the web app recently.
        // Restore the data if necessary.
        storage.updateFromWebappIntentDataProvider(mIntentDataProvider);

        // A recent last used time is the indicator that the web app is still
        // present on the home screen, and enables sources such as notifications to
        // launch web apps. Thus, we do not update the last used time when the web
        // app is not directly launched from the home screen, as this interferes
        // with the heuristic.
        if (mWebappInfo.isLaunchedFromHomescreen()) {
            // TODO(yusufo): WebappRegistry#unregisterOldWebapps uses this information to delete
            // WebappDataStorage objects for legacy webapps which haven't been used in a while.
            storage.updateLastUsedTime();
        }
    }

    /**
     * @return the {@link WebappInfo} for the WebappActivity.
     */
    public WebappInfo getWebappInfo() {
        return mWebappInfo;
    }
}
