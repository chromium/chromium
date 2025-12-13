// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.metrics.LaunchMetrics;
import org.chromium.chrome.browser.util.AndroidTaskUtils;

/**
 * Coordinator shared between webapp activity and WebAPK activity components. Add methods here if
 * other components need to communicate with either of these components.
 */
@NullMarked
public class WebappActivityCoordinator
        implements InflationObserver, PauseResumeWithNativeObserver, StartStopWithNativeObserver {
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final WebappInfo mWebappInfo;
    private final Activity mActivity;
    private final WebappDeferredStartupWithStorageHandler mDeferredStartupWithStorageHandler;

    public WebappActivityCoordinator(
            BrowserServicesIntentDataProvider intentDataProvider,
            Activity activity,
            WebappDeferredStartupWithStorageHandler webappDeferredStartupWithStorageHandler,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mIntentDataProvider = intentDataProvider;
        mWebappInfo = WebappInfo.create(mIntentDataProvider);
        mActivity = activity;
        mDeferredStartupWithStorageHandler = webappDeferredStartupWithStorageHandler;

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

        LaunchMetrics.recordHomeScreenLaunchIntoStandaloneActivity(mWebappInfo);
    }

    /** Invoked to add deferred startup tasks to queue. */
    public void initDeferredStartupForActivity() {
        mDeferredStartupWithStorageHandler.initDeferredStartupForActivity();
    }

    @Override
    public void onPreInflationStartup() {}

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

    private void updateStorage(WebappDataStorage storage) {
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
